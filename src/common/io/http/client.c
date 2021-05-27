/***********************************************************************************************************************************
HTTP Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/client.h"
#include "common/io/http/client.h"
#include "common/log.h"
#include "common/stat.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Statistics constants
***********************************************************************************************************************************/
STRING_EXTERN(HTTP_STAT_CLIENT_STR,                                 HTTP_STAT_CLIENT);
STRING_EXTERN(HTTP_STAT_CLOSE_STR,                                  HTTP_STAT_CLOSE);
STRING_EXTERN(HTTP_STAT_REQUEST_STR,                                HTTP_STAT_REQUEST);
STRING_EXTERN(HTTP_STAT_RETRY_STR,                                  HTTP_STAT_RETRY);
STRING_EXTERN(HTTP_STAT_SESSION_STR,                                HTTP_STAT_SESSION);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpClient
{
    HttpClientPub pub;                                              // Publicly accessible variables
    MemContext *memContext;                                         // Mem context
    IoClient *ioClient;                                             // Io client (e.g. TLS or socket client)

    List *sessionReuseList;                                         // List of HTTP sessions that can be reused
};

/**********************************************************************************************************************************/
HttpClient *
httpClientNew(IoClient *ioClient, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(IO_CLIENT, ioClient);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(ioClient != NULL);

    HttpClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpClient")
    {
        this = memNew(sizeof(HttpClient));

        *this = (HttpClient)
        {
            .pub =
            {
                .timeout = timeout,
            },
            .memContext = MEM_CONTEXT_NEW(),
            .ioClient = ioClient,
            .sessionReuseList = lstNewP(sizeof(HttpSession *)),
        };

        statInc(HTTP_STAT_CLIENT_STR);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_CLIENT, this);
}

/**********************************************************************************************************************************/
HttpSession *
httpClientOpen(HttpClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(HTTP_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    HttpSession *result = NULL;

    // Check if there is a resuable session
    if (!lstEmpty(this->sessionReuseList))
    {
        // Remove session from reusable list
        result = *(HttpSession **)lstGet(this->sessionReuseList, 0);
        lstRemoveIdx(this->sessionReuseList, 0);

        // Move session to the calling context
        httpSessionMove(result, memContextCurrent());
    }
    // Else create a new session
    else
    {
        result = httpSessionNew(this, ioClientOpen(this->ioClient));
        statInc(HTTP_STAT_SESSION_STR);
    }

    FUNCTION_LOG_RETURN(HTTP_SESSION, result);
}

/**********************************************************************************************************************************/
void
httpClientReuse(HttpClient *this, HttpSession *session)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(HTTP_CLIENT, this);
        FUNCTION_LOG_PARAM(HTTP_SESSION, session);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(session != NULL);

    httpSessionMove(session, lstMemContext(this->sessionReuseList));
    lstAdd(this->sessionReuseList, &session);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
httpClientToLog(const HttpClient *this)
{
    return strNewFmt(
        "{ioClient: %s, reusable: %u, timeout: %" PRIu64"}", strZ(ioClientToLog(this->ioClient)), lstSize(this->sessionReuseList),
        httpClientTimeout(this));
}
