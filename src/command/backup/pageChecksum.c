/***********************************************************************************************************************************
Page Checksum Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/pageChecksum.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/macro.h"
#include "common/type/json.h"
#include "common/type/object.h"
#include "postgres/interface/static.vendor.h"
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct PageChecksum
{
    unsigned int segmentPageTotal;                                  // Total pages in a segment
    PgPageSize pageSize;                                            // Page size
    unsigned int pageNoOffset;                                      // Page number offset for subsequent segments
    bool headerCheck;                                               // Perform additional header checks?
    const String *fileName;                                         // Used to load the file to retry pages

    unsigned char *pageBuffer;                                      // Buffer to hold a page while verifying the checksum

    bool valid;                                                     // Is the relation structure valid?
    bool align;                                                     // Is the relation alignment valid?
    PackWrite *error;                                               // List of checksum errors
} PageChecksum;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static void
pageChecksumToLog(const PageChecksum *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{valid: %s, align: %s}", cvtBoolToConstZ(this->valid), cvtBoolToConstZ(this->align));
}

#define FUNCTION_LOG_PAGE_CHECKSUM_TYPE                                                                                            \
    PageChecksum *
#define FUNCTION_LOG_PAGE_CHECKSUM_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_OBJECT_FORMAT(value, pageChecksumToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Verify page checksums
***********************************************************************************************************************************/
static void
pageChecksumProcess(THIS_VOID, const Buffer *const input)
{
    THIS(PageChecksum);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PAGE_CHECKSUM, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(input != NULL);

    // Calculate total pages in the buffer
    unsigned int pageTotal = (unsigned int)(bufUsed(input) / this->pageSize);

    // If there is a partial page make sure there is enough of it to validate the checksum
    const unsigned int pageRemainder = (unsigned int)(bufUsed(input) % this->pageSize);

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
            const PageHeaderData *const pageHeader = (const PageHeaderData *)(bufPtrConst(input) + pageIdx * this->pageSize);

            // Block number relative to all segments in the relation
            const unsigned int blockNo = this->pageNoOffset + pageIdx;

            // Only validate page checksum if the page is complete
            if (this->align || pageIdx < pageTotal - 1)
            {
                bool pageValid = true;

                // Skip new all-zero pages. To speed this up first check pd_upper when header check is enabled or pd_checksum when
                // header check is disabled. The latter is required when the page is encrypted.
                if ((this->headerCheck && pageHeader->pd_upper == 0) || (!this->headerCheck && pageHeader->pd_checksum == 0))
                {
                    // Check that the entire page is zero
                    for (unsigned int pageIdx = 0; pageIdx < this->pageSize / sizeof(size_t); pageIdx++)
                    {
                        if (((const size_t *)pageHeader)[pageIdx] != 0)
                        {
                            pageValid = false;
                            break;
                        }
                    }

                    // If the entire page is zero it is valid
                    if (pageValid)
                        continue;
                }

                // Only validate the checksum if the page is valid
                if (pageValid)
                {
                    // Make a copy of the page since it will be modified by the page checksum function
                    memcpy(this->pageBuffer, pageHeader, this->pageSize);

                    // Continue if the checksum matches
                    if (pageHeader->pd_checksum == pgPageChecksum(this->pageBuffer, blockNo, this->pageSize))
                        continue;
                }

                // On error retry the page
                bool changed = false;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    // Reload the page
                    const Buffer *const pageRetry = storageGetP(
                        storageNewReadP(
                            storagePosixNewP(FSLASH_STR), this->fileName,
                            .offset = (uint64_t)(blockNo % this->segmentPageTotal) * this->pageSize,
                            .limit = VARUINT64(this->pageSize)));

                    // Check if the page has changed since it was last read
                    changed = !bufEq(pageRetry, BUF(pageHeader, this->pageSize));
                }
                MEM_CONTEXT_TEMP_END();

                // If the page has changed then PostgreSQL must be updating it so we won't consider it to be invalid. We are not
                // concerned about whether this new page has a valid checksum or not, since the page will be replayed from WAL on
                // recovery. This prevents (theoretically) limitless retries for a hot page.
                if (changed)
                    continue;
            }

            // Create the error list if it does not exist yet
            if (this->error == NULL)
            {
                MEM_CONTEXT_OBJ_BEGIN(this)
                {
                    this->error = pckWriteNewP();
                    pckWriteArrayBeginP(this->error);
                }
                MEM_CONTEXT_OBJ_END();
            }

            // Add page number and lsn to the error list
            pckWriteObjBeginP(this->error, .id = blockNo + 1);
            pckWriteObjEndP(this->error);
        }

        this->pageNoOffset += pageTotal;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Return filter result
