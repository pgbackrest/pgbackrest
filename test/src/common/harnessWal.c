#include "build.auto.h"

#include "common/type/object.h"
#include "common/walFilter/versions/recordProcessGPDB6.h"
#include "harnessWal.h"
#include "string.h"

#define TRANSACTION_ID_PLACEHOLDER 0xADDE
#define PREV_RECPTR_PLACEHOLDER 0xAABB
#define RECORD_BODY_PLACEHOLDER 0XAB

XLogRecord *
hrnGpdbCreateXRecord(uint8_t rmid, uint8_t info, uint32_t bodySize, void *body, CreateXRecordParam param)
{
    if (param.heapPageSize == 0)
        param.heapPageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;

    if (param.xl_len == 0)
        param.xl_len = bodySize;

    XLogRecord *record = memNew(SizeOfXLogRecord + bodySize);
    *record = (XLogRecord){
        .xl_tot_len = (uint32_t) (SizeOfXLogRecord + bodySize),
        .xl_xid = TRANSACTION_ID_PLACEHOLDER,
        .xl_len = param.xl_len,
        .xl_info = info,
        .xl_rmid = (uint8_t) rmid,
        .xl_prev = PREV_RECPTR_PLACEHOLDER
    };

    size_t alignSize = MAXALIGN(sizeof(XLogRecord)) - sizeof(XLogRecord);
    memset((char *) record + sizeof(XLogRecord), 0, alignSize);

    if (body == NULL)
        memset((void *) XLogRecGetData(record), RECORD_BODY_PLACEHOLDER, bodySize);
    else
        memcpy((void *) XLogRecGetData(record), body, bodySize);

    record->xl_crc = xLogRecordChecksumGPDB6(record, param.heapPageSize);

    return record;
}

void
hrnGpdbWalInsertXRecord(
    Buffer *const walBuffer,
    XLogRecord *record,
    InsertXRecordParam param,
    InsertRecordFlags flags)
{
    if (param.magic == 0)
        param.magic = GPDB6_XLOG_PAGE_HEADER_MAGIC;

    if (param.walPageSize == 0)
        param.walPageSize = DEFAULT_GDPB_XLOG_PAGE_SIZE;

    if (bufUsed(walBuffer) == 0)
    {
        // This is first record
        // Workaround gcc bug 53119
        XLogLongPageHeaderData longHeader = {{0}};
        longHeader.std.xlp_magic = param.magic;
        longHeader.std.xlp_info = XLP_LONG_HEADER;
        longHeader.std.xlp_tli = 1;
        longHeader.std.xlp_pageaddr = param.segno * GPDB6_XLOG_SEG_SIZE;
        longHeader.std.xlp_rem_len = param.beginOffset;
        longHeader.xlp_sysid = 10000000000000090400ULL;
        longHeader.xlp_seg_size = GPDB6_XLOG_SEG_SIZE;
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
        header.xlp_pageaddr = bufUsed(walBuffer);
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
                return;
            if (wrote == totalLen)
                break;

            ASSERT(bufUsed(walBuffer) % param.walPageSize == 0);
            // We should be on the beginning of the page. so write header
            XLogPageHeaderData header = {0};
            header.xlp_magic = param.magic;
            header.xlp_info = !(flags & NO_COND_FLAG) ? XLP_FIRST_IS_CONTRECORD : 0;
            header.xlp_tli = 1;
            header.xlp_pageaddr = param.segno * GPDB6_XLOG_SEG_SIZE + bufUsed(walBuffer);

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

void
hrnGpdbWalInsertXRecordSimple(Buffer *const walBuffer, XLogRecord *record)
{
    hrnGpdbWalInsertXRecordP(walBuffer, record, NO_FLAGS);
}
