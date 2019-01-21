/***********************************************************************************************************************************
Buffer IO Write
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/io/bufferWrite.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoBufferWrite
{
    MemContext *memContext;                                         // Object memory context
    IoWrite *io;                                                    // IoWrite interface
    Buffer *write;                                                  // Buffer to write into
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoBufferWrite *
ioBufferWriteNew(Buffer *buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(buffer != NULL);

    IoBufferWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoBufferWrite")
    {
        this = memNew(sizeof(IoBufferWrite));
        this->memContext = memContextCurrent();
        this->io = ioWriteNewP(this, .write = (IoWriteInterfaceWrite)ioBufferWrite);
        this->write = buffer;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_BUFFER_WRITE, this);
}

/***********************************************************************************************************************************
Write to the buffer
***********************************************************************************************************************************/
void
ioBufferWrite(IoBufferWrite *this, Buffer *buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_BUFFER_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    bufCat(this->write, buffer);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Move the object to a new context
***********************************************************************************************************************************/
IoBufferWrite *
ioBufferWriteMove(IoBufferWrite *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_BUFFER_WRITE, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RETURN(IO_BUFFER_WRITE, this);
}

/***********************************************************************************************************************************
Get io interface
***********************************************************************************************************************************/
IoWrite *
ioBufferWriteIo(const IoBufferWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_BUFFER_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(IO_WRITE, this->io);
}

/***********************************************************************************************************************************
Free the object
***********************************************************************************************************************************/
void
ioBufferWriteFree(IoBufferWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_BUFFER_WRITE, this);
    FUNCTION_LOG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_LOG_RETURN_VOID();
}
