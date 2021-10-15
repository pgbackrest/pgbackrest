/***********************************************************************************************************************************
Socket Server
***********************************************************************************************************************************/
#include "build.auto.h"

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/io/server.h"
#include "common/io/socket/server.h"
#include "common/io/socket/common.h"
#include "common/io/socket/session.h"
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
static String *
sckServerToLog(const THIS_VOID)
{
    THIS(const SocketServer);

    return strNewFmt("{address: %s, port: %u, timeout: %" PRIu64 "}", strZ(this->address), this->port, this->timeout);
}

#define FUNCTION_LOG_SOCKET_SERVER_TYPE                                                                                            \
    SocketServer *
#define FUNCTION_LOG_SOCKET_SERVER_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, sckServerToLog, buffer, bufferSize)

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

        THROW_ON_SYS_ERROR(serverSocket == -1, FileOpenError, "unable to accept socket");

        // Create socket session
        sckOptionSet(serverSocket);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = sckSessionNew(ioSessionRoleServer, serverSocket, this->address, this->port, this->timeout);
        }
        MEM_CONTEXT_PRIOR_END();

        statInc(SOCKET_STAT_SESSION_STR);
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

    FUNCTION_TEST_RETURN(this->name);                                                                               // {vm_covered}
}

/**********************************************************************************************************************************/
static const IoServerInterface sckServerInterface =
{
    .type = IO_SERVER_SOCKET_TYPE,
    .name = sckServerName,
    .accept = sckServerAccept,
    .toLog = sckServerToLog,
};

IoServer *
sckServerNew(const String *const address, const unsigned int port, const TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(STRING, address);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(address != NULL);
    ASSERT(port > 0);

    IoServer *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("SocketServer")
    {
        SocketServer *driver = memNew(sizeof(SocketServer));

        *driver = (SocketServer)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .address = strDup(address),
            .port = port,
            .name = strNewFmt("%s:%u", strZ(address), port),
            .timeout = timeout,
        };

        // Lookup address
        struct addrinfo *addressFound = sckHostLookup(driver->address, driver->port);

        TRY_BEGIN()
        {
            // Create socket
            THROW_ON_SYS_ERROR(
                (driver->socket = socket(addressFound->ai_family, SOCK_STREAM, 0)) == -1, FileOpenError, "unable to create socket");

            // Set the address as reusable so we can bind again quickly after a restart or crash
            int reuseAddr = 1;
            setsockopt(driver->socket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

            // Ensure file descriptor is closed
            memContextCallbackSet(driver->memContext, sckServerFreeResource, driver);

            // Bind the address
            THROW_ON_SYS_ERROR(
                bind(driver->socket, addressFound->ai_addr, addressFound->ai_addrlen) == -1, FileOpenError,
                "unable to bind socket");
        }
        FINALLY()
        {
            freeaddrinfo(addressFound);
        }
        TRY_END();

        // Listen for client connections. It might be a good idea to make the backlog configurable but this value seems OK for now.
        THROW_ON_SYS_ERROR(listen(driver->socket, 100) == -1, FileOpenError, "unable to listen on socket");

        statInc(SOCKET_STAT_SERVER_STR);

        this = ioServerNew(driver, &sckServerInterface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_SERVER, this);
}
