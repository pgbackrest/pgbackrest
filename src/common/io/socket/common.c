/***********************************************************************************************************************************
Socket Common Functions
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "common/debug.h"
#include "common/io/fd.h"
#include "common/io/socket/common.h"
#include "common/log.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct SocketLocal
{
    bool init;                                                      // sckInit() has been called

    bool block;                                                     // Use blocking mode socket

    bool keepAlive;                                                 // Is socket keep-alive enabled?
    int tcpKeepAliveCount;                                          // TCP keep alive count (0 disables)
    int tcpKeepAliveIdle;                                           // TCP keep alive idle (0 disables)
    int tcpKeepAliveInterval;                                       // TCP keep alive interval (0 disables)
} socketLocal;

/**********************************************************************************************************************************/
void
sckInit(bool block, bool keepAlive, int tcpKeepAliveCount, int tcpKeepAliveIdle, int tcpKeepAliveInterval)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, block);
        FUNCTION_LOG_PARAM(BOOL, keepAlive);
        FUNCTION_LOG_PARAM(INT, tcpKeepAliveCount);
        FUNCTION_LOG_PARAM(INT, tcpKeepAliveIdle);
        FUNCTION_LOG_PARAM(INT, tcpKeepAliveInterval);
    FUNCTION_LOG_END();

    ASSERT(tcpKeepAliveCount >= 0);
    ASSERT(tcpKeepAliveIdle >= 0);
    ASSERT(tcpKeepAliveInterval >= 0);

    socketLocal.init = true;
    socketLocal.block = block;
    socketLocal.keepAlive = keepAlive;
    socketLocal.tcpKeepAliveCount = tcpKeepAliveCount;
    socketLocal.tcpKeepAliveIdle = tcpKeepAliveIdle;
    socketLocal.tcpKeepAliveInterval = tcpKeepAliveInterval;

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
struct addrinfo *
sckHostLookup(const String *const host, unsigned int port)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);
    ASSERT(port != 0);

    // Set hints that narrow the type of address we are looking for -- we'll take ipv4 or ipv6
    struct addrinfo hints = (struct addrinfo)
    {
        .ai_family = AF_UNSPEC,
        .ai_flags = AI_PASSIVE,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    };

    // Convert the port to a zero-terminated string for use with getaddrinfo()
    char portZ[CVT_BASE10_BUFFER_SIZE];
    cvtUIntToZ(port, portZ, sizeof(portZ));

    // Do the lookup
    struct addrinfo *result;
    int error;

    if ((error = getaddrinfo(strZ(host), portZ, &hints, &result)) != 0)
        THROW_FMT(HostConnectError, "unable to get address for '%s': [%d] %s", strZ(host), error, gai_strerror(error));

    FUNCTION_LOG_RETURN_P(VOID, result);
}

/**********************************************************************************************************************************/
void
sckOptionSet(int fd)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, fd);
    FUNCTION_TEST_END();

    ASSERT(socketLocal.init);

    // Disable the Nagle algorithm. This means that segments are always sent as soon as possible, even if there is only a small
    // amount of data. Our internal buffering minimizes the benefit of this optimization so lower latency is preferred.
#ifdef TCP_NODELAY
    int socketValue = 1;

    THROW_ON_SYS_ERROR(
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &socketValue, sizeof(int)) == -1, ProtocolError, "unable to set TCP_NODELAY");
#endif

    // Put the socket in non-blocking mode
    if (!socketLocal.block)
    {
        int flags;

        THROW_ON_SYS_ERROR((flags = fcntl(fd, F_GETFL)) == -1, ProtocolError, "unable to get flags");
        THROW_ON_SYS_ERROR(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1, ProtocolError, "unable to set O_NONBLOCK");
    }

    // Automatically close the socket (in the child process) on a successful execve() call. Connections are never shared between
    // processes so there is no reason to leave them open.
#ifdef F_SETFD
	THROW_ON_SYS_ERROR(fcntl(fd, F_SETFD, FD_CLOEXEC) == -1, ProtocolError, "unable to set FD_CLOEXEC");
#endif

    // Enable TCP keepalives
    if (socketLocal.keepAlive)
    {
        int socketValue = 1;

        THROW_ON_SYS_ERROR(
            setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &socketValue, sizeof(int)) == -1, ProtocolError, "unable to set SO_KEEPALIVE");

        // Set TCP_KEEPCNT when available
#ifdef TCP_KEEPIDLE
        if (socketLocal.tcpKeepAliveCount > 0)
        {
            socketValue = socketLocal.tcpKeepAliveCount;

            THROW_ON_SYS_ERROR(
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &socketValue, sizeof(int)) == -1, ProtocolError,
                "unable to set TCP_KEEPCNT");
        }
#endif

        // Set TCP_KEEPIDLE when available
#ifdef TCP_KEEPIDLE
        if (socketLocal.tcpKeepAliveIdle > 0)
        {
            socketValue = socketLocal.tcpKeepAliveIdle;

            THROW_ON_SYS_ERROR(
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &socketValue, sizeof(int)) == -1, ProtocolError,
                "unable to set SO_KEEPIDLE");
        }
#endif

    // Set TCP_KEEPINTVL when available
#ifdef TCP_KEEPIDLE
        if (socketLocal.tcpKeepAliveInterval > 0)
        {
            socketValue = socketLocal.tcpKeepAliveInterval;

            THROW_ON_SYS_ERROR(
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &socketValue, sizeof(int)) == -1, ProtocolError,
                "unable to set SO_KEEPINTVL");
        }
#endif
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static bool
sckConnectInProgress(int errNo)
{
    return errNo == EINPROGRESS || errNo == EINTR;
}

void
sckConnect(int fd, const String *host, unsigned int port, const struct addrinfo *hostAddress, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, fd);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM_P(VOID, hostAddress);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);
    ASSERT(hostAddress != NULL);

    // Attempt connection
    if (connect(fd, hostAddress->ai_addr, hostAddress->ai_addrlen) == -1)
    {
        // Save the error
        int errNo = errno;

        // The connection has started but since we are in non-blocking mode it has not completed yet
        if (sckConnectInProgress(errNo))
        {
            // Wait for write-ready
            if (!fdReadyWrite(fd, timeout))
                THROW_FMT(HostConnectError, "timeout connecting to '%s:%u'", strZ(host), port);

            // Check for success or error. If the connection was successful this will set errNo to 0.
            socklen_t errNoLen = sizeof(errNo);

            THROW_ON_SYS_ERROR(
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &errNo, &errNoLen) == -1, HostConnectError, "unable to get socket error");
        }

        // Throw error if it is still set
        if (errNo != 0)
            THROW_SYS_ERROR_CODE_FMT(errNo, HostConnectError, "unable to connect to '%s:%u'", strZ(host), port);
    }

    FUNCTION_LOG_RETURN_VOID();
}
