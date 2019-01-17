/***********************************************************************************************************************************
Handle IO Write
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/assert.h"
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
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INT, handle);
    FUNCTION_DEBUG_END();

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

    FUNCTION_DEBUG_RESULT(IO_HANDLE_WRITE, this);
}

/***********************************************************************************************************************************
Write to the handle
***********************************************************************************************************************************/
void
ioHandleWrite(IoHandleWrite *this, Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_HANDLE_WRITE, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(buffer != NULL);
    FUNCTION_DEBUG_END();

    THROW_ON_SYS_ERROR_FMT(
        write(this->handle, bufPtr(buffer), bufUsed(buffer)) == -1, FileWriteError, "unable to write to %s", strPtr(this->name));

    FUNCTION_DEBUG_RESULT_VOID();
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

        FUNCTION_TEST_ASSERT(parentNew != NULL);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RESULT(IO_HANDLE_WRITE, this);
}

/***********************************************************************************************************************************
Get io interface
***********************************************************************************************************************************/
IoWrite *
ioHandleWriteIo(const IoHandleWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_HANDLE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_WRITE, this->io);
}

/***********************************************************************************************************************************
Free the object
***********************************************************************************************************************************/
void
ioHandleWriteFree(IoHandleWrite *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(IO_HANDLE_WRITE, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Write a string to the specified handle
***********************************************************************************************************************************/
void
ioHandleWriteOneStr(int handle, const String *string)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INT, handle);
        FUNCTION_DEBUG_PARAM(STRING, string);

        FUNCTION_DEBUG_ASSERT(string != NULL);
    FUNCTION_DEBUG_END();

    if (write(handle, strPtr(string), strSize(string)) != (int)strSize(string))
        THROW_SYS_ERROR(FileWriteError, "unable to write to handle");

    FUNCTION_DEBUG_RESULT_VOID();
}
