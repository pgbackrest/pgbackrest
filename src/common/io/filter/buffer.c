/***********************************************************************************************************************************
IO Buffer Filter
***********************************************************************************************************************************/
#include <stdio.h>

#include "common/debug.h"
#include "common/io/filter/buffer.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define BUFFER_FILTER_TYPE                                          "buffer"
    STRING_STATIC(BUFFER_FILTER_TYPE_STR,                           BUFFER_FILTER_TYPE);

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
ioBufferNew(void)
{
    FUNCTION_DEBUG_VOID(logLevelTrace);

    IoBuffer *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoBuffer")
    {
        this = memNew(sizeof(IoBuffer));
        this->memContext = memContextCurrent();

        // Create filter interface
        this->filter = ioFilterNewP(
            BUFFER_FILTER_TYPE_STR, this, .inOut = (IoFilterInterfaceProcessInOut)ioBufferProcess,
            .inputSame = (IoFilterInterfaceInputSame)ioBufferInputSame);
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

    // If all data was copied then reset inputPos and allow new input
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
Render as string for logging
***********************************************************************************************************************************/
String *
ioBufferToLog(const IoBuffer *this)
{
    return strNewFmt("{inputSame: %s, inputPos: %zu}", cvtBoolToConstZ(this->inputSame), this->inputPos);
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
