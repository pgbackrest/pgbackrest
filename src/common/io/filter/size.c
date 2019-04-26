/***********************************************************************************************************************************
IO Size Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>

#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/io/filter/size.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define SIZE_FILTER_TYPE                                            "size"
    STRING_STATIC(SIZE_FILTER_TYPE_STR,                             SIZE_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoSize
{
    MemContext *memContext;                                         // Mem context of filter
    IoFilter *filter;                                               // Filter interface

    uint64_t size;                                                  // Total size of al input
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoSize *
ioSizeNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoSize *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoSize")
    {
        this = memNew(sizeof(IoSize));
        this->memContext = memContextCurrent();

        // Create filter interface
        this->filter = ioFilterNewP(
            SIZE_FILTER_TYPE_STR, this, .in = (IoFilterInterfaceProcessIn)ioSizeProcess,
            .result = (IoFilterInterfaceResult)ioSizeResult);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_SIZE, this);
}

/***********************************************************************************************************************************
Count bytes in the input
***********************************************************************************************************************************/
void
ioSizeProcess(IoSize *this, const Buffer *input)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SIZE, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(input != NULL);

    this->size += bufUsed(input);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get filter interface
***********************************************************************************************************************************/
IoFilter *
ioSizeFilter(const IoSize *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SIZE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->filter);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
ioSizeToLog(const IoSize *this)
{
    return strNewFmt("{size: %" PRIu64 "}", this->size);
}

/***********************************************************************************************************************************
Return filter result
***********************************************************************************************************************************/
const Variant *
ioSizeResult(IoSize *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SIZE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    Variant *result = NULL;

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        result = varNewUInt64(this->size);
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN(VARIANT, result);
}

/***********************************************************************************************************************************
Free the filter group
***********************************************************************************************************************************/
void
ioSizeFree(IoSize *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SIZE, this);
    FUNCTION_LOG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_LOG_RETURN_VOID();
}
