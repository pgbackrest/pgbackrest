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

    unsigned int segmentNo;                                         // Segment number for calculating offsets
    uint64_t lsnLimit;                                              // Lower limit of pages that could be torn
    unsigned int pageSize;                                          // Page size

    bool valid;                                                     // Is the relation structure valid?
    bool align;                                                     // Is the relation alignment valid?
    VariantList *error;                                             // List of checksum errors
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

    (void)this;
    (void)input;

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

    FUNCTION_LOG_RETURN(VARIANT, NULL);
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoFilter *
pageChecksumNew(unsigned int segmentNo, uint64_t lsnLimit, unsigned int pageSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(UINT, segmentNo);
        FUNCTION_LOG_PARAM(UINT64, lsnLimit);
        FUNCTION_LOG_PARAM(UINT, pageSize);
    FUNCTION_LOG_END();

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("PageChecksum")
    {
        PageChecksum *driver = memNew(sizeof(PageChecksum));
        driver->memContext = memContextCurrent();

        driver->segmentNo = segmentNo;
        driver->lsnLimit = lsnLimit;
        driver->pageSize = pageSize;

        driver->valid = true;
        driver->align = true;

        this = ioFilterNewP(PAGE_CHECKSUM_FILTER_TYPE_STR, driver, .in = pageChecksumProcess, .result = pageChecksumResult);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
