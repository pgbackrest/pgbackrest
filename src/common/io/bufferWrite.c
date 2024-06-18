/***********************************************************************************************************************************
Buffer IO Write
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/bufferWrite.h"
#include "common/io/write.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoBufferWrite
{
    Buffer *write;                                                  // Buffer to write into
} IoBufferWrite;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_BUFFER_WRITE_TYPE                                                                                          \
    IoBufferWrite *
#define FUNCTION_LOG_IO_BUFFER_WRITE_FORMAT(value, buffer, bufferSize)                                                             \
    objNameToLog(value, "IoBufferWrite", buffer, bufferSize)

/***********************************************************************************************************************************
Write to the buffer
***********************************************************************************************************************************/
static void
ioBufferWrite(THIS_VOID, const Buffer *const buffer)
{
    THIS(IoBufferWrite);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_BUFFER_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    bufCat(this->write, buffer);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN IoWrite *
ioBufferWriteNew(Buffer *const buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(buffer != NULL);

    OBJ_NEW_BEGIN(IoBufferWrite, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (IoBufferWrite)
        {
            .write = buffer,
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_WRITE, ioWriteNewP(this, .write = ioBufferWrite));
}
