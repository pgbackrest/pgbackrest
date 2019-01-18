/***********************************************************************************************************************************
IO Functions
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"

/***********************************************************************************************************************************
Buffer size

This buffer size will be used for all IO operations that require buffers not passed by the caller.  Initially it is set to a
conservative default with the expectation that it will be changed to a new value after options have been loaded.  In general callers
should set their buffer size using ioBufferSize() but there may be cases where an alternative buffer size makes sense.
***********************************************************************************************************************************/
#define IO_BUFFER_BLOCK_SIZE                                        (8 * 1024)

static size_t bufferSize = (8 * IO_BUFFER_BLOCK_SIZE);

/***********************************************************************************************************************************
Get/set buffer size
***********************************************************************************************************************************/
size_t
ioBufferSize(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RESULT(SIZE, bufferSize);
}

void
ioBufferSizeSet(size_t bufferSizeParam)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, bufferSizeParam);
    FUNCTION_TEST_END();

    bufferSize = bufferSizeParam;

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Read all IO into a buffer
***********************************************************************************************************************************/
Buffer *
ioReadBuf(IoRead *read)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_READ, read);

        FUNCTION_TEST_ASSERT(read != NULL);
    FUNCTION_TEST_END();

    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Read IO into the buffer
        result = bufNew(0);

        do
        {
            bufResize(result, bufSize(result) + ioBufferSize());
            ioRead(read, result);
        }
        while (!ioReadEof(read));

        // Resize the buffer and move to calling context
        bufResize(result, bufUsed(result));
        bufMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RESULT(BUFFER, result);
}
