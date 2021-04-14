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
#include "common/memContext.h"
#include "common/stat.h"
#include "common/type/object.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Io client type
***********************************************************************************************************************************/
STRING_EXTERN(IO_CLIENT_SOCKET_TYPE_STR,                            IO_CLIENT_SOCKET_TYPE);

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
    MemContext *memContext;                                         // Mem context
    String *host;                                                   // Hostname or IP address
    unsigned int port;                                              // Port to connect to host on
    String *name;                                                   // Socket name (host:port)
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)
} SocketClient;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static String *
sckClientToLog(const THIS_VOID)
{
    THIS(const SocketClient);

    return strNewFmt("{host: %s, port: %u, timeout: %" PRIu64 "}", strZ(this->host), this->port, this->timeout);
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
        Wait *wait = waitNew(this->timeout);

        do
        {
            // Assume there will be no retry
            retry = false;
            int fd = -1;

            TRY_BEGIN()
            {
                // Set hints that narrow the type of address we are looking for -- we'll take ipv4 or ipv6
                struct addrinfo hints = (struct addrinfo)
                {
                    .ai_family = AF_UNSPEC,
                    .ai_socktype = SOCK_STREAM,
                    .ai_protocol = IPPROTO_TCP,
                };

                // Convert the port to a zero-terminated string for use with getaddrinfo()
                char port[CVT_BASE10_BUFFER_SIZE];
                cvtUIntToZ(this->port, port, sizeof(port));

                // Get an address for the host.  We are only going to try the first address returned.
                struct addrinfo *hostAddress;
                int resultAddr;

                if ((resultAddr = getaddrinfo(strZ(this->host), port, &hints, &hostAddress)) != 0)
                {
                    THROW_FMT(
                        HostConnectError, "unable to get address for '%s': [%d] %s", strZ(this->host), resultAddr,
                        gai_strerror(resultAddr));
                }

                // Connect to the host
                TRY_BEGIN()
                {
                    fd = socket(hostAddress->ai_family, hostAddress->ai_socktype, hostAddress->ai_protocol);
                    THROW_ON_SYS_ERROR(fd == -1, HostConnectError, "unable to create socket");

                    sckOptionSet(fd);
                    sckConnect(fd, this->host, this->port, hostAddress, waitRemaining(wait));
                }
                FINALLY()
                {
                    freeaddrinfo(hostAddress);
                }
                TRY_END();

                // Create the session
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = sckSessionNew(ioSessionRoleClient, fd, this->host, this->port, this->timeout);
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
    .type = &IO_CLIENT_SOCKET_TYPE_STR,
    .name = sckClientName,
    .open = sckClientOpen,
    .toLog = sckClientToLog,
};

IoClient *
sckClientNew(const String *host, unsigned int port, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);

    IoClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("SocketClient")
    {
        SocketClient *driver = memNew(sizeof(SocketClient));

        *driver = (SocketClient)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .host = strDup(host),
            .port = port,
            .name = strNewFmt("%s:%u", strZ(host), port),
            .timeout = timeout,
        };

        statInc(SOCKET_STAT_CLIENT_STR);

        this = ioClientNew(driver, &sckClientInterface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_CLIENT, this);
}
