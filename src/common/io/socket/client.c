/***********************************************************************************************************************************
Socket Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/error/retry.h"
#include "common/io/client.h"
#include "common/io/fd.h"
#include "common/io/socket/address.h"
#include "common/io/socket/client.h"
#include "common/io/socket/common.h"
#include "common/io/socket/session.h"
#include "common/log.h"
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
static void
sckClientToLog(const THIS_VOID, StringStatic *const debugLog)
{
    THIS(const SocketClient);

    strStcFmt(
        debugLog, "{host: %s, port: %u, timeoutConnect: %" PRIu64 ", timeoutSession: %" PRIu64 "}", strZ(this->host), this->port,
        this->timeoutConnect, this->timeoutSession);
}

#define FUNCTION_LOG_SOCKET_CLIENT_TYPE                                                                                            \
    SocketClient *
#define FUNCTION_LOG_SOCKET_CLIENT_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_OBJECT_FORMAT(value, sckClientToLog, buffer, bufferSize)

/**********************************************************************************************************************************/
static bool
sckConnectInProgress(int errNo)
{
    return errNo == EINPROGRESS || errNo == EINTR;
}

static bool
sckClientOpenWait(const int fd, int errNo, const TimeMSec timeout, const char *const name)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, fd);
        FUNCTION_LOG_PARAM(INT, errNo);
        FUNCTION_LOG_PARAM(STRINGZ, name);
    FUNCTION_LOG_END();

    bool result = false;

    // The connection has started but since we are in non-blocking mode it has not completed yet
    if (sckConnectInProgress(errNo))
    {
        // Wait for write-ready
        if (fdReadyWrite(fd, timeout))
        {
            // Check for success or error. If the connection was successful this will set errNo to 0.
            socklen_t errNoLen = sizeof(errNo);

            THROW_ON_SYS_ERROR_FMT(
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &errNo, &errNoLen) == -1, HostConnectError,
                "unable to get socket error for '%s'", name);

            // Connect success
            result = true;
        }
        // Clear error locally so we do not error below
        else
            errNo = 0;
    }

    // Throw error if it is still set
    if (errNo != 0)
    {
        THROW_SYS_ERROR_CODE_FMT(
            errNo, HostConnectError, "unable to connect to '%s'", name);
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

typedef struct SckClientOpenData
{
    const struct addrinfo *const address;                           // IP address
    const char *name;                                               // Combination of host, address, port
    int fd;                                                         // File descriptor for socket
    int errNo;                                                      // Current error (may just indicate to keep waiting)
} SckClientOpenData;

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
        Wait *const wait = waitNew(this->timeoutConnect);
        ErrorRetry *const errRetry = errRetryNew();
        bool retry;

        // Get an address list for the host and sort it
        AddressInfo *const addrInfo = addrInfoNew(this->host, this->port);
        addrInfoSort(addrInfo);

        // Try addresses until success or timeout
        List *const openDataList = lstNewP(sizeof(SckClientOpenData));
        volatile unsigned int addrInfoIdx = 0;

        do
        {
            // Assume there will be no retry
            SckClientOpenData *volatile openData = NULL;
            retry = false;

            // Try connection
            TRY_BEGIN()
            {
                // Iterate running connections and see if any of them have completed
                for (unsigned int openDataIdx = 0; openDataIdx < lstSize(openDataList); openDataIdx++)
                {
                    SckClientOpenData *const openDataWait = lstGet(openDataList, openDataIdx);

                    if (openDataWait->fd != 0 && // {uncovered - !!! ALSO PUT THIS BACK ON ONE LINE}
                        sckClientOpenWait(openData->fd, openData->errNo, 0, openData->name)) // {uncovered - !!!}
                    {
                        openData = openDataWait; // {uncovered - !!!}
                        openData->errNo = 0; // {uncovered - !!!}
                    }
                }

                // Try or retry a connection since none of the waiting connections completed
                if (openData == NULL)  // {uncovered - !!!}
                {
                    // If connection does not exist yet then create it
                    if (addrInfoIdx == lstSize(openDataList))
                    {
                        openData = lstAdd(openDataList, &(SckClientOpenData){.address = addrInfoGet(addrInfo, addrInfoIdx)->info});
                        openData->name = strZ(addrInfoToName(this->host, this->port, openData->address));
                    }
                    // Else search for a connection that is not waiting
                    else
                    {
                        for (; addrInfoIdx < lstSize(openDataList); addrInfoIdx++)
                        {
                            SckClientOpenData *const openDataNoWait = lstGet(openDataList, addrInfoIdx);

                            if (openDataNoWait->fd == 0)
                            {
                                openData = openDataNoWait;
                                break;
                            }
                        }

                        openData = lstGet(openDataList, addrInfoIdx);
                    }

                    // Attempt connection to host
                    if (openData != NULL) // {uncovered - !!!}
                    {
                        openData->fd = socket(
                            openData->address->ai_family, openData->address->ai_socktype, openData->address->ai_protocol);
                        THROW_ON_SYS_ERROR(openData->fd == -1, HostConnectError, "unable to create socket");

                        sckOptionSet(openData->fd);

                        if (connect(openData->fd, openData->address->ai_addr, openData->address->ai_addrlen) == -1)
                        {
                            // Save the error and wait for connection
                            openData->errNo = errno;

                            if (!sckClientOpenWait(openData->fd, openData->errNo, 250, openData->name))
                                THROW_FMT(HostConnectError, "timeout connecting to '%s'", openData->name);

                            openData->errNo = 0;
                        }
                    }
                }

                // Connection was successful so open session and return
                if (openData != NULL && openData->errNo == 0) // {uncovered - !!!}
                {
                    // Create the session
                    MEM_CONTEXT_PRIOR_BEGIN()
                    {
                        result = sckSessionNew(ioSessionRoleClient, openData->fd, this->host, this->port, this->timeoutSession);
                    }
                    MEM_CONTEXT_PRIOR_END();

                    // Update client name to include address
                    strTrunc(this->name);
                    strCatZ(this->name, openData->name);

                    // Set preferred address
                    addrInfoPrefer(addrInfo, addrInfoIdx);
                }
            }
            CATCH_ANY()
            {
                // Close socket
                ASSERT(openData != NULL);
                close(openData->fd);

                // Clear connection so it can be retried
                openData->fd = 0;
                openData = NULL;

                // Add the error retry info
                errRetryAdd(errRetry);
            }
            TRY_END();

            // If the connection errored or all connections are waiting then wait and/or retry
            if (openData == NULL)
            {
                // Increment address info index and reset if the end has been reached. When the end has been reached sleep for a bit
                // to hopefully have better chance at succeeding, otherwise continue right to the next address.
                addrInfoIdx++;

                if (addrInfoIdx >= addrInfoSize(addrInfo))
                {
                    addrInfoIdx = 0;
                    retry = waitMore(wait);
                }
                else
                    retry = true;

                // Error when no retries remain
                if (!retry)
                    THROWP(errRetryType(errRetry), strZ(errRetryMessage(errRetry)));

                // Log retry
                LOG_DEBUG_FMT("retry %s: %s", errorTypeName(errorType()), errorMessage());
                statInc(SOCKET_STAT_RETRY_STR);
            }
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

    FUNCTION_TEST_RETURN_CONST(STRING, this->name);
}

/**********************************************************************************************************************************/
static const IoClientInterface sckClientInterface =
{
    .type = IO_CLIENT_SOCKET_TYPE,
    .name = sckClientName,
    .open = sckClientOpen,
    .toLog = sckClientToLog,
};

FN_EXTERN IoClient *
sckClientNew(const String *const host, const unsigned int port, const TimeMSec timeoutConnect, const TimeMSec timeoutSession)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutConnect);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutSession);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);

    OBJ_NEW_BEGIN(SocketClient, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (SocketClient)
        {
            .host = strDup(host),
            .port = port,
            .name = strCatFmt(strNew(), "%s:%u", strZ(host), port),
            .timeoutConnect = timeoutConnect,
            .timeoutSession = timeoutSession,
        };
    }
    OBJ_NEW_END();

    statInc(SOCKET_STAT_CLIENT_STR);

    FUNCTION_LOG_RETURN(IO_CLIENT, ioClientNew(this, &sckClientInterface));
}
