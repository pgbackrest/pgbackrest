/***********************************************************************************************************************************
Socket Session
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/debug.h"
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/io/session.h"
#include "common/io/socket/client.h"
#include "common/io/socket/session.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct SocketSession
{
    IoSessionRole role;                                             // Role (server or client)
    int fd;                                                         // File descriptor
    String *host;                                                   // Hostname or IP address
    unsigned int port;                                              // Port to connect to host on
    TimeMSec timeout;                                               // Timeout for any i/o operation (read, write, etc.)

    IoRead *read;                                                   // IoRead interface to the file descriptor
    IoWrite *write;                                                 // IoWrite interface to the file descriptor
} SocketSession;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static void
sckSessionToLog(const THIS_VOID, StringStatic *const debugLog)
{
    THIS(const SocketSession);

    strStcFmt(
        debugLog, "{host: %s, port: %u, fd: %d, timeout: %" PRIu64 "}", strZ(this->host), this->port, this->fd, this->timeout);
}

#define FUNCTION_LOG_SOCKET_SESSION_TYPE                                                                                           \
    SocketSession *
#define FUNCTION_LOG_SOCKET_SESSION_FORMAT(value, buffer, bufferSize)                                                              \
    FUNCTION_LOG_OBJECT_FORMAT(value, sckSessionToLog, buffer, bufferSize)

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
        memContextCallbackClear(objMemContext(this));

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

    FUNCTION_TEST_RETURN(INT, this->fd);
}

/**********************************************************************************************************************************/
static IoRead *
sckSessionIoRead(THIS_VOID, const bool ignoreUnexpectedEof)
{
    THIS(SocketSession);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SOCKET_SESSION, this);
        (void)ignoreUnexpectedEof;                                  // Unused
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(IO_READ, this->read);
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

    FUNCTION_TEST_RETURN(IO_WRITE, this->write);
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

    FUNCTION_TEST_RETURN(STRING_ID, this->role);
}

/**********************************************************************************************************************************/
static const IoSessionInterface sckSessionInterface =
{
    .type = IO_CLIENT_SOCKET_TYPE,
    .close = sckSessionClose,
    .fd = sckSessionFd,
    .ioRead = sckSessionIoRead,
    .ioWrite = sckSessionIoWrite,
    .role = sckSessionRole,
    .toLog = sckSessionToLog,
};

FN_EXTERN IoSession *
sckSessionNew(const IoSessionRole role, const int fd, const String *const host, const unsigned int port, const TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, role);
        FUNCTION_LOG_PARAM(INT, fd);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(fd != -1);
    ASSERT(host != NULL);

    OBJ_NEW_BEGIN(SocketSession, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        String *const name = strNewFmt("%s:%u", strZ(host), port);

        *this = (SocketSession)
        {
            .role = role,
            .fd = fd,
            .host = strDup(host),
            .port = port,
            .timeout = timeout,
            .read = ioFdReadNewOpen(name, fd, timeout),
            .write = ioFdWriteNewOpen(name, fd, timeout),
        };

        strFree(name);

        // Ensure file descriptor is closed
        memContextCallbackSet(objMemContext(this), sckSessionFreeResource, this);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_SESSION, ioSessionNew(this, &sckSessionInterface));
}
