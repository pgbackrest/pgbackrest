/***********************************************************************************************************************************
HTTP Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/client.h"
#include "common/io/http/client.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Statistics
***********************************************************************************************************************************/
HttpClientStat httpClientStat;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpClient
{
    MemContext *memContext;                                         // Mem context
    TimeMSec timeout;                                               // Request timeout
    IoClient *ioClient;                                             // Io client (e.g. TLS or socket client)

    List *sessionReuseList;                                         // List of HTTP sessions that can be reused
};

OBJECT_DEFINE_GET(Timeout, const, HTTP_CLIENT, TimeMSec, timeout);

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
            .memContext = MEM_CONTEXT_NEW(),
            .timeout = timeout,
            .ioClient = ioClient,
            .sessionReuseList = lstNewP(sizeof(HttpSession *)),
        };

        httpClientStat.object++;
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
    if (lstSize(this->sessionReuseList) > 0)
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
        httpClientStat.session++;
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
        "{ioClient: %s, timeout: %" PRIu64"}", memContextFreeing(this->memContext) ? NULL_Z : strZ(ioClientToLog(this->ioClient)),
        this->timeout);
}

/**********************************************************************************************************************************/
String *
httpClientStatStr(void)
{
    FUNCTION_TEST_VOID();

    String *result = NULL;

    if (httpClientStat.object > 0)
    {
        result = strNewFmt(
            "http statistics: objects %" PRIu64 ", sessions %" PRIu64 ", requests %" PRIu64 ", retries %" PRIu64
                ", closes %" PRIu64,
            httpClientStat.object, httpClientStat.session, httpClientStat.request, httpClientStat.retry, httpClientStat.close);
    }

    FUNCTION_TEST_RETURN(result);
}
