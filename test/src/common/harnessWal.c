#include "build.auto.h"

#include "common/walFilter/postgresCommon.h"
#include "config/config.h"
#include "harnessDebug.h"
#include "harnessWal.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Interface definition
***********************************************************************************************************************************/
XLogRecordBase *hrnGpdbCreateXRecord94GPDB(uint8 rmid, uint8 info, CreateXRecordParam param);
XLogRecordBase *hrnGpdbCreateXRecord12GPDB(uint8 rmid, uint8 info, CreateXRecordParam param);

typedef struct HrnWalInterface
{
    // Version of PostgreSQL supported by this interface
    unsigned int version;

    StringId fork;

    XLogRecordBase *(*hrnGpdbCreateXRecord)(uint8 rmid, uint8 info, CreateXRecordParam param);
} HrnWalInterface;

static const HrnWalInterface hrnWalInterfaces[] = {
    {
        PG_VERSION_94,
        CFGOPTVAL_FORK_GPDB,
        hrnGpdbCreateXRecord94GPDB
    },
    {
        PG_VERSION_12,
        CFGOPTVAL_FORK_GPDB,
        hrnGpdbCreateXRecord12GPDB
    }
};

static const HrnWalInterface *
hrnWalInterfaceVersion(unsigned int pgVersion)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, pgVersion);
    FUNCTION_HARNESS_END();

    const HrnWalInterface *result = NULL;
    const StringId fork = cfgOptionStrId(cfgOptFork);

    for (unsigned int interfaceIdx = 0; interfaceIdx < LENGTH_OF(hrnWalInterfaces); interfaceIdx++)
    {
        if (hrnWalInterfaces[interfaceIdx].version == pgVersion && hrnWalInterfaces[interfaceIdx].fork == fork)
        {
            result = &hrnWalInterfaces[interfaceIdx];
            break;
        }
    }

    // If the version was not found then error
    if (result == NULL)
        THROW_FMT(AssertError, "invalid " PG_NAME " version %u", pgVersion);

    FUNCTION_HARNESS_RETURN(STRUCT, result);
}

XLogRecordBase *
hrnGpdbCreateXRecord(unsigned int pgVersion, uint8 rmid, uint8 info, CreateXRecordParam param)
{
    if (param.heapPageSize == 0)
        param.heapPageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;
    return hrnWalInterfaceVersion(pgVersion)->hrnGpdbCreateXRecord(rmid, info, param);
}