***********************************************************************************************************************************/
static Pack *
pageChecksumResult(THIS_VOID)
{
    THIS(PageChecksum);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PAGE_CHECKSUM, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    Pack *result;

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        // End the error array
        if (this->error != NULL)
        {
            pckWriteArrayEndP(this->error);
            this->valid = false;
        }
        // Else create a pack to hold the flags
        else
        {
            this->error = pckWriteNewP();
            pckWriteNullP(this->error);
        }

        // Valid and align flags
        pckWriteBoolP(this->error, this->valid, .defaultWrite = true);
        pckWriteBoolP(this->error, this->align, .defaultWrite = true);

        // End pack
        pckWriteEndP(this->error);

        result = pckMove(pckWriteResult(this->error), memContextPrior());
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
pageChecksumNew(
    const unsigned int segmentNo, const unsigned int segmentPageTotal, const PgPageSize pageSize, const bool headerCheck,
    const String *const fileName)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(UINT, segmentNo);
        FUNCTION_LOG_PARAM(UINT, segmentPageTotal);
        FUNCTION_LOG_PARAM(ENUM, pageSize);
        FUNCTION_LOG_PARAM(BOOL, headerCheck);
        FUNCTION_LOG_PARAM(STRING, fileName);
    FUNCTION_LOG_END();

    ASSERT(pgPageSizeValid(pageSize));
    ASSERT(segmentPageTotal > 0 && segmentPageTotal % pageSize == 0);

    OBJ_NEW_BEGIN(PageChecksum, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (PageChecksum)
        {
            .segmentPageTotal = segmentPageTotal,
            .pageSize = pageSize,
            .pageNoOffset = segmentNo * segmentPageTotal,
            .headerCheck = headerCheck,
            .fileName = strDup(fileName),
            .pageBuffer = bufPtr(bufNew(pageSize)),
            .valid = true,
            .align = true,
        };
    }
    OBJ_NEW_END();

    // Create param list
    Pack *paramList;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteU32P(packWrite, segmentNo);
        pckWriteU32P(packWrite, segmentPageTotal);
        pckWriteU32P(packWrite, pageSize);
        pckWriteBoolP(packWrite, headerCheck);
        pckWriteStrP(packWrite, fileName);
        pckWriteEndP(packWrite);

        paramList = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER,
        ioFilterNewP(PAGE_CHECKSUM_FILTER_TYPE, this, paramList, .in = pageChecksumProcess, .result = pageChecksumResult));
}

FN_EXTERN IoFilter *
pageChecksumNewPack(const Pack *const paramList)
{
    IoFilter *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const paramListPack = pckReadNew(paramList);
        const unsigned int segmentNo = pckReadU32P(paramListPack);
        const unsigned int segmentPageTotal = pckReadU32P(paramListPack);
        const PgPageSize pageSize = (PgPageSize)pckReadU32P(paramListPack);
        const bool headerCheck = pckReadBoolP(paramListPack);
        const String *const fileName = pckReadStrP(paramListPack);

        result = ioFilterMove(pageChecksumNew(segmentNo, segmentPageTotal, pageSize, headerCheck, fileName), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
