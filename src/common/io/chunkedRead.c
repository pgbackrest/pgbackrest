/***********************************************************************************************************************************
Read Chunked I/O
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/bufferRead.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoChunkedRead
{
    IoRead *read;                                                   // IoRead to read chunked data from
    bool eof;                                                       // Has the end of the chunked data been reached?
    size_t chunkLast;                                               // Size of the last chunk
    size_t chunkRemains;                                            // Remaining data in the current chunk
} IoChunkedRead;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_CHUNKED_READ_TYPE                                                                                           \
    IoChunkedRead *
#define FUNCTION_LOG_IO_CHUNKED_READ_FORMAT(value, buffer, bufferSize)                                                              \
    objToLog(value, "IoChunkedRead", buffer, bufferSize)

/***********************************************************************************************************************************
Read data from the buffer
***********************************************************************************************************************************/
static size_t
ioChunkedRead(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(IoChunkedRead);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_CHUNKED_READ, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    size_t actualBytes = 0;

    while (!bufFull(buffer))
    {
        if (this->chunkRemains == 0)
        {
            const uint64_t chunkDelta = ioReadVarIntU64(this->read);

            // Stop when chunk delta is zero, which indicates the end of the chunk list
            if (chunkDelta == 0)
            {
                this->eof = true;
                break;
            }

            // Calculate next chunk size from delta
            if (this->chunkLast == 0)
                this->chunkRemains = (size_t)chunkDelta;
            else
                this->chunkRemains = (uint64_t)(cvtInt64FromZigZag(chunkDelta - 1) + (int64_t)this->chunkLast);

            this->chunkLast = this->chunkRemains;
        }

        if (this->chunkRemains < bufRemains(buffer))
        {
            bufLimitSet(buffer, bufUsed(buffer) + this->chunkRemains);
            ioRead(this->read, buffer);

            actualBytes += this->chunkRemains;
            this->chunkRemains = 0;
        }
        else
        {
            actualBytes += bufRemains(buffer);
            this->chunkRemains -= bufRemains(buffer);

            ioRead(this->read, buffer);
        }
    }

    FUNCTION_LOG_RETURN(SIZE, actualBytes);
}

/***********************************************************************************************************************************
Have all bytes been read from the buffer?
***********************************************************************************************************************************/
static bool
ioChunkedReadEof(THIS_VOID)
{
    THIS(IoChunkedRead);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_CHUNKED_READ, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(BOOL, this->eof);
}

/**********************************************************************************************************************************/
IoRead *
ioChunkedReadNew(IoRead *const read)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_READ, read);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);

    IoRead *this = NULL;

    OBJ_NEW_BEGIN(IoChunkedRead, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        IoChunkedRead *driver = OBJ_NEW_ALLOC();

        *driver = (IoChunkedRead)
        {
            .read = read,
        };

        this = ioReadNewP(driver, .eof = ioChunkedReadEof, .read = ioChunkedRead);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_READ, this);
}
