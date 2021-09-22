/***********************************************************************************************************************************
Page Checksum Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "command/backup/pageChecksum.h"
#include "common/log.h"
#include "common/macro.h"
#include "common/type/json.h"
#include "common/type/object.h"
#include "postgres/interface.h"
#include "postgres/interface/static.vendor.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct PageChecksum
{
    MemContext *memContext;                                         // Mem context of filter

    unsigned int pageNoOffset;                                      // Page number offset for subsequent segments
    uint64_t lsnLimit;                                              // Lower limit of pages that could be torn

    unsigned char *pageBuffer;                                      // Buffer to hold a page while verifying the checksum

    bool valid;                                                     // Is the relation structure valid?
    bool align;                                                     // Is the relation alignment valid?
    VariantList *error;                                             // List of checksum errors

    unsigned int errorMin;                                          // Current min error page
    unsigned int errorMax;                                          // Current max error page
} PageChecksum;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *
pageChecksumToLog(const PageChecksum *this)
{
    return strNewFmt("{valid: %s, align: %s}", cvtBoolToConstZ(this->valid), cvtBoolToConstZ(this->align));
}

#define FUNCTION_LOG_PAGE_CHECKSUM_TYPE                                                                                            \
    PageChecksum *
#define FUNCTION_LOG_PAGE_CHECKSUM_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, pageChecksumToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Count bytes in the input
***********************************************************************************************************************************/
static void
pageChecksumProcess(THIS_VOID, const Buffer *input)
{
    THIS(PageChecksum);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PAGE_CHECKSUM, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(input != NULL);

    // Calculate total pages in the buffer
    unsigned int pageTotal = (unsigned int)(bufUsed(input) / PG_PAGE_SIZE_DEFAULT);

    // If there is a partial page make sure there is enough of it to validate the checksum
    unsigned int pageRemainder = (unsigned int)(bufUsed(input) % PG_PAGE_SIZE_DEFAULT);

    if (pageRemainder != 0)
    {
        // Misaligned blocks, if any, should only be at the end of the file
        if (!this->align)
            THROW(AssertError, "should not be possible to see two misaligned pages in a row");

        // Mark this buffer as misaligned in case we see another one
        this->align = false;

        // If there at least 512 bytes then we'll treat this as a partial write (modern file systems will have at least 4096)
        if (pageRemainder >= 512)
        {
            pageTotal++;
        }
        // Else this appears to be a corrupted file and we'll stop doing page checksums
        else
            this->valid = false;
    }

    // Verify the checksums of complete pages in the buffer
    if (this->valid)
    {
        for (unsigned int pageIdx = 0; pageIdx < pageTotal; pageIdx++)
        {
            // Get a pointer to the page header
            const PageHeaderData *const pageHeader = (const PageHeaderData *)(bufPtrConst(input) + pageIdx * PG_PAGE_SIZE_DEFAULT);

            // Skip new pages ??? Improved to exactly match what PostgreSQL does in PageIsVerifiedExtended()
            if (pageHeader->pd_upper == 0)
                continue;

            // Get the page lsn
            uint64_t pageLsn = PageXLogRecPtrGet(pageHeader->pd_lsn);

            // Block number relative to all segments in the relation
            unsigned int blockNo = this->pageNoOffset + pageIdx;

            // Skip pages after the backup start LSN since they may be torn
            if (pageLsn >= this->lsnLimit)
                continue;

            // Only validate page checksum if the page is complete
            if (this->align || pageIdx < pageTotal - 1)
            {
                // Make a copy of the page since it will be modified by the page checksum function
                memcpy(this->pageBuffer, pageHeader, PG_PAGE_SIZE_DEFAULT);

                // Continue if the checksum matches
                if (pageHeader->pd_checksum == pgPageChecksum(this->pageBuffer, blockNo))
                    continue;
            }

            // Add the page error
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                // Create the error list if it does not exist yet
                if (this->error == NULL)
                    this->error = varLstNew();

                // Add page number and lsn to the error list
                VariantList *pair = varLstNew();
                varLstAdd(pair, varNewUInt(blockNo));
                varLstAdd(pair, varNewUInt64(pageLsn));
                varLstAdd(this->error, varNewVarLst(pair));
            }
            MEM_CONTEXT_END();
        }

        this->pageNoOffset += pageTotal;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Return filter result
***********************************************************************************************************************************/
static Buffer *
pageChecksumResult(THIS_VOID)
{
    THIS(PageChecksum);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PAGE_CHECKSUM, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        KeyValue *error = kvNew();

        if (this->error != NULL)
        {
            VariantList *errorList = varLstNew();
            unsigned int errorIdx = 0;

            // Convert the full list to an abbreviated list. In the future we want to return the entire list so pages can be verified
            // in the WAL.
            do
            {
                unsigned int pageId = varUInt(varLstGet(varVarLst(varLstGet(this->error, errorIdx)), 0));

                if (errorIdx == varLstSize(this->error) - 1)
                {
                    varLstAdd(errorList, varNewUInt(pageId));
                    errorIdx++;
                }
                else
                {
                    unsigned int pageIdNext = varUInt(varLstGet(varVarLst(varLstGet(this->error, errorIdx + 1)), 0));

                    if (pageIdNext > pageId + 1)
                    {
                        varLstAdd(errorList, varNewUInt(pageId));
                        errorIdx++;
                    }
                    else
                    {
                        unsigned int pageIdLast = pageIdNext;
                        errorIdx++;

                        while (errorIdx < varLstSize(this->error) - 1)
                        {
                            pageIdNext = varUInt(varLstGet(varVarLst(varLstGet(this->error, errorIdx + 1)), 0));

                            if (pageIdNext > pageIdLast + 1)
                                break;

                            pageIdLast = pageIdNext;
                            errorIdx++;
                        }

                        VariantList *errorListSub = varLstNew();
                        varLstAdd(errorListSub, varNewUInt(pageId));
                        varLstAdd(errorListSub, varNewUInt(pageIdLast));
                        varLstAdd(errorList, varNewVarLst(errorListSub));
                        errorIdx++;
                    }
                }
            }
            while (errorIdx < varLstSize(this->error));

            this->valid = false;
            kvPut(error, varNewStrZ("error"), varNewVarLst(errorList));
        }

        kvPut(error, VARSTRDEF("valid"), VARBOOL(this->valid));
        kvPut(error, VARSTRDEF("align"), VARBOOL(this->align));

        result = bufNew(PACK_EXTRA_MIN);
        PackWrite *const write = pckWriteNewBuf(result);

        pckWriteStrP(write, jsonFromKv(error));
        pckWriteEndP(write);

        bufMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BUFFER, result);
}

