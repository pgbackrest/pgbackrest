/***********************************************************************************************************************************
Sink IO Write
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/sinkWrite.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoSinkWrite
{
    MemContext *memContext;                                         // Object memory context
} IoSinkWrite;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_BUFFER_WRITE_TYPE                                                                                          \
    IoSinkWrite *
#define FUNCTION_LOG_IO_BUFFER_WRITE_FORMAT(value, buffer, bufferSize)                                                             \
    objToLog(value, "IoSinkWrite", buffer, bufferSize)

/***********************************************************************************************************************************
Throw the buffer down the drain
***********************************************************************************************************************************/
static void
ioSinkWrite(THIS_VOID, const Buffer *buffer)
{
    THIS(IoSinkWrite);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_BUFFER_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoWrite *
ioSinkWriteNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoSinkWrite")
    {
        IoSinkWrite *driver = memNew(sizeof(IoSinkWrite));
        driver->memContext = memContextCurrent();

        this = ioWriteNewP(driver, .write = ioSinkWrite);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_WRITE, this);
}
