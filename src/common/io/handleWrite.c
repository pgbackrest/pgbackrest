/***********************************************************************************************************************************
Handle IO Write
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/debug.h"
#include "common/io/handleWrite.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoHandleWrite
{
    MemContext *memContext;                                         // Object memory context
    IoWrite *io;                                                    // IoWrite interface
    const String *name;                                             // Handle name for error messages
    int handle;                                                     // Handle to write to
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
IoHandleWrite *
ioHandleWriteNew(const String *name, int handle)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, handle);
    FUNCTION_LOG_END();

    IoHandleWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoHandleWrite")
    {
        this = memNew(sizeof(IoHandleWrite));
        this->memContext = memContextCurrent();
        this->io = ioWriteNewP(this, .write = (IoWriteInterfaceWrite)ioHandleWrite);
        this->name = strDup(name);
        this->handle = handle;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_HANDLE_WRITE, this);
}

/***********************************************************************************************************************************
Write to the handle
***********************************************************************************************************************************/
void
ioHandleWrite(IoHandleWrite *this, Buffer *buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_HANDLE_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    THROW_ON_SYS_ERROR_FMT(
        write(this->handle, bufPtr(buffer), bufUsed(buffer)) == -1, FileWriteError, "unable to write to %s", strPtr(this->name));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Move the object to a new context
***********************************************************************************************************************************/
IoHandleWrite *
ioHandleWriteMove(IoHandleWrite *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_HANDLE_WRITE, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Get io interface
***********************************************************************************************************************************/
IoWrite *
ioHandleWriteIo(const IoHandleWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_HANDLE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->io);
}

/***********************************************************************************************************************************
Free the object
***********************************************************************************************************************************/
void
ioHandleWriteFree(IoHandleWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_HANDLE_WRITE, this);
    FUNCTION_LOG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write a string to the specified handle
***********************************************************************************************************************************/
void
ioHandleWriteOneStr(int handle, const String *string)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, handle);
        FUNCTION_LOG_PARAM(STRING, string);
    FUNCTION_LOG_END();

    ASSERT(string != NULL);

    if (write(handle, strPtr(string), strSize(string)) != (int)strSize(string))
        THROW_SYS_ERROR(FileWriteError, "unable to write to handle");

    FUNCTION_LOG_RETURN_VOID();
}
