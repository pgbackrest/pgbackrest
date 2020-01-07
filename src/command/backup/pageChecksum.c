/***********************************************************************************************************************************
Page Checksum Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "command/backup/pageChecksum.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "postgres/pageChecksum.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(PAGE_CHECKSUM_FILTER_TYPE_STR,                        PAGE_CHECKSUM_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct PageChecksum
{
    MemContext *memContext;                                         // Mem context of filter

    unsigned int pageNoOffset;                                      // Page number offset for subsequent segments
    size_t pageSize;                                                // Page size
    uint64_t lsnLimit;                                              // Lower limit of pages that could be torn

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
    unsigned int pageTotal = (unsigned int)(bufUsed(input) / this->pageSize);

    // If there is a partial page make sure there is enough of it to validate the checksum
    unsigned int pageRemainder = (unsigned int)(bufUsed(input) % this->pageSize);

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
            unsigned char *pagePtr = bufPtr(input) + (pageIdx * this->pageSize);
            unsigned int pageNo = this->pageNoOffset + pageIdx;
            size_t pageSize = this->align || pageIdx < pageTotal - 1 ? this->pageSize : pageRemainder;

            if (!pageChecksumTest(
                    pagePtr, pageNo, (unsigned int)pageSize, (unsigned int)(this->lsnLimit >> 32),
                    (unsigned int)(this->lsnLimit & 0xFFFFFFFF)))
            {
                MEM_CONTEXT_BEGIN(this->memContext)
                {
                    // Create the error list if it does not exist yet
                    if (this->error == NULL)
                        this->error = varLstNew();

                    // Add page number and lsn to the error list
                    VariantList *pair = varLstNew();
                    varLstAdd(pair, varNewUInt(pageNo));
                    varLstAdd(pair, varNewUInt64(pageLsn(pagePtr)));
                    varLstAdd(this->error, varNewVarLst(pair));
                }
                MEM_CONTEXT_END();
            }
        }

        this->pageNoOffset += pageTotal;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Return filter result
***********************************************************************************************************************************/
static Variant *
pageChecksumResult(THIS_VOID)
{
    THIS(PageChecksum);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PAGE_CHECKSUM, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    KeyValue *result = kvNew();

    if (this->error != NULL)
    {
        VariantList *errorList = varLstNew();
        unsigned int errorIdx = 0;

        // Convert the full list to an abbreviated list.  In the future we want to return the entire list so pages can be verified
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
        kvPut(result, varNewStrZ("error"), varNewVarLst(errorList));
    }

    kvPut(result, VARSTRDEF("valid"), VARBOOL(this->valid));
    kvPut(result, VARSTRDEF("align"), VARBOOL(this->align));

    FUNCTION_LOG_RETURN(VARIANT, varNewKv(result));
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoFilter *
pageChecksumNew(unsigned int segmentNo, unsigned int segmentPageTotal, size_t pageSize, uint64_t lsnLimit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(UINT, segmentNo);
        FUNCTION_LOG_PARAM(UINT, segmentPageTotal);
        FUNCTION_LOG_PARAM(SIZE, pageSize);
        FUNCTION_LOG_PARAM(UINT64, lsnLimit);
    FUNCTION_LOG_END();

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("PageChecksum")
    {
        PageChecksum *driver = memNew(sizeof(PageChecksum));
        driver->memContext = memContextCurrent();

        driver->pageNoOffset = segmentNo * segmentPageTotal;
        driver->pageSize = pageSize;
        driver->lsnLimit = lsnLimit;

        driver->valid = true;
        driver->align = true;

        // Create param list
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewUInt(segmentNo));
        varLstAdd(paramList, varNewUInt(segmentPageTotal));
        varLstAdd(paramList, varNewUInt64(pageSize));
        varLstAdd(paramList, varNewUInt64(lsnLimit));

        this = ioFilterNewP(
            PAGE_CHECKSUM_FILTER_TYPE_STR, driver, paramList, .in = pageChecksumProcess, .result = pageChecksumResult);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

IoFilter *
pageChecksumNewVar(const VariantList *paramList)
{
    return pageChecksumNew(
        varUIntForce(varLstGet(paramList, 0)), varUIntForce(varLstGet(paramList, 1)), varUIntForce(varLstGet(paramList, 2)),
        varUInt64(varLstGet(paramList, 3)));
}
