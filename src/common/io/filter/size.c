/***********************************************************************************************************************************
IO Size Filter
***********************************************************************************************************************************/
#include <stdio.h>

#include "common/debug.h"
#include "common/io/filter/size.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define SIZE_FILTER_TYPE                                            "size"

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
ioSizeNew()
{
    FUNCTION_DEBUG_VOID(logLevelTrace);

    IoSize *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoSize")
    {
        this = memNew(sizeof(IoSize));
        this->memContext = memContextCurrent();

        this->filter = ioFilterNew(
            strNew(SIZE_FILTER_TYPE), this, NULL, NULL, (IoFilterProcessIn)ioSizeProcess, NULL, (IoFilterResult)ioSizeResult);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(IO_SIZE, this);
}

/***********************************************************************************************************************************
Count bytes in the input
***********************************************************************************************************************************/
void
ioSizeProcess(IoSize *this, const Buffer *input)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_SIZE, this);
        FUNCTION_DEBUG_PARAM(BUFFER, input);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(input != NULL);
    FUNCTION_DEBUG_END();

    this->size += bufUsed(input);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Get filter interface
***********************************************************************************************************************************/
IoFilter *
ioSizeFilter(const IoSize *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SIZE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_FILTER, this->filter);
}

/***********************************************************************************************************************************
Convert to a zero-terminated string for logging
***********************************************************************************************************************************/
size_t
ioSizeToLog(const IoSize *this, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *string = NULL;

        if (this == NULL)
            string = strNew("null");
        else
            string = strNewFmt("{size: %" PRIu64 "}", this->size);

        result = (size_t)snprintf(buffer, bufferSize, "%s", strPtr(string));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Return filter result
***********************************************************************************************************************************/
const Variant *
ioSizeResult(IoSize *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_SIZE, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    Variant *result = NULL;

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        result = varNewUInt64(this->size);
    }
    MEM_CONTEXT_END();

    FUNCTION_DEBUG_RESULT(VARIANT, result);
}

/***********************************************************************************************************************************
Free the filter group
***********************************************************************************************************************************/
void
ioSizeFree(IoSize *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_SIZE, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
