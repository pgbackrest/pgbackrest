/***********************************************************************************************************************************
Socket Common Functions
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "common/debug.h"
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

    bool keepAlive;                                                 // Are socket keep alives enabled?
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
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &socketValue, sizeof(int)) == -1, ProtocolError, "unable set TCP_NODELAY");
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
	THROW_ON_SYS_ERROR(fcntl(fd, F_SETFD, FD_CLOEXEC) == -1, ProtocolError, "unable set FD_CLOEXEC");
#endif

    // Enable TCP keepalives
    if (socketLocal.keepAlive)
    {
        int socketValue = 1;

        THROW_ON_SYS_ERROR(
            setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &socketValue, sizeof(int)) == -1, ProtocolError, "unable set SO_KEEPALIVE");

        // Set TCP_KEEPCNT when available
#ifdef TCP_KEEPIDLE
        if (socketLocal.tcpKeepAliveCount > 0)
        {
            socketValue = socketLocal.tcpKeepAliveCount;

            THROW_ON_SYS_ERROR(
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &socketValue, sizeof(int)) == -1, ProtocolError, "unable set TCP_KEEPCNT");
        }
#endif

        // Set TCP_KEEPIDLE when available
#ifdef TCP_KEEPIDLE
        if (socketLocal.tcpKeepAliveIdle > 0)
        {
            socketValue = socketLocal.tcpKeepAliveIdle;

            THROW_ON_SYS_ERROR(
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &socketValue, sizeof(int)) == -1, ProtocolError,
                "unable set SO_KEEPIDLE");
        }
#endif

    // Set TCP_KEEPINTVL when available
#ifdef TCP_KEEPIDLE
        if (socketLocal.tcpKeepAliveInterval > 0)
        {
            socketValue = socketLocal.tcpKeepAliveInterval;

            THROW_ON_SYS_ERROR(
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &socketValue, sizeof(int)) == -1, ProtocolError,
                "unable set SO_KEEPINTVL");
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
            if (!sckReadyWrite(fd, timeout))
                THROW_FMT(HostConnectError, "timeout connecting to '%s:%u'", strPtr(host), port);

            // Check for success or error. If the connection was successful this will set errNo to 0.
            socklen_t errNoLen = sizeof(errNo);

            THROW_ON_SYS_ERROR(
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &errNo, &errNoLen) == -1, HostConnectError, "unable to get socket error");
        }

        // Throw error if it is still set
        if (errNo != 0)
            THROW_SYS_ERROR_CODE_FMT(errNo, HostConnectError, "unable to connect to '%s:%u'", strPtr(host), port);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Use poll() to determine when data is ready to read/write on a socket. Retry after EINTR with whatever time is left on the timer.
***********************************************************************************************************************************/
// Helper to determine when poll() should be retried
static bool
sckReadyRetry(int pollResult, int errNo, bool first, TimeMSec *timeout, TimeMSec timeEnd)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, pollResult);
        FUNCTION_TEST_PARAM(INT, errNo);
        FUNCTION_TEST_PARAM(BOOL, first);
        FUNCTION_TEST_PARAM_P(TIME_MSEC, timeout);
        FUNCTION_TEST_PARAM(TIME_MSEC, timeEnd);
    FUNCTION_TEST_END();

    ASSERT(timeout != NULL);

    // No retry by default
    bool result = false;

    // Process errors
    if (pollResult == -1)
    {
        // Don't error on an interrupt. If the interrupt lasts long enough there may be a timeout, though.
        if (errNo != EINTR)
            THROW_SYS_ERROR_CODE(errNo, KernelError, "unable to poll socket");

        // Always retry on the first iteration
        if (first)
        {
            result = true;
        }
        // Else retry if there is time left
        else
        {
            TimeMSec timeCurrent = timeMSec();

            if (timeEnd > timeCurrent)
            {
                *timeout = timeEnd - timeCurrent;
                result = true;
            }
        }
    }

    FUNCTION_TEST_RETURN(result);
}

bool
sckReady(int fd, bool read, bool write, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, fd);
        FUNCTION_LOG_PARAM(BOOL, read);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(fd >= 0);
    ASSERT(read || write);
    ASSERT(timeout < INT_MAX);

    // Poll settings
    struct pollfd inputFd = {.fd = fd};

    if (read)
        inputFd.events |= POLLIN;

    if (write)
        inputFd.events |= POLLOUT;

    // Wait for ready or timeout
    TimeMSec timeEnd = timeMSec() + timeout;
    bool first = true;

    // Initialize result and errno to look like a retryable error. We have no good way to test this function with interrupts so this
    // at least ensures that the condition is retried.
    int result = -1;
    int errNo = EINTR;

    while (sckReadyRetry(result, errNo, first, &timeout, timeEnd))
    {
        result = poll(&inputFd, 1, (int)timeout);

        errNo = errno;
        first = false;
    }

    FUNCTION_LOG_RETURN(BOOL, result > 0);
}

bool
sckReadyRead(int fd, TimeMSec timeout)
{
    return sckReady(fd, true, false, timeout);
}

bool
sckReadyWrite(int fd, TimeMSec timeout)
{
    return sckReady(fd, false, true, timeout);
}
