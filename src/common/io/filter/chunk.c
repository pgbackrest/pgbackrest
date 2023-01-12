/***********************************************************************************************************************************
Chunk Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/filter/chunk.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoChunk
{
    MemContext *memContext;                                         // Mem context of filter

    const uint8_t *buffer;                                          // Internal buffer
    size_t bufferSize;                                              // Buffer size
    size_t bufferOffset;                                            // Buffer offset
    size_t sizeLast;                                                // Size of last chunk
    bool done;                                                      // Is the filter done?
    uint8_t header[CVT_VARINT128_BUFFER_SIZE];                      // Chunk header
} IoChunk;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_CHUNK_TYPE                                                                                                 \
    IoChunk *
#define FUNCTION_LOG_IO_CHUNK_FORMAT(value, buffer, bufferSize)                                                                    \
    objNameToLog(value, "IoChunk", buffer, bufferSize)

/***********************************************************************************************************************************
Should the same input be provided again?
***********************************************************************************************************************************/
static bool
ioChunkInputSame(const THIS_VOID)
{
    THIS(const IoChunk);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_CHUNK, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->buffer != NULL);
}

/***********************************************************************************************************************************
Is filter done?
***********************************************************************************************************************************/
static bool
ioChunkDone(const THIS_VOID)
{
    THIS(const IoChunk);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_CHUNK, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->done && !ioChunkInputSame(this));
}

/***********************************************************************************************************************************
Count bytes in the input
***********************************************************************************************************************************/
static void
ioChunkProcess(THIS_VOID, const Buffer *const input, Buffer *const output)
{
    THIS(IoChunk);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_CHUNK, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
        FUNCTION_LOG_PARAM(BUFFER, output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(output != NULL);

    // If there is input to process
    if (input != NULL)
    {
        // Write the chunk size
        if (this->buffer == NULL)
        {
            // Initialize the header with the chunk size
            this->buffer = this->header;
            this->bufferSize = 0;
            this->bufferOffset = 0;

            cvtUInt64ToVarInt128(
                this->sizeLast == 0 ? bufUsed(input) : cvtInt64ToZigZag((int64_t)bufUsed(input) - (int64_t)this->sizeLast) + 1,
                this->header, &this->bufferSize, SIZE_OF_STRUCT_MEMBER(IoChunk, header));

            this->sizeLast = bufUsed(input);
        }

        // Output the chunk
        do
        {
            // Output the entire buffer if possible
            if (bufRemains(output) >= this->bufferSize - this->bufferOffset)
            {
                bufCatC(output, this->buffer, this->bufferOffset, this->bufferSize - this->bufferOffset);

                // If the header was written then switch to the chunk
                if (this->buffer == this->header)
                {
                    this->buffer = bufPtrConst(input);
                    this->bufferSize = bufUsed(input);
                    this->bufferOffset = 0;
                }
                // Else done writing the chunk
                else
                    this->buffer = NULL;
            }
            // Else output part of the buffer
            else
            {
                const size_t outputSize = bufRemains(output);
                bufCatC(output, this->buffer, this->bufferOffset, outputSize);

                this->bufferOffset += outputSize;
            }
        }
        while (ioChunkInputSame(this) && !bufFull(output));
    }
    // Else processing is complete
    else
    {
        ASSERT(bufRemains(output) > 0);

        // Write the terminating zero byte
        *(bufPtr(output) + bufUsed(output)) = '\0';
        bufUsedInc(output, 1);

        this->done = true;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
ioChunkNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(IoChunk, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        IoChunk *const driver = OBJ_NAME(OBJ_NEW_ALLOC(), IoFilter::IoChunk);

        *driver = (IoChunk)
        {
            .memContext = memContextCurrent(),
        };

        this = ioFilterNewP(
            CHUNK_FILTER_TYPE, driver, NULL, .done = ioChunkDone, .inOut = ioChunkProcess,
            .inputSame = ioChunkInputSame);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
