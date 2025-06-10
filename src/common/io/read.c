/***********************************************************************************************************************************
IO Read Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/read.h"
#include "common/log.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoRead
{
    IoReadPub pub;                                                  // Publicly accessible variables
    Buffer *input;                                                  // Input buffer
    Buffer *output;                                                 // Internal output buffer (extra output from buffered reads)
    size_t outputPos;                                               // Current position in the internal output buffer
};

/**********************************************************************************************************************************/
FN_EXTERN IoRead *
ioReadNew(void *const driver, const IoReadInterface interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(IO_READ_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface.read != NULL);

    OBJ_NEW_BEGIN(IoRead, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (IoRead)
        {
            .pub =
            {
                .driver = objMoveToInterface(driver, this, memContextPrior()),
                .interface = interface,
                .filterGroup = ioFilterGroupNew(),
            },
            .input = bufNew(ioBufferSize()),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_READ, this);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
ioReadOpen(IoRead *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!this->pub.opened && !this->pub.closed);
    ASSERT(ioFilterGroupSize(this->pub.filterGroup) == 0 || !ioReadBlock(this));

    // Open if the driver has an open function
    const bool result = ioReadInterface(this)->open != NULL ? ioReadInterface(this)->open(ioReadDriver(this)) : true;

    // Only open the filter group if the read was opened
    if (result)
        ioFilterGroupOpen(this->pub.filterGroup);

#ifdef DEBUG
    this->pub.opened = result;
#endif

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Is the driver at EOF?

This is different from the overall eof because filters may still be holding buffered data.
***********************************************************************************************************************************/
static bool
ioReadEofDriver(const IoRead *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.opened && !this->pub.closed);

    FUNCTION_LOG_RETURN(BOOL, ioReadInterface(this)->eof != NULL ? ioReadInterface(this)->eof(ioReadDriver(this)) : false);
}

/**********************************************************************************************************************************/
static void
ioReadInternal(IoRead *const this, Buffer *const buffer, const bool block)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);
    ASSERT(this->pub.opened && !this->pub.closed);

    // Loop until EOF or the output buffer is full
    const size_t bufferUsedBegin = bufUsed(buffer);

    while (!ioReadEof(this) && bufRemains(buffer) > 0)
    {
        // Process input buffer again to get more output
        if (ioFilterGroupInputSame(this->pub.filterGroup))
        {
            ioFilterGroupProcess(this->pub.filterGroup, this->input, buffer);
        }
        // Else new input can be accepted
        else
        {
            // Read if not EOF
            if (this->input != NULL)
            {
                if (!ioReadEofDriver(this))
                {
                    bufUsedZero(this->input);

                    // If blocking then limit the amount of data requested
                    if (ioReadBlock(this) && bufRemains(this->input) > bufRemains(buffer))
                        bufLimitSet(this->input, bufRemains(buffer));

                    ioReadInterface(this)->read(ioReadDriver(this), this->input, block);
                    bufLimitClear(this->input);
                }
                // Set input to NULL and flush (no need to actually free the buffer here as it will be freed with the mem context)
                else
                    this->input = NULL;
            }

            // Process the input buffer (or flush if NULL)
            if (this->input == NULL || !bufEmpty(this->input))
                ioFilterGroupProcess(this->pub.filterGroup, this->input, buffer);

            // Stop if not blocking -- we don't need to fill the buffer as long as we got some data
            if (!block && bufUsed(buffer) > bufferUsedBegin)
                break;
        }

        // Eof when no more input and the filter group is done
        this->pub.eofAll = ioReadEofDriver(this) && ioFilterGroupDone(this->pub.filterGroup);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Read data and use buffered line read output when present
***********************************************************************************************************************************/
FN_EXTERN size_t
ioRead(IoRead *const this, Buffer *const buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BUFFER, this->output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);
    ASSERT(this->pub.opened && !this->pub.closed);

    // Store size of remaining portion of buffer to calculate total read at the end
    const size_t outputRemains = bufRemains(buffer);

    // Copy any data in the internal output buffer
    if (this->output != NULL && bufUsed(this->output) - this->outputPos > 0 && bufRemains(buffer) > 0)
    {
        // Internal output buffer remains taking into account the position
        const size_t outputInternalRemains = bufUsed(this->output) - this->outputPos;

        // Determine how much data should be copied
        const size_t size = outputInternalRemains > bufRemains(buffer) ? bufRemains(buffer) : outputInternalRemains;

        // Copy data to the output buffer
        bufCatSub(buffer, this->output, this->outputPos, size);
        this->outputPos += size;
    }

    // Read data
    ioReadInternal(this, buffer, true);

    FUNCTION_LOG_RETURN(SIZE, outputRemains - bufRemains(buffer));
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
ioReadSmall(IoRead *const this, Buffer *const buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_READ, this);
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);
    ASSERT(this->pub.opened && !this->pub.closed);

    // Allocate the internal output buffer if it has not already been allocated
    if (this->output == NULL)
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            this->output = bufNew(ioBufferSize());
        }
        MEM_CONTEXT_OBJ_END();
    }

    // Store size of remaining portion of buffer to calculate total read at the end
    size_t outputRemains = bufRemains(buffer);

    do
    {
        // Internal output buffer remains taking into account the position
        const size_t outputInternalRemains = bufUsed(this->output) - this->outputPos;

        // Use any data in the internal output buffer
        if (outputInternalRemains > 0)
        {
            // Determine how much data should be copied
            const size_t size = outputInternalRemains > bufRemains(buffer) ? bufRemains(buffer) : outputInternalRemains;

            // Copy data to the output buffer
            bufCatSub(buffer, this->output, this->outputPos, size);
            this->outputPos += size;
        }

        // If more data is required
        if (!bufFull(buffer))
        {
            // If the data required is the same size as the internal output buffer then just read into the external buffer
            if (bufRemains(buffer) >= bufSize(this->output))
            {
                ioReadInternal(this, buffer, true);
            }
            // Else read as much data as is available. If it is not enough we will try again later.
            else
            {
                // Clear the internal output buffer since all data was copied already
                bufUsedZero(this->output);
                this->outputPos = 0;

                ioReadInternal(this, this->output, false);
            }
        }
    }
    while (!bufFull(buffer) && !ioReadEof(this));

    FUNCTION_TEST_RETURN(SIZE, outputRemains - bufRemains(buffer));
}

