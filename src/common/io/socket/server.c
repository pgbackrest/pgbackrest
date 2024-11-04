/***********************************************************************************************************************************
Socket Server
***********************************************************************************************************************************/
#include "build.auto.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/server.h"
#include "common/io/socket/address.h"
#include "common/io/socket/common.h"
#include "common/io/socket/server.h"
#include "common/io/socket/session.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/stat.h"
#include "common/type/object.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Statistics constants
***********************************************************************************************************************************/
STRING_EXTERN(SOCKET_STAT_SERVER_STR,                               SOCKET_STAT_SERVER);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct SocketServer
{
    MemContext *memContext;                                         // Mem context
    String *address;                                                // Address to listen on
    unsigned int port;                                              // Port to listen on
    int socket;                                                     // Socket used to listen for new connections
    String *name;                                                   // Socket name (address:port)
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)
} SocketServer;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static void
sckServerToLog(const THIS_VOID, StringStatic *const debugLog)
{
    THIS(const SocketServer);

    strStcFmt(debugLog, "{address: %s, port: %u, timeout: %" PRIu64 "}", strZ(this->address), this->port, this->timeout);
}

#define FUNCTION_LOG_SOCKET_SERVER_TYPE                                                                                            \
    SocketServer *
#define FUNCTION_LOG_SOCKET_SERVER_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_OBJECT_FORMAT(value, sckServerToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free connection
***********************************************************************************************************************************/
static void
sckServerFreeResource(THIS_VOID)
{
    THIS(SocketServer);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SOCKET_SERVER, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    close(this->socket);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static IoSession *
sckServerAccept(THIS_VOID, IoSession *const session)
{
    THIS(SocketServer);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SOCKET_SERVER, this);
        (void)session;                                              // Not used by this server
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(session == NULL);

    IoSession *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Accept the socket connection
        struct sockaddr_in addr;
        unsigned int len = sizeof(addr);

        int serverSocket = accept(this->socket, (struct sockaddr *)&addr, &len);

        if (serverSocket != -1)
        {
            // Create socket session
            sckOptionSet(serverSocket);

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result = sckSessionNew(ioSessionRoleServer, serverSocket, this->address, this->port, this->timeout);
            }
            MEM_CONTEXT_PRIOR_END();

            statInc(SOCKET_STAT_SESSION_STR);
        }
        else
            THROW_ON_SYS_ERROR(errno != EINTR, FileOpenError, "unable to accept socket");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(IO_SESSION, result);
}

/**********************************************************************************************************************************/
static const String *
sckServerName(THIS_VOID)                                                                                            // {vm_covered}
{
    THIS(SocketServer);                                                                                             // {vm_covered}

    FUNCTION_TEST_BEGIN();                                                                                          // {vm_covered}
        FUNCTION_TEST_PARAM(SOCKET_SERVER, this);                                                                   // {vm_covered}
    FUNCTION_TEST_END();                                                                                            // {vm_covered}

    ASSERT(this != NULL);                                                                                           // {vm_covered}

    FUNCTION_TEST_RETURN_CONST(STRING, this->name);                                                                 // {vm_covered}
}

/**********************************************************************************************************************************/
static const IoServerInterface sckServerInterface =
{
    .type = IO_SERVER_SOCKET_TYPE,
    .name = sckServerName,
    .accept = sckServerAccept,
    .toLog = sckServerToLog,
};

FN_EXTERN IoServer *
sckServerNew(const String *const address, const unsigned int port, const TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, address);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(address != NULL);
    ASSERT(port > 0);

    OBJ_NEW_BEGIN(SocketServer, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (SocketServer)
        {
            .address = strDup(address),
            .port = port,
            .name = strCatFmt(strNew(), "%s:%u", strZ(address), port),
            .timeout = timeout,
        };

        // Lookup address
        const struct addrinfo *const addressFound = addrInfoGet(addrInfoNew(this->address, this->port), 0)->info;

        // Create socket
        THROW_ON_SYS_ERROR(
            (this->socket = socket(addressFound->ai_family, SOCK_STREAM, 0)) == -1, FileOpenError, "unable to create socket");

        // Set the address as reusable so we can bind again quickly after a restart or crash
        int reuseAddr = 1;

        THROW_ON_SYS_ERROR(
            setsockopt(this->socket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr)) == -1, ProtocolError,
            "unable to set SO_REUSEADDR");

        // Ensure file descriptor is closed
        memContextCallbackSet(objMemContext(this), sckServerFreeResource, this);

        // Update server name to include address
        strTrunc(this->name);
        strCat(this->name, addrInfoToName(this->address, this->port, addressFound));

        // Bind the address
        Wait *const wait = waitNew(timeout);
        int result;

        do
        {
            result = bind(this->socket, addressFound->ai_addr, addressFound->ai_addrlen);

            if (result == -1 && !waitMore(wait))
                THROW_SYS_ERROR(FileOpenError, "unable to bind socket");
        }
        while (result == -1);

        // Listen for client connections. It might be a good idea to make the backlog configurable but this value seems OK for now.
        THROW_ON_SYS_ERROR(listen(this->socket, 100) == -1, FileOpenError, "unable to listen on socket");
    }
    OBJ_NEW_END();

    statInc(SOCKET_STAT_SERVER_STR);

    FUNCTION_LOG_RETURN(IO_SERVER, ioServerNew(this, &sckServerInterface));
}
