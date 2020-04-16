/***********************************************************************************************************************************
Socket Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/io/socket/client.h"
#include "common/io/socket/common.h"
#include "common/io/socket/session.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Statistics
***********************************************************************************************************************************/
static SocketClientStat sckClientStatLocal;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct SocketClient
{
    MemContext *memContext;                                         // Mem context
    String *host;                                                   // Hostname or IP address
    unsigned int port;                                              // Port to connect to host on
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)
};

OBJECT_DEFINE_MOVE(SOCKET_CLIENT);

OBJECT_DEFINE_GET(Host, const, SOCKET_CLIENT, const String *, host);
OBJECT_DEFINE_GET(Port, const, SOCKET_CLIENT, unsigned int, port);

/**********************************************************************************************************************************/
SocketClient *
sckClientNew(const String *host, unsigned int port, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);

    SocketClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("SocketClient")
    {
        this = memNew(sizeof(SocketClient));

        *this = (SocketClient)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .host = strDup(host),
            .port = port,
            .timeout = timeout,
        };

        sckClientStatLocal.object++;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(SOCKET_CLIENT, this);
}

/**********************************************************************************************************************************/
SocketSession *
sckClientOpen(SocketClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM(SOCKET_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    SocketSession *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        bool connected = false;
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

                if ((resultAddr = getaddrinfo(strPtr(this->host), port, &hints, &hostAddress)) != 0)
                {
                    THROW_FMT(
                        HostConnectError, "unable to get address for '%s': [%d] %s", strPtr(this->host), resultAddr,
                        gai_strerror(resultAddr));
                }

                // Connect to the host
                TRY_BEGIN()
                {
                    fd = socket(hostAddress->ai_family, hostAddress->ai_socktype, hostAddress->ai_protocol);
                    THROW_ON_SYS_ERROR(fd == -1, HostConnectError, "unable to create socket");

                    sckOptionSet(fd);

                    if (connect(fd, hostAddress->ai_addr, hostAddress->ai_addrlen) == -1)
                        THROW_SYS_ERROR_FMT(HostConnectError, "unable to connect to '%s:%u'", strPtr(this->host), this->port);
                }
                FINALLY()
                {
                    freeaddrinfo(hostAddress);
                }
                TRY_END();

                // Create the session
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = sckSessionNew(sckSessionTypeClient, fd, this->host, this->port, this->timeout);
                }
                MEM_CONTEXT_PRIOR_END();

                // Connection was successful
                connected = true;
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

                    sckClientStatLocal.retry++;
                }
            }
            TRY_END();
        }
        while (!connected && retry);

        if (!connected)
            RETHROW();

        sckClientStatLocal.session++;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(SOCKET_SESSION, result);
}

/**********************************************************************************************************************************/
String *
sckClientStatStr(void)
{
    FUNCTION_TEST_VOID();

    String *result = NULL;

    if (sckClientStatLocal.object > 0)
    {
        result = strNewFmt(
            "socket statistics: objects %" PRIu64 ", sessions %" PRIu64 ", retries %" PRIu64, sckClientStatLocal.object,
            sckClientStatLocal.session, sckClientStatLocal.retry);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
String *
sckClientToLog(const SocketClient *this)
{
    return strNewFmt("{host: %s, port: %u, timeout: %" PRIu64 "}", strPtr(this->host), this->port, this->timeout);
}