/***********************************************************************************************************************************
The entire string to search for must fit within a single buffer.
***********************************************************************************************************************************/
FN_EXTERN String *
ioReadLineParam(IoRead *const this, const bool allowEof)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, this);
        FUNCTION_LOG_PARAM(BOOL, allowEof);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.opened && !this->pub.closed);

    // Allocate the output buffer if it has not already been allocated. This buffer is not allocated at object creation because it
    // is not always used.
    if (this->output == NULL)
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            this->output = bufNew(ioBufferSize());
        }
        MEM_CONTEXT_OBJ_END();
    }

    // Search for a linefeed
    String *result = NULL;

    do
    {
        // Internal output buffer remains taking into account the position
        size_t outputInternalRemains = bufUsed(this->output) - this->outputPos;

        if (outputInternalRemains > 0)
        {
            // Internal output buffer pointer taking into account the position
            const char *const outputPtr = (char *)bufPtr(this->output) + this->outputPos;

            // Search for a linefeed in the buffer
            const char *const linefeed = memchr(outputPtr, '\n', outputInternalRemains);

            // A linefeed was found so get the string
            if (linefeed != NULL)
            {
                // Get the string size
                const size_t size = (size_t)(linefeed - outputPtr);

                // Create the string
                result = strNewZN(outputPtr, size);
                this->outputPos += size + 1;
            }
        }

        // Read data if no linefeed was found in the existing buffer
        if (result == NULL)
        {
            // If there is remaining data left in the internal output buffer then trim off the used data
            if (outputInternalRemains > 0)
            {
                memmove(
                    bufPtr(this->output), bufPtr(this->output) + (bufUsed(this->output) - outputInternalRemains),
                    outputInternalRemains);
            }

            // Set used bytes and reset position
            bufUsedSet(this->output, outputInternalRemains);
            this->outputPos = 0;

            // If the buffer is full then the linefeed (if it exists) is outside the buffer
            if (bufFull(this->output))
                THROW_FMT(FileReadError, "unable to find line in %zu byte buffer", bufUsed(this->output));

            if (ioReadEof(this))
            {
                if (allowEof)
                    result = strNewZN((char *)bufPtr(this->output), bufUsed(this->output));
                else
                    THROW(FileReadError, "unexpected eof while reading line");
            }
            else
                ioReadInternal(this, this->output, false);
        }
    }
    while (result == NULL);

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN uint64_t
ioReadVarIntU64(IoRead *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, this);
    FUNCTION_LOG_END();

    // Allocate the internal output buffer if it has not already been allocated
    if (this->output == NULL)
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            this->output = bufNew(ioBufferSize());
        }
        MEM_CONTEXT_OBJ_END();
    }

    uint64_t result = 0;
    uint8_t byte;

    // Convert bytes from varint-128 encoding to a uint64
    for (unsigned int bufferIdx = 0; bufferIdx < CVT_VARINT128_BUFFER_SIZE; bufferIdx++)
    {
        // Get more bytes if needed
        if (bufUsed(this->output) - this->outputPos == 0)
        {
            // Clear the internal output buffer since all data was copied already
            bufUsedZero(this->output);
            this->outputPos = 0;

            // Read into the internal output buffer
            ioReadInternal(this, this->output, false);

            // Error on eof
            if (bufUsed(this->output) == 0)
                THROW(FileReadError, "unexpected eof");
        }

        // Get the next encoded byte
        byte = bufPtr(this->output)[this->outputPos++];

        // Shift the lower order 7 encoded bits into the uint64 in reverse order
        result |= (uint64_t)(byte & 0x7f) << (7 * bufferIdx);

        // Done if the high order bit is not set to indicate more data
        if (byte < 0x80)
            break;
    }

    // By this point all bytes should have been read so error if this is not the case. This could be due to a coding error or
    // corrupton in the data stream.
    if (byte >= 0x80)
        THROW(FormatError, "unterminated base-128 integer");

    FUNCTION_LOG_RETURN(UINT64, result);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
