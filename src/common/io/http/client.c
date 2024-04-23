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
    IoClient *ioClient;                                             // Io client (e.g. TLS or socket client)

    List *sessionReuseList;                                         // List of HTTP sessions that can be reused
};

/**********************************************************************************************************************************/
FN_EXTERN HttpClient *
httpClientNew(IoClient *const ioClient, const TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_CLIENT, ioClient);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(ioClient != NULL);

    OBJ_NEW_BEGIN(HttpClient, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HttpClient)
        {
            .pub =
            {
                .timeout = timeout,
            },
            .ioClient = ioClient,
            .sessionReuseList = lstNewP(sizeof(HttpSession *)),
        };

        statInc(HTTP_STAT_CLIENT_STR);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_CLIENT, this);
}

/**********************************************************************************************************************************/
FN_EXTERN HttpSession *
httpClientOpen(HttpClient *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(HTTP_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    HttpSession *result = NULL;

    // Check if there is a reusable session
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
FN_EXTERN void
httpClientReuse(HttpClient *const this, HttpSession *const session)
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
FN_EXTERN void
httpClientToLog(const HttpClient *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{ioClient: ");
    ioClientToLog(this->ioClient, debugLog);
    strStcFmt(debugLog, ", reusable: %u, timeout: %" PRIu64 "}", lstSize(this->sessionReuseList), httpClientTimeout(this));
}
