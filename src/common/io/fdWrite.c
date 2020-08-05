/***********************************************************************************************************************************
File Descriptor Io Write
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/debug.h"
#include "common/io/fdWrite.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoFdWrite
{
    MemContext *memContext;                                         // Object memory context
    const String *name;                                             // File descriptor name for error messages
    int fd;                                                         // File descriptor to write to
} IoFdWrite;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_FD_WRITE_TYPE                                                                                              \
    IoFdWrite *
#define FUNCTION_LOG_IO_FD_WRITE_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "IoFdWrite", buffer, bufferSize)

/***********************************************************************************************************************************
Write to the file descriptor
***********************************************************************************************************************************/
static void
ioFdWrite(THIS_VOID, const Buffer *buffer)
{
    THIS(IoFdWrite);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FD_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    THROW_ON_SYS_ERROR_FMT(
        write(this->fd, bufPtrConst(buffer), bufUsed(buffer)) == -1, FileWriteError, "unable to write to %s", strZ(this->name));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get file descriptor
***********************************************************************************************************************************/
static int
ioFdWriteFd(const THIS_VOID)
{
    THIS(const IoFdWrite);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FD_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->fd);
}

/**********************************************************************************************************************************/
IoWrite *
ioFdWriteNew(const String *name, int fd)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, fd);
    FUNCTION_LOG_END();

    IoWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("IoFdWrite")
    {
        IoFdWrite *driver = memNew(sizeof(IoFdWrite));

        *driver = (IoFdWrite)
        {
            .memContext = memContextCurrent(),
            .name = strDup(name),
            .fd = fd,
        };

        this = ioWriteNewP(driver, .fd = ioFdWriteFd, .write = ioFdWrite);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_WRITE, this);
}

/**********************************************************************************************************************************/
void
ioFdWriteOneStr(int fd, const String *string)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, fd);
        FUNCTION_LOG_PARAM(STRING, string);
    FUNCTION_LOG_END();

    ASSERT(string != NULL);

    if (write(fd, strZ(string), strSize(string)) != (int)strSize(string))
        THROW_SYS_ERROR(FileWriteError, "unable to write to fd");

    FUNCTION_LOG_RETURN_VOID();
}
