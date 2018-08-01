/***********************************************************************************************************************************
IO Buffer Filter
***********************************************************************************************************************************/
#include <stdio.h>

#include "common/debug.h"
#include "common/io/filter/buffer.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define BUFFER_FILTER_TYPE                                          "buffer"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoBuffer
{
    MemContext *memContext;                                         // Mem context of filter
    IoFilter *filter;                                               // Filter interface

    size_t inputPos;                                                // Position in input buffer
    bool inputSame;                                                 // Is the same input required again?
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoBuffer *
ioBufferNew()
{
    FUNCTION_DEBUG_VOID(logLevelTrace);

    IoBuffer *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoBuffer")
    {
        this = memNew(sizeof(IoBuffer));
        this->memContext = memContextCurrent();

        this->filter = ioFilterNew(
            strNew(BUFFER_FILTER_TYPE), this, NULL, (IoFilterInputSame)ioBufferInputSame, NULL,
            (IoFilterProcessInOut)ioBufferProcess, NULL);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(IO_BUFFER, this);
}

/***********************************************************************************************************************************
Move data from the input buffer to the output buffer
***********************************************************************************************************************************/
void
ioBufferProcess(IoBuffer *this, const Buffer *input, Buffer *output)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_BUFFER, this);
        FUNCTION_DEBUG_PARAM(BUFFER, input);
        FUNCTION_DEBUG_PARAM(BUFFER, output);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(input != NULL);
        FUNCTION_DEBUG_ASSERT(output != NULL);
    FUNCTION_DEBUG_END();

    // Determine how much data needs to be copied and reduce if there is not enough space in the output
    size_t copySize = bufUsed(input) - this->inputPos;

    if (copySize > bufRemains(output))
        copySize = bufRemains(output);

    // Copy data to the output buffer
    bufCatSub(output, input, this->inputPos, copySize);

    // If all data was copied the reset inputPos and allow new input
    if (this->inputPos + copySize == bufUsed(input))
    {
        this->inputSame = false;
        this->inputPos = 0;
    }

    // Else update inputPos and indicate that the same input should be passed again
    else
    {
        this->inputSame = true;
        this->inputPos += copySize;
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Is the same input required again?

If the remaining space in the output buffer is smaller than the used space in the input buffer then the same input must be provided
again.
***********************************************************************************************************************************/
bool
ioBufferInputSame(const IoBuffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->inputSame);
}

/***********************************************************************************************************************************
Get filter interface
***********************************************************************************************************************************/
IoFilter *
ioBufferFilter(const IoBuffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_FILTER, this->filter);
}

/***********************************************************************************************************************************
Convert to a zero-terminated string for logging
***********************************************************************************************************************************/
size_t
ioBufferToLog(const IoBuffer *this, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *string = NULL;

        if (this == NULL)
            string = strNew("null");
        else
            string = strNewFmt("{inputSame: %s, inputPos: %zu}", cvtBoolToConstZ(this->inputSame), this->inputPos);

        result = (size_t)snprintf(buffer, bufferSize, "%s", strPtr(string));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Free the filter group
***********************************************************************************************************************************/
void
ioBufferFree(IoBuffer *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_BUFFER, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
