/***********************************************************************************************************************************
TCP Functions
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef __FreeBSD__
#include <netinet/in.h>
#endif

#ifdef __linux__
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include "common/debug.h"
#include "common/log.h"
#include "common/io/socket/tcp.h"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct TcpLocal
{
    bool init;                                                      // tcpInit() has been called

    bool keepAlive;                                                 // Are keep alives enabled?
    int keepAliveCount;                                             // Keep alive count (0 disables)
    int keepAliveIdle;                                              // Keep alive idle (0 disables)
    int keepAliveInterval;                                          // Keep alive interval (0 disables)
} tcpLocal;

/**********************************************************************************************************************************/
void
tcpInit(bool keepAlive, int keepAliveCount, int keepAliveIdle, int keepAliveInterval)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, keepAlive);
        FUNCTION_LOG_PARAM(INT, keepAliveCount);
        FUNCTION_LOG_PARAM(INT, keepAliveIdle);
        FUNCTION_LOG_PARAM(INT, keepAliveInterval);
    FUNCTION_LOG_END();

    ASSERT(keepAliveCount >= 0);
    ASSERT(keepAliveIdle >= 0);
    ASSERT(keepAliveInterval >= 0);

    tcpLocal.init = true;
    tcpLocal.keepAlive = keepAlive;
    tcpLocal.keepAliveCount = keepAliveCount;
    tcpLocal.keepAliveIdle = keepAliveIdle;
    tcpLocal.keepAliveInterval = keepAliveInterval;

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
tcpOptionSet(int fd)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, fd);
    FUNCTION_TEST_END();

    ASSERT(tcpLocal.init);

    // Enable TCP keepalives
    if (tcpLocal.keepAlive)
    {
        int socketValue = 1;

        THROW_ON_SYS_ERROR(
            setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &socketValue, sizeof(int)) == -1, ProtocolError, "unable set SO_KEEPALIVE");

        // Set TCP_KEEPCNT when available
#ifdef TCP_KEEPIDLE
        if (tcpLocal.keepAliveCount > 0)
        {
            socketValue = tcpLocal.keepAliveCount;

            THROW_ON_SYS_ERROR(
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &socketValue, sizeof(int)) == -1, ProtocolError, "unable set TCP_KEEPCNT");
        }
#endif

        // Set TCP_KEEPIDLE when available
#ifdef TCP_KEEPIDLE
        if (tcpLocal.keepAliveIdle > 0)
        {
            socketValue = tcpLocal.keepAliveIdle;

            THROW_ON_SYS_ERROR(
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &socketValue, sizeof(int)) == -1, ProtocolError,
                "unable set SO_KEEPIDLE");
        }
#endif

    // Set TCP_KEEPINTVL when available
#ifdef TCP_KEEPIDLE
        if (tcpLocal.keepAliveInterval > 0)
        {
            socketValue = tcpLocal.keepAliveInterval;

            THROW_ON_SYS_ERROR(
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &socketValue, sizeof(int)) == -1, ProtocolError,
                "unable set SO_KEEPINTVL");
        }
#endif
    }

    FUNCTION_TEST_RETURN_VOID();
}