/**********************************************************************************************************************************/
IoFilter *
pageChecksumNew(unsigned int segmentNo, unsigned int segmentPageTotal, uint64_t lsnLimit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(UINT, segmentNo);
        FUNCTION_LOG_PARAM(UINT, segmentPageTotal);
        FUNCTION_LOG_PARAM(UINT64, lsnLimit);
    FUNCTION_LOG_END();

    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(PageChecksum)
    {
        PageChecksum *driver = OBJ_NEW_ALLOC();

        *driver = (PageChecksum)
        {
            .memContext = memContextCurrent(),
            .pageNoOffset = segmentNo * segmentPageTotal,
            .lsnLimit = lsnLimit,
            .pageBuffer = memNew(PG_PAGE_SIZE_DEFAULT),
            .valid = true,
            .align = true,
        };

        // Create param list
        Buffer *const paramList = bufNew(PACK_EXTRA_MIN);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            PackWrite *const packWrite = pckWriteNewBuf(paramList);

            pckWriteU32P(packWrite, segmentNo);
            pckWriteU32P(packWrite, segmentPageTotal);
            pckWriteU64P(packWrite, lsnLimit);
            pckWriteEndP(packWrite);
        }
        MEM_CONTEXT_TEMP_END();

        this = ioFilterNewP(
            PAGE_CHECKSUM_FILTER_TYPE, driver, paramList, .in = pageChecksumProcess, .result = pageChecksumResult);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

IoFilter *
pageChecksumNewPack(const Buffer *const paramList)
{
    IoFilter *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const paramListPack = pckReadNewBuf(paramList);
        const unsigned int segmentNo = pckReadU32P(paramListPack);
        const unsigned int segmentPageTotal = pckReadU32P(paramListPack);
        const uint64_t lsnLimit = pckReadU64P(paramListPack);

        result = objMoveContext(pageChecksumNew(segmentNo, segmentPageTotal, lsnLimit), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
