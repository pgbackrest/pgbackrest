/***********************************************************************************************************************************
Buffer IO Read
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
typedef struct IoBufferRead
{
    const Buffer *read;                                             // Buffer to read data from

    size_t readPos;                                                 // Current position in the read buffer
    bool eof;                                                       // Has the end of the buffer been reached?
} IoBufferRead;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_BUFFER_READ_TYPE                                                                                           \
    IoBufferRead *
#define FUNCTION_LOG_IO_BUFFER_READ_FORMAT(value, buffer, bufferSize)                                                              \
    objNameToLog(value, "IoBufferRead", buffer, bufferSize)

/***********************************************************************************************************************************
Read data from the buffer
***********************************************************************************************************************************/
static size_t
ioBufferRead(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(IoBufferRead);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_BUFFER_READ, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    size_t actualBytes = 0;

    if (!this->eof)
    {
        // Determine how many bytes can actually be read from the buffer (if all bytes will be read then EOF)
        actualBytes = bufUsed(this->read) - this->readPos;

        if (actualBytes > bufRemains(buffer))
            actualBytes = bufRemains(buffer);
        else
            this->eof = true;

        // Copy bytes to buffer
        bufCatSub(buffer, this->read, this->readPos, actualBytes);
        this->readPos += actualBytes;
    }

    FUNCTION_LOG_RETURN(SIZE, actualBytes);
}

/***********************************************************************************************************************************
Have all bytes been read from the buffer?
***********************************************************************************************************************************/
static bool
ioBufferReadEof(THIS_VOID)
{
    THIS(IoBufferRead);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_BUFFER_READ, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(BOOL, this->eof);
}

/**********************************************************************************************************************************/
FN_EXTERN IoRead *
ioBufferReadNew(const Buffer *const buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(buffer != NULL);

    OBJ_NEW_BEGIN(IoBufferRead, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (IoBufferRead)
        {
            .read = buffer,
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_READ, ioReadNewP(this, .eof = ioBufferReadEof, .read = ioBufferRead));
}
