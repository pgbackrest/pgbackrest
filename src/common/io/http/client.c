/***********************************************************************************************************************************
Http Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/tls/client.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Statistics
***********************************************************************************************************************************/
static HttpClientStat httpClientStatLocal;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpClient
{
    MemContext *memContext;                                         // Mem context
    TimeMSec timeout;                                               // Request timeout
    TlsClient *tlsClient;                                           // TLS client

    List *sessionReuseList;                                         // List of http sessions that can be reused
};

OBJECT_DEFINE_GET(Timeout, const, HTTP_CLIENT, TimeMSec, timeout);

/**********************************************************************************************************************************/
HttpClient *
httpClientNew(
    const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(BOOL, verifyPeer);
        FUNCTION_LOG_PARAM(STRING, caFile);
        FUNCTION_LOG_PARAM(STRING, caPath);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);

    HttpClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpClient")
    {
        this = memNew(sizeof(HttpClient));

        *this = (HttpClient)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .timeout = timeout,
            .tlsClient = tlsClientNew(sckClientNew(host, port, timeout), timeout, verifyPeer, caFile, caPath),
            .sessionReuseList = lstNew(sizeof(HttpSession *)),
        };

        httpClientStatLocal.object++;
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

    // Check is there is a resuable session
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
        result = httpSessionNew(this, tlsClientOpen(this->tlsClient));
        httpClientStatLocal.session++;
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
httpClientStatStr(void)
{
    FUNCTION_TEST_VOID();

    String *result = NULL;

    if (httpClientStatLocal.object > 0)
    {
        result = strNewFmt(
            "http statistics: objects %" PRIu64 ", sessions %" PRIu64 ", requests %" PRIu64 ", retries %" PRIu64
                ", closes %" PRIu64,
            httpClientStatLocal.object, httpClientStatLocal.session, httpClientStatLocal.request, httpClientStatLocal.retry,
            httpClientStatLocal.close);
    }

    FUNCTION_TEST_RETURN(result);
}
