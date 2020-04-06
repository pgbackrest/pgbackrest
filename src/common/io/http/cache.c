/***********************************************************************************************************************************
Http Client Cache
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/cache.h"
#include "common/log.h"
#include "common/type/list.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpClientCache
{
    MemContext *memContext;                                         // Mem context

    const String *host;                                             // Client settings
    unsigned int port;
    TimeMSec timeout;
    bool verifyPeer;
    const String *caFile;
    const String *caPath;

    List *clientList;                                               // List of http clients
};

OBJECT_DEFINE_FREE(HTTP_CLIENT_CACHE);

/**********************************************************************************************************************************/
HttpClientCache *
httpClientCacheNew(
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

    HttpClientCache *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpClientCache")
    {
        // Allocate state and set context
        this = memNew(sizeof(HttpClientCache));

        *this = (HttpClientCache)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .host = strDup(host),
            .port = port,
            .timeout = timeout,
            .verifyPeer = verifyPeer,
            .caFile = strDup(caFile),
            .caPath = strDup(caPath),
            .clientList = lstNew(sizeof(HttpClient *)),
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_CLIENT_CACHE, this);
}

/**********************************************************************************************************************************/
HttpClient *
httpClientCacheGet(HttpClientCache *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM(HTTP_CLIENT_CACHE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    HttpClient *result = NULL;

    // Search for a client that is not busy
    for (unsigned int clientIdx = 0; clientIdx < lstSize(this->clientList); clientIdx++)
    {
        HttpClient *httpClient = *(HttpClient **)lstGet(this->clientList, clientIdx);

        if (!httpClientBusy(httpClient))
            result = httpClient;
    }

    // If none found then create a new one
    if (result == NULL)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            result = httpClientNew(this->host, this->port, this->timeout, this->verifyPeer, this->caFile, this->caPath);
            lstAdd(this->clientList, &result);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(HTTP_CLIENT, result);
}
