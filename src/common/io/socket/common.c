/***********************************************************************************************************************************
Socket Common Functions
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <poll.h>

#ifdef __FreeBSD__
#include <netinet/in.h>
#endif

#ifdef __linux__
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

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

    bool keepAlive;                                                 // Are socket keep alives enabled?
    int tcpKeepAliveCount;                                          // TCP keep alive count (0 disables)
    int tcpKeepAliveIdle;                                           // TCP keep alive idle (0 disables)
    int tcpKeepAliveInterval;                                       // TCP keep alive interval (0 disables)
} socketLocal;

/**********************************************************************************************************************************/
void
sckInit(bool keepAlive, int tcpKeepAliveCount, int tcpKeepAliveIdle, int tcpKeepAliveInterval)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, keepAlive);
        FUNCTION_LOG_PARAM(INT, tcpKeepAliveCount);
        FUNCTION_LOG_PARAM(INT, tcpKeepAliveIdle);
        FUNCTION_LOG_PARAM(INT, tcpKeepAliveInterval);
    FUNCTION_LOG_END();

    ASSERT(tcpKeepAliveCount >= 0);
    ASSERT(tcpKeepAliveIdle >= 0);
    ASSERT(tcpKeepAliveInterval >= 0);

    socketLocal.init = true;
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
sckReadyRetry(int pollResult, int errNo, bool first, Wait *wait)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, pollResult);
        FUNCTION_LOG_PARAM(INT, errNo);
        FUNCTION_LOG_PARAM(BOOL, first);
        FUNCTION_LOG_PARAM(WAIT, wait);
    FUNCTION_LOG_END();

    bool result = false;

    // Process errors
    if (pollResult == -1)
    {
        // Don't error on an interrupt. If the interrupt lasts long enough there may be a timeout, though.
        if (errNo != EINTR)
            THROW_SYS_ERROR_CODE(errNo, KernelError, "unable to poll socket");

        // Retry if first iteration or time remaining
        result = first || waitMore(wait);
    }

    // Poll returned a result so no retry
    FUNCTION_LOG_RETURN(BOOL, result);
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

    ASSERT(fd != -1);
    ASSERT(read || write);
    ASSERT(timeout < INT_MAX);

    // Poll settings
    struct pollfd inputFd = {.fd = fd, .events = POLLERR};

    if (read)
        inputFd.events |= POLLIN;

    if (write)
        inputFd.events |= POLLOUT;

    // Wait for ready or timeout
    bool first = true;
    Wait *wait = waitNew(timeout);
    int result = -1;
    int errNo = EINTR;

    while (sckReadyRetry(result, errNo, first, wait))
    {
        result = poll(&inputFd, 1, (int)waitRemaining(wait));

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