ioReadReady(IoRead *const this, const IoReadReadyParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, this);
        FUNCTION_LOG_PARAM(BOOL, param.error);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = true;

    if (ioReadInterface(this)->ready != NULL)
        result = ioReadInterface(this)->ready(ioReadDriver(this), param.error);

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN uint64_t
ioReadFlush(IoRead *const this, const IoReadFlushParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, this);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnBytes);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.opened && !this->pub.closed);

    // Flush remaining data
    uint64_t result = 0;

    if (!ioReadEof(this))
    {
        Buffer *const buffer = bufNew(ioBufferSize());

        do
        {
            result += ioRead(this, buffer);
            bufUsedZero(buffer);
        }
        while (!ioReadEof(this));

        bufFree(buffer);
    }

    // Error when bytes found and error requested
    if (result != 0 && param.errorOnBytes)
        THROW_FMT(FileReadError, "expected EOF but flushed %" PRIu64 " byte(s)", result);

    FUNCTION_LOG_RETURN(UINT64, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioReadClose(IoRead *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.opened && !this->pub.closed);

    // Close the filter group and gather results
    ioFilterGroupClose(this->pub.filterGroup);

    // Close the driver if there is a close function
    if (ioReadInterface(this)->close != NULL)
        ioReadInterface(this)->close(ioReadDriver(this));

#ifdef DEBUG
    this->pub.closed = true;
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN int
ioReadFd(const IoRead *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INT, ioReadInterface(this)->fd == NULL ? -1 : ioReadInterface(this)->fd(ioReadDriver(this)));
}
