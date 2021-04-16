/***********************************************************************************************************************************
Socket Session
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/io/socket/client.h"
#include "common/io/session.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct SocketSession
{
    MemContext *memContext;                                         // Mem context
    IoSessionRole role;                                             // Role (server or client)
    int fd;                                                         // File descriptor
    String *host;                                                   // Hostname or IP address
    unsigned int port;                                              // Port to connect to host on
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)

    IoRead *read;                                                   // IoRead interface to the file descriptor
    IoWrite *write;                                                 // IoWrite interface to the file descriptor
} SocketSession;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static String *
sckSessionToLog(const THIS_VOID)
{
    THIS(const SocketSession);

    return strNewFmt("{fd %d, host: %s, port: %u, timeout: %" PRIu64 "}", this->fd, strZ(this->host), this->port, this->timeout);
}

#define FUNCTION_LOG_SOCKET_SESSION_TYPE                                                                                           \
    SocketSession *
#define FUNCTION_LOG_SOCKET_SESSION_FORMAT(value, buffer, bufferSize)                                                              \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, sckSessionToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free connection
***********************************************************************************************************************************/
static void
sckSessionFreeResource(THIS_VOID)
{
    THIS(SocketSession);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SOCKET_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    close(this->fd);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static void
sckSessionClose(THIS_VOID)
{
    THIS(SocketSession);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SOCKET_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // If not already closed
    if (this->fd != -1)
    {
        // Clear the callback to close the socket
        memContextCallbackClear(this->memContext);

        // Close the socket
        close(this->fd);
        this->fd = -1;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static int
sckSessionFd(THIS_VOID)
{
    THIS(SocketSession);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SOCKET_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->fd);
}

/**********************************************************************************************************************************/
static IoRead *
sckSessionIoRead(THIS_VOID)
{
    THIS(SocketSession);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SOCKET_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->read);
}

/**********************************************************************************************************************************/
static IoWrite *
sckSessionIoWrite(THIS_VOID)
{
    THIS(SocketSession);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SOCKET_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->write);
}

/**********************************************************************************************************************************/
static IoSessionRole
sckSessionRole(const THIS_VOID)
{
    THIS(const SocketSession);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SOCKET_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->role);
}

/**********************************************************************************************************************************/
static const IoSessionInterface sckSessionInterface =
{
    .type = &IO_CLIENT_SOCKET_TYPE_STR,
    .close = sckSessionClose,
    .fd = sckSessionFd,
    .ioRead = sckSessionIoRead,
    .ioWrite = sckSessionIoWrite,
    .role = sckSessionRole,
    .toLog = sckSessionToLog,
};

IoSession *
sckSessionNew(IoSessionRole role, int fd, const String *host, unsigned int port, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(STRING_ID, role);
        FUNCTION_LOG_PARAM(INT, fd);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(fd != -1);
    ASSERT(host != NULL);

    IoSession *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("SocketSession")
    {
        SocketSession *driver = memNew(sizeof(SocketSession));

        String *name = strNewFmt("%s:%u", strZ(host), port);

        *driver = (SocketSession)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .role = role,
            .fd = fd,
            .host = strDup(host),
            .port = port,
            .timeout = timeout,
            .read = ioFdReadNew(name, fd, timeout),
            .write = ioFdWriteNew(name, fd, timeout),
        };

        strFree(name);

        // Open read/write io
        ioReadOpen(driver->read);
        ioWriteOpen(driver->write);

        // Ensure file descriptor is closed
        memContextCallbackSet(driver->memContext, sckSessionFreeResource, driver);

        this = ioSessionNew(driver, &sckSessionInterface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_SESSION, this);
}
