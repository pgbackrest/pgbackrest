/***********************************************************************************************************************************
Socket Common Functions
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "common/debug.h"
#include "common/io/socket/address.h"
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
FN_EXTERN void
sckInit(
    const bool block, const bool keepAlive, const int tcpKeepAliveCount, const int tcpKeepAliveIdle, const int tcpKeepAliveInterval)
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
FN_EXTERN void
sckOptionSet(const int fd)
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
