/***********************************************************************************************************************************
File Descriptor Io Write
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/debug.h"
#include "common/io/fd.h"
#include "common/io/fdWrite.h"
#include "common/io/write.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoFdWrite
{
    const String *name;                                             // File descriptor name for error messages
    int fd;                                                         // File descriptor to write to
    TimeMSec timeout;                                               // Timeout for write operation
} IoFdWrite;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_FD_WRITE_TYPE                                                                                              \
    IoFdWrite *
#define FUNCTION_LOG_IO_FD_WRITE_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "IoFdWrite", buffer, bufferSize)

/***********************************************************************************************************************************
// Can bytes be written immediately?
***********************************************************************************************************************************/
static bool
ioFdWriteReady(THIS_VOID, bool error)
{
    THIS(IoFdWrite);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FD_WRITE, this);
        FUNCTION_LOG_PARAM(BOOL, error);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = true;

    // Check if the file descriptor is ready to write
    if (!fdReadyWrite(this->fd, this->timeout))
    {
        // Error if requested
        if (error)
            THROW_FMT(FileWriteError, "timeout after %" PRIu64 "ms waiting for write to '%s'", this->timeout, strZ(this->name));

        // File descriptor is not ready to write
        result = false;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

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
ioFdWriteNew(const String *name, int fd, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(INT, fd);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    IoWrite *this = NULL;

    OBJ_NEW_BEGIN(IoFdWrite)
    {
        IoFdWrite *driver = OBJ_NEW_ALLOC();

        *driver = (IoFdWrite)
        {
            .name = strDup(name),
            .fd = fd,
            .timeout = timeout,
        };

        this = ioWriteNewP(driver, .fd = ioFdWriteFd, .ready = ioFdWriteReady, .write = ioFdWrite);
    }
    OBJ_NEW_END();

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
