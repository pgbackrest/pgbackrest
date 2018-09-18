/***********************************************************************************************************************************
Buffer IO Read
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/debug.h"
#include "common/io/bufferRead.h"
#include "common/io/read.intern.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoBufferRead
{
    MemContext *memContext;                                         // Object memory context
    IoRead *io;                                                     // IoRead interface
    const Buffer *read;                                             // Buffer to read data from

    size_t readPos;                                                 // Current position in the read buffer
    bool eof;                                                       // Has the end of the buffer been reached?
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoBufferRead *
ioBufferReadNew(const Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_DEBUG_END();

    IoBufferRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoBufferRead")
    {
        this = memNew(sizeof(IoBufferRead));
        this->memContext = memContextCurrent();
        this->io = ioReadNewP(this, .eof = (IoReadInterfaceEof)ioBufferReadEof, .read = (IoReadInterfaceRead)ioBufferRead);
        this->read = buffer;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(IO_BUFFER_READ, this);
}

/***********************************************************************************************************************************
Read data from the buffer
***********************************************************************************************************************************/
size_t
ioBufferRead(IoBufferRead *this, Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_BUFFER_READ, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(buffer != NULL);
    FUNCTION_DEBUG_END();

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

    FUNCTION_DEBUG_RESULT(SIZE, actualBytes);
}

/***********************************************************************************************************************************
Move the object to a new context
***********************************************************************************************************************************/
IoBufferRead *
ioBufferReadMove(IoBufferRead *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_BUFFER_READ, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);

        FUNCTION_TEST_ASSERT(parentNew != NULL);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RESULT(IO_BUFFER_READ, this);
}

/***********************************************************************************************************************************
Have all bytes been read from the buffer?
***********************************************************************************************************************************/
bool
ioBufferReadEof(const IoBufferRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_BUFFER_READ, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(BOOL, this->eof);
}

/***********************************************************************************************************************************
Get io interface
***********************************************************************************************************************************/
IoRead *
ioBufferReadIo(const IoBufferRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_BUFFER_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_READ, this->io);
}

/***********************************************************************************************************************************
Free the object
***********************************************************************************************************************************/
void
ioBufferReadFree(IoBufferRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_BUFFER_READ, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
