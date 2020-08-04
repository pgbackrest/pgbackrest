/***********************************************************************************************************************************
Handle IO Write
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/debug.h"
#include "common/io/handleWrite.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoHandleWrite
{
    MemContext *memContext;                                         // Object memory context
    const String *name;                                             // Handle name for error messages
    int handle;                                                     // Handle to write to
} IoHandleWrite;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_HANDLE_WRITE_TYPE                                                                                          \
    IoHandleWrite *
#define FUNCTION_LOG_IO_HANDLE_WRITE_FORMAT(value, buffer, bufferSize)                                                             \
    objToLog(value, "IoHandleWrite", buffer, bufferSize)

/***********************************************************************************************************************************
Write to the handle
***********************************************************************************************************************************/
static void
ioHandleWrite(THIS_VOID, const Buffer *buffer)
{
    THIS(IoHandleWrite);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_HANDLE_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    THROW_ON_SYS_ERROR_FMT(
        write(this->handle, bufPtrConst(buffer), bufUsed(buffer)) == -1, FileWriteError, "unable to write to %s", strZ(this->name));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get handle (file descriptor)
***********************************************************************************************************************************/
static int
ioHandleWriteHandle(const THIS_VOID)
{
    THIS(const IoHandleWrite);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_HANDLE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->handle);
}

/**********************************************************************************************************************************/
IoWrite *
ioHandleWriteNew(const String *name, int handle)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, handle);
    FUNCTION_LOG_END();

    IoWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoHandleWrite")
    {
        IoHandleWrite *driver = memNew(sizeof(IoHandleWrite));

        *driver = (IoHandleWrite)
        {
            .memContext = memContextCurrent(),
            .name = strDup(name),
            .handle = handle,
        };

        this = ioWriteNewP(driver, .handle = ioHandleWriteHandle, .write = ioHandleWrite);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_WRITE, this);
}

/**********************************************************************************************************************************/
void
ioHandleWriteOneStr(int handle, const String *string)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, handle);
        FUNCTION_LOG_PARAM(STRING, string);
    FUNCTION_LOG_END();

    ASSERT(string != NULL);

    if (write(handle, strZ(string), strSize(string)) != (int)strSize(string))
        THROW_SYS_ERROR(FileWriteError, "unable to write to handle");

    FUNCTION_LOG_RETURN_VOID();
}
