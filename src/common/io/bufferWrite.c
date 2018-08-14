/***********************************************************************************************************************************
Buffer IO Write
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/debug.h"
#include "common/io/bufferWrite.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Buffer write object
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
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_DEBUG_END();

    IoBufferWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoBufferWrite")
    {
        this = memNew(sizeof(IoBufferWrite));
        this->memContext = memContextCurrent();

        this->write = buffer;
        this->io = ioWriteNew(this, NULL, (IoWriteProcess)ioBufferWrite, NULL);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(IO_BUFFER_WRITE, this);
}

/***********************************************************************************************************************************
Write to the buffer
***********************************************************************************************************************************/
void
ioBufferWrite(IoBufferWrite *this, Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_BUFFER_WRITE, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(buffer != NULL);
    FUNCTION_DEBUG_END();

    bufCat(this->write, buffer);

    FUNCTION_DEBUG_RESULT_VOID();
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

        FUNCTION_TEST_ASSERT(parentNew != NULL);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RESULT(IO_BUFFER_WRITE, this);
}

/***********************************************************************************************************************************
Get io interface
***********************************************************************************************************************************/
IoWrite *
ioBufferWriteIo(const IoBufferWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_BUFFER_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_WRITE, this->io);
}

/***********************************************************************************************************************************
Free the object
***********************************************************************************************************************************/
void
ioBufferWriteFree(IoBufferWrite *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_BUFFER_WRITE, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