void
hrnGpdbWalInsertXRecord(
    Buffer *const walBuffer,
    XLogRecordBase *record,
    InsertXRecordParam param,
    InsertRecordFlags flags)
{
    if (param.magic == 0)
        param.magic = GPDB6_XLOG_PAGE_HEADER_MAGIC;

    if (param.walPageSize == 0)
        param.walPageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;
    if (param.segSize == 0)
        param.segSize = GPDB_XLOG_SEG_SIZE;

    if (bufUsed(walBuffer) == 0)
    {
        // This is first record
        // Workaround gcc bug 53119
        XLogLongPageHeaderData longHeader = {{0}};
        longHeader.std.xlp_magic = param.magic;
        longHeader.std.xlp_info = XLP_LONG_HEADER;
        longHeader.std.xlp_tli = 1;
        longHeader.std.xlp_pageaddr = param.segno * param.segSize;
        longHeader.std.xlp_rem_len = param.beginOffset;
        longHeader.xlp_sysid = 10000000000000090400ULL;
        longHeader.xlp_seg_size = param.segSize;
        longHeader.xlp_xlog_blcksz = param.walPageSize;

        if (flags & OVERWRITE)
            longHeader.std.xlp_info |= XLP_FIRST_IS_OVERWRITE_CONTRECORD;
        if (param.beginOffset)
            longHeader.std.xlp_info |= XLP_FIRST_IS_CONTRECORD;

        *((XLogLongPageHeaderData *) bufRemainsPtr(walBuffer)) = longHeader;

        bufUsedInc(walBuffer, sizeof(longHeader));
        size_t alignSize = MAXALIGN(sizeof(longHeader)) - sizeof(longHeader);
        memset(bufRemainsPtr(walBuffer), 0, alignSize);
        bufUsedInc(walBuffer, alignSize);
    }

    if (bufUsed(walBuffer) % param.walPageSize == 0)
    {
        XLogPageHeaderData header = {0};
        header.xlp_magic = param.magic;
        header.xlp_tli = 1;
        header.xlp_pageaddr = param.segno * param.segSize + bufUsed(walBuffer);
        header.xlp_rem_len = 0;

        if (flags & COND_FLAG)
            header.xlp_info |= XLP_FIRST_IS_CONTRECORD;
        else if (flags & OVERWRITE)
            header.xlp_info = XLP_FIRST_IS_OVERWRITE_CONTRECORD;
        else
            header.xlp_info = 0;

        *((XLogPageHeaderData *) bufRemainsPtr(walBuffer)) = header;
        bufUsedInc(walBuffer, sizeof(header));
        memset(bufRemainsPtr(walBuffer), 0, XLogPageHeaderSize(&header) - sizeof(header));
        bufUsedInc(walBuffer, XLogPageHeaderSize(&header) - sizeof(header));
    }

    size_t spaceOnPage = param.walPageSize - bufUsed(walBuffer) % param.walPageSize;

    size_t totalLen;
    unsigned char *recordPtr;

    if (param.beginOffset == 0 || flags & OVERWRITE)
    {
        totalLen = record->xl_tot_len;
        recordPtr = (unsigned char *) record;
    }
    else
    {
        totalLen = param.beginOffset;
        recordPtr = ((unsigned char *) record) + (record->xl_tot_len - param.beginOffset);
    }

    if (spaceOnPage < totalLen)
    {
        // We need to split record into two or more pages
        size_t wrote = 0;
        while (wrote != totalLen)
        {
            spaceOnPage = param.walPageSize - bufUsed(walBuffer) % param.walPageSize;
            size_t toWrite = Min(spaceOnPage, totalLen - wrote);
            memcpy(bufRemainsPtr(walBuffer), recordPtr + wrote, toWrite);
            wrote += toWrite;

            bufUsedInc(walBuffer, toWrite);
            if (flags & INCOMPLETE_RECORD)
            {
                if (param.incompletePosition == 0)
                    return;
                else
                    param.incompletePosition--;
            }
            if (wrote == totalLen)
                break;

            ASSERT(bufUsed(walBuffer) % param.walPageSize == 0);
            // We should be on the beginning of the page. so write header
            XLogPageHeaderData header = {0};
            header.xlp_magic = param.magic;
            header.xlp_info = !(flags & NO_COND_FLAG) ? XLP_FIRST_IS_CONTRECORD : 0;
            header.xlp_tli = 1;
            header.xlp_pageaddr = param.segno * param.segSize + bufUsed(walBuffer);

            if (flags & ZERO_REM_LEN)
                header.xlp_rem_len = 0;
            else if (flags & WRONG_REM_LEN)
                header.xlp_rem_len = 1;
            else
                header.xlp_rem_len = (uint32_t) (totalLen - wrote);

            *((XLogPageHeaderData *) bufRemainsPtr(walBuffer)) = header;

            bufUsedInc(walBuffer, sizeof(header));

            size_t alignSize = MAXALIGN(sizeof(header)) - sizeof(header);
            memset(bufRemainsPtr(walBuffer), 0, alignSize);
            bufUsedInc(walBuffer, alignSize);
        }
    }
    else
    {
        // Record should fit into current page
        memcpy(bufRemainsPtr(walBuffer), recordPtr, totalLen);
        bufUsedInc(walBuffer, totalLen);
    }
    size_t alignSize = MAXALIGN(totalLen) - (totalLen);
    memset(bufRemainsPtr(walBuffer), 0, alignSize);
    bufUsedInc(walBuffer, alignSize);
}
