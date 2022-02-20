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
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct PageChecksum
{
    MemContext *memContext;                                         // Mem context of filter

    unsigned int segmentPageTotal;                                  // Total pages in a segment
    unsigned int pageNoOffset;                                      // Page number offset for subsequent segments
    const String *fileName;                                         // Used to load the file to retry pages

    unsigned char *pageBuffer;                                      // Buffer to hold a page while verifying the checksum

    bool valid;                                                     // Is the relation structure valid?
    bool align;                                                     // Is the relation alignment valid?
    PackWrite *error;                                               // List of checksum errors
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

            // Only validate page checksum if the page is complete
            if (this->align || pageIdx < pageTotal - 1)
            {
                // Make a copy of the page since it will be modified by the page checksum function
                memcpy(this->pageBuffer, pageHeader, PG_PAGE_SIZE_DEFAULT);

                // Continue if the checksum matches
                if (pageHeader->pd_checksum == pgPageChecksum(this->pageBuffer, blockNo))
                    continue;

                // On checksum mismatch retry the page
                bool changed = false;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    // Reload the page
                    const Buffer *const pageRetry = storageGetP(
                        storageNewReadP(
                            storagePosixNewP(FSLASH_STR), this->fileName,
                            .offset = (blockNo % this->segmentPageTotal) * PG_PAGE_SIZE_DEFAULT,
                            .limit = VARUINT64(PG_PAGE_SIZE_DEFAULT)));

                    // Check if the page has changed since it was last read
                    changed = !bufEq(pageRetry, BUF(this->pageBuffer, PG_PAGE_SIZE_DEFAULT));
                }
                MEM_CONTEXT_TEMP_END();

                // If the page has changed then PostgreSQL must be updating it so we won't consider it to be in error
                if (changed)
                    continue;
            }

            // Create the error list if it does not exist yet
            if (this->error == NULL)
            {
                MEM_CONTEXT_BEGIN(this->memContext)
                {
                    this->error = pckWriteNewP();
                    pckWriteArrayBeginP(this->error);
                }
                MEM_CONTEXT_END();
            }

            // Add page number and lsn to the error list
            pckWriteObjBeginP(this->error, .id = blockNo + 1);
            pckWriteU64P(this->error, pageLsn);
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

    Pack *result = NULL;

    MEM_CONTEXT_BEGIN(this->memContext)
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
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
IoFilter *
pageChecksumNew(const unsigned int segmentNo, const unsigned int segmentPageTotal, const String *const fileName)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(UINT, segmentNo);
        FUNCTION_LOG_PARAM(UINT, segmentPageTotal);
        FUNCTION_LOG_PARAM(STRING, fileName);
    FUNCTION_LOG_END();

    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(PageChecksum)
    {
        PageChecksum *driver = OBJ_NEW_ALLOC();

        *driver = (PageChecksum)
        {
            .memContext = memContextCurrent(),
            .segmentPageTotal = segmentPageTotal,
            .pageNoOffset = segmentNo * segmentPageTotal,
            .fileName = strDup(fileName),
            .pageBuffer = memNew(PG_PAGE_SIZE_DEFAULT),
            .valid = true,
            .align = true,
        };

        // Create param list
        Pack *paramList = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            PackWrite *const packWrite = pckWriteNewP();

            pckWriteU32P(packWrite, segmentNo);
            pckWriteU32P(packWrite, segmentPageTotal);
            pckWriteStrP(packWrite, fileName);
            pckWriteEndP(packWrite);

            paramList = pckMove(pckWriteResult(packWrite), memContextPrior());
        }
        MEM_CONTEXT_TEMP_END();

        this = ioFilterNewP(PAGE_CHECKSUM_FILTER_TYPE, driver, paramList, .in = pageChecksumProcess, .result = pageChecksumResult);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

IoFilter *
pageChecksumNewPack(const Pack *const paramList)
{
    IoFilter *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const paramListPack = pckReadNew(paramList);
        const unsigned int segmentNo = pckReadU32P(paramListPack);
        const unsigned int segmentPageTotal = pckReadU32P(paramListPack);
        const String *const fileName = pckReadStrP(paramListPack);

        result = ioFilterMove(pageChecksumNew(segmentNo, segmentPageTotal, fileName), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
