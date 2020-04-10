/***********************************************************************************************************************************
Socket Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/io/socket/client.h"
#include "common/io/socket/common.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct SocketSession
{
    MemContext *memContext;                                         // Mem context
    SocketSessionType type;                                         // Type (server or client)
    String *host;                                                   // Hostname or IP address
    unsigned int port;                                              // Port to connect to host on
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)

    int fd;                                                         // File descriptor
};

OBJECT_DEFINE_MOVE(SOCKET_SESSION);

OBJECT_DEFINE_GET(Fd, , SOCKET_SESSION, int, fd);
OBJECT_DEFINE_GET(Host, const, SOCKET_SESSION, const String *, host);
OBJECT_DEFINE_GET(Port, const, SOCKET_SESSION, unsigned int, port);

OBJECT_DEFINE_FREE(SOCKET_SESSION);

/***********************************************************************************************************************************
Free connection
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(SOCKET_SESSION, LOG, logLevelTrace)
{
    close(this->fd);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/**********************************************************************************************************************************/
SocketSession *
sckSessionNew(SocketSessionType type, int fd, const String *host, unsigned int port, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(INT, fd);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(fd != -1);
    ASSERT(host != NULL);
    ASSERT(timeout > 0);

    SocketSession *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("SocketSession")
    {
        this = memNew(sizeof(SocketSession));

        *this = (SocketSession)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .type = type,
            .host = strDup(host),
            .port = port,
            .timeout = timeout,
            .fd = fd,
        };

        memContextCallbackSet(this->memContext, sckSessionFreeResource, this);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(SOCKET_SESSION, this);
}

/**********************************************************************************************************************************/
void
sckSessionReady(SocketSession *this, bool read, bool write)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SOCKET_SESSION, this);
        FUNCTION_LOG_PARAM(BOOL, read);
        FUNCTION_LOG_PARAM(BOOL, write);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->fd != -1);

    if (!sckReady(this->fd, read, write, this->timeout))
    {
        THROW_FMT(
            FileReadError, "timeout after %" PRIu64 "ms waiting for %s%s%s from '%s:%u'", this->timeout, read ? "read" : "",
            read && write ? "/" : "", write ? "write" : "", strPtr(this->host), this->port);
    }

    FUNCTION_LOG_RETURN_VOID();
}

void
sckSessionReadyRead(SocketSession *this)
{
    sckSessionReady(this, true, false);
}

void
sckSessionReadyWrite(SocketSession *this)
{
    sckSessionReady(this, false, true);
}

/**********************************************************************************************************************************/
String *
sckSessionToLog(const SocketSession *this)
{
    return strNewFmt(
        "{type: %s, fd %d, host: %s, port: %u, timeout: %" PRIu64 "}", this->type == sckSessionTypeClient ? "client" : "server",
        this->fd, strPtr(this->host), this->port, this->timeout);
}
