/***********************************************************************************************************************************
IO Functions
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/filter/sink.h"
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

// I/O timeout in milliseconds
static TimeMSec timeoutMs = 60000;

/**********************************************************************************************************************************/
size_t
ioBufferSize(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(bufferSize);
}

void
ioBufferSizeSet(size_t bufferSizeParam)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, bufferSizeParam);
    FUNCTION_TEST_END();

    bufferSize = bufferSizeParam;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
TimeMSec
ioTimeoutMs(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(timeoutMs);
}

void
ioTimeoutMsSet(TimeMSec timeoutMsParam)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TIME_MSEC, timeoutMsParam);
    FUNCTION_TEST_END();

    timeoutMs = timeoutMsParam;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
Buffer *
ioReadBuf(IoRead *read)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_READ, read);
    FUNCTION_TEST_END();

    ASSERT(read != NULL);

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

        // Resize the buffer and move to prior context
        bufResize(result, bufUsed(result));
        bufMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void
ioCopy(IoRead *const source, IoWrite *const destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_READ, read);
        FUNCTION_TEST_PARAM(IO_READ, write);
    FUNCTION_TEST_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Buffer *const buffer = bufNew(ioBufferSize());

        do
        {
            ioRead(source, buffer);
            ioWrite(destination, buffer);
            bufUsedZero(buffer);
        }
        while (!ioReadEof(source));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
ioReadDrain(IoRead *read)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_READ, read);
    FUNCTION_TEST_END();

    ASSERT(read != NULL);

    // Add a sink filter so we only need one read
    ioFilterGroupAdd(ioReadFilterGroup(read), ioSinkNew());

    // Check if the IO can be opened
    bool result = ioReadOpen(read);

    if (result)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // A single read that returns zero bytes
            ioRead(read, bufNew(1));
            ASSERT(ioReadEof(read));

            // Close the IO
            ioReadClose(read);
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RETURN(result);
}
