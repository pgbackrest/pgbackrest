/***********************************************************************************************************************************
IO Buffer Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>

#include "common/debug.h"
#include "common/io/filter/buffer.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define BUFFER_FILTER_TYPE                                          "buffer"
    STRING_STATIC(BUFFER_FILTER_TYPE_STR,                           BUFFER_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoBuffer
{
    MemContext *memContext;                                         // Mem context of filter

    size_t inputPos;                                                // Position in input buffer
    bool inputSame;                                                 // Is the same input required again?
} IoBuffer;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *
ioBufferToLog(const IoBuffer *this)
{
    return strNewFmt("{inputSame: %s, inputPos: %zu}", cvtBoolToConstZ(this->inputSame), this->inputPos);
}

#define FUNCTION_LOG_IO_BUFFER_TYPE                                                                                                \
    IoBuffer *
#define FUNCTION_LOG_IO_BUFFER_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, ioBufferToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Move data from the input buffer to the output buffer
***********************************************************************************************************************************/
static void
ioBufferProcess(THIS_VOID, const Buffer *input, Buffer *output)
{
    THIS(IoBuffer);

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
static bool
ioBufferInputSame(const THIS_VOID)
{
    THIS(const IoBuffer);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoFilter *
ioBufferNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoBuffer")
    {
        IoBuffer *driver = memNew(sizeof(IoBuffer));

        *driver = (IoBuffer)
        {
            .memContext = memContextCurrent(),
        };

        this = ioFilterNewP(BUFFER_FILTER_TYPE_STR, driver, NULL, .inOut = ioBufferProcess, .inputSame = ioBufferInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
