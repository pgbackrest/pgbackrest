/***********************************************************************************************************************************
Socket Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/io/client.h"
#include "common/io/socket/client.h"
#include "common/io/socket/common.h"
#include "common/io/socket/session.h"
#include "common/stat.h"
#include "common/type/object.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Statistics constants
***********************************************************************************************************************************/
STRING_EXTERN(SOCKET_STAT_CLIENT_STR,                               SOCKET_STAT_CLIENT);
STRING_EXTERN(SOCKET_STAT_RETRY_STR,                                SOCKET_STAT_RETRY);
STRING_EXTERN(SOCKET_STAT_SESSION_STR,                              SOCKET_STAT_SESSION);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct SocketClient
{
    String *host;                                                   // Hostname or IP address
    unsigned int port;                                              // Port to connect to host on
    String *name;                                                   // Socket name (host:port)
    TimeMSec timeoutConnect;                                        // Timeout for connection
    TimeMSec timeoutSession;                                        // Timeout passed to session
} SocketClient;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static String *
sckClientToLog(const THIS_VOID)
{
    THIS(const SocketClient);

    return strNewFmt(
        "{host: %s, port: %u, timeoutConnect: %" PRIu64 ", timeoutSession: %" PRIu64 "}", strZ(this->host), this->port,
        this->timeoutConnect, this->timeoutSession);
}

#define FUNCTION_LOG_SOCKET_CLIENT_TYPE                                                                                            \
    SocketClient *
#define FUNCTION_LOG_SOCKET_CLIENT_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, sckClientToLog, buffer, bufferSize)

/**********************************************************************************************************************************/
static IoSession *
sckClientOpen(THIS_VOID)
{
    THIS(SocketClient);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(SOCKET_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    IoSession *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        bool retry;
        Wait *wait = waitNew(this->timeoutConnect);

        do
        {
            // Assume there will be no retry
            retry = false;
            int fd = -1;

            TRY_BEGIN()
            {
                // Get an address for the host. We are only going to try the first address returned.
                struct addrinfo *addressFound = sckHostLookup(this->host, this->port);

                // Connect to the host
                TRY_BEGIN()
                {
                    fd = socket(addressFound->ai_family, addressFound->ai_socktype, addressFound->ai_protocol);
                    THROW_ON_SYS_ERROR(fd == -1, HostConnectError, "unable to create socket");

                    sckOptionSet(fd);
                    sckConnect(fd, this->host, this->port, addressFound, waitRemaining(wait));
                }
                FINALLY()
                {
                    freeaddrinfo(addressFound);
                }
                TRY_END();

                // Create the session
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = sckSessionNew(ioSessionRoleClient, fd, this->host, this->port, this->timeoutSession);
                }
                MEM_CONTEXT_PRIOR_END();
            }
            CATCH_ANY()
            {
                if (fd != -1)
                    close(fd);

                // Retry if wait time has not expired
                if (waitMore(wait))
                {
                    LOG_DEBUG_FMT("retry %s: %s", errorTypeName(errorType()), errorMessage());
                    retry = true;

                    statInc(SOCKET_STAT_RETRY_STR);
                }
                else
                    RETHROW();
            }
            TRY_END();
        }
        while (retry);

        statInc(SOCKET_STAT_SESSION_STR);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(IO_SESSION, result);
}

/**********************************************************************************************************************************/
static const String *
sckClientName(THIS_VOID)
{
    THIS(SocketClient);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SOCKET_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->name);
}

/**********************************************************************************************************************************/
static const IoClientInterface sckClientInterface =
{
    .type = IO_CLIENT_SOCKET_TYPE,
    .name = sckClientName,
    .open = sckClientOpen,
    .toLog = sckClientToLog,
};

IoClient *
sckClientNew(const String *const host, const unsigned int port, const TimeMSec timeoutConnect, const TimeMSec timeoutSession)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutConnect);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutSession);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);

    IoClient *this = NULL;

    OBJ_NEW_BEGIN(SocketClient)
    {
        SocketClient *driver = OBJ_NEW_ALLOC();

        *driver = (SocketClient)
        {
            .host = strDup(host),
            .port = port,
            .name = strNewFmt("%s:%u", strZ(host), port),
            .timeoutConnect = timeoutConnect,
            .timeoutSession = timeoutSession,
        };

        statInc(SOCKET_STAT_CLIENT_STR);

        this = ioClientNew(driver, &sckClientInterface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_CLIENT, this);
}
