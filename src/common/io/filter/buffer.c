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
    FUNCTION_LOG_VOID(logLevelTrace);

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

    FUNCTION_LOG_RETURN(IO_BUFFER, this);
}

/***********************************************************************************************************************************
Move data from the input buffer to the output buffer
***********************************************************************************************************************************/
void
ioBufferProcess(IoBuffer *this, const Buffer *input, Buffer *output)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_BUFFER, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
        FUNCTION_LOG_PARAM(BUFFER, output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(input != NULL);
    ASSERT(output != NULL);

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

    FUNCTION_LOG_RETURN_VOID();
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
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/***********************************************************************************************************************************
Get filter interface
***********************************************************************************************************************************/
IoFilter *
ioBufferFilter(const IoBuffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->filter);
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
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_BUFFER, this);
    FUNCTION_LOG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_LOG_RETURN_VOID();
}
