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

    // Put the socket in non-blocking mode
    int flags;

    THROW_ON_SYS_ERROR((flags = fcntl(fd, F_GETFL)) == -1, ProtocolError, "unable to get flags");
    THROW_ON_SYS_ERROR(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1, ProtocolError, "unable to set O_NONBLOCK");

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


bool
sckPoll(int fd, bool read, bool write, uint64_t timeout)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, fd);
        FUNCTION_LOG_PARAM(BOOL, read);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(UINT64, timeout);
    FUNCTION_LOG_END();

    ASSERT(fd != -1);
    ASSERT(read || write);

// 	/* We use poll(2) if available, otherwise select(2) */
// #ifdef HAVE_POLL
	struct pollfd input_fd = {.fd = fd, .events = POLLERR};

	if (read)
		input_fd.events |= POLLIN;

	if (write)
		input_fd.events |= POLLOUT;

    int result = 0;

    do
    {
        result = poll(&input_fd, 1, (int)timeout);

        if (result == -1 && errno != EINTR)
            THROW_ON_SYS_ERROR_FMT(result == -1, KernelError, "unable to poll socket");
    }
    while (result == -1);

    FUNCTION_LOG_RETURN(BOOL, result != 0);
// #else							/* !HAVE_POLL */
//
// 	fd_set		input_mask;
// 	fd_set		output_mask;
// 	fd_set		except_mask;
// 	struct timeval timeout;
// 	struct timeval *ptr_timeout;
//
// 	if (!forRead && !forWrite)
// 		return 0;
//
// 	FD_ZERO(&input_mask);
// 	FD_ZERO(&output_mask);
// 	FD_ZERO(&except_mask);
// 	if (forRead)
// 		FD_SET(sock, &input_mask);
//
// 	if (forWrite)
// 		FD_SET(sock, &output_mask);
// 	FD_SET(sock, &except_mask);
//
// 	/* Compute appropriate timeout interval */
// 	if (end_time == ((time_t) -1))
// 		ptr_timeout = NULL;
// 	else
// 	{
// 		time_t		now = time(NULL);
//
// 		if (end_time > now)
// 			timeout.tv_sec = end_time - now;
// 		else
// 			timeout.tv_sec = 0;
// 		timeout.tv_usec = 0;
// 		ptr_timeout = &timeout;
// 	}
//
// 	return select(sock + 1, &input_mask, &output_mask,
// 				  &except_mask, ptr_timeout);
// #endif							/* HAVE_POLL */
}
