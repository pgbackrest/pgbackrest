/***********************************************************************************************************************************
Http Request
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/common.h"
#include "common/io/http/request.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Http constants
***********************************************************************************************************************************/
STRING_EXTERN(HTTP_VERSION_STR,                                     HTTP_VERSION);

STRING_EXTERN(HTTP_VERB_DELETE_STR,                                 HTTP_VERB_DELETE);
STRING_EXTERN(HTTP_VERB_GET_STR,                                    HTTP_VERB_GET);
STRING_EXTERN(HTTP_VERB_HEAD_STR,                                   HTTP_VERB_HEAD);
STRING_EXTERN(HTTP_VERB_POST_STR,                                   HTTP_VERB_POST);
STRING_EXTERN(HTTP_VERB_PUT_STR,                                    HTTP_VERB_PUT);

STRING_EXTERN(HTTP_HEADER_AUTHORIZATION_STR,                        HTTP_HEADER_AUTHORIZATION);
STRING_EXTERN(HTTP_HEADER_CONTENT_LENGTH_STR,                       HTTP_HEADER_CONTENT_LENGTH);
STRING_EXTERN(HTTP_HEADER_CONTENT_MD5_STR,                          HTTP_HEADER_CONTENT_MD5);
STRING_EXTERN(HTTP_HEADER_ETAG_STR,                                 HTTP_HEADER_ETAG);
STRING_EXTERN(HTTP_HEADER_HOST_STR,                                 HTTP_HEADER_HOST);
STRING_EXTERN(HTTP_HEADER_LAST_MODIFIED_STR,                        HTTP_HEADER_LAST_MODIFIED);

// 5xx errors that should always be retried
#define HTTP_RESPONSE_CODE_RETRY_CLASS                              5

/***********************************************************************************************************************************
Statistics
***********************************************************************************************************************************/
extern HttpClientStat httpClientStat;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpRequest
{
    MemContext *memContext;                                         // Mem context
    HttpClient *client;                                             // HTTP client
    const String *verb;                                             // HTTP verb (GET, POST, etc.)
    const String *uri;                                              // HTTP URI
    const HttpQuery *query;                                         // HTTP query
    const HttpHeader *header;                                       // HTTP headers
    const Buffer *content;                                          // HTTP content

    HttpSession *session;                                           // Session for async requests
};

OBJECT_DEFINE_MOVE(HTTP_REQUEST);
OBJECT_DEFINE_FREE(HTTP_REQUEST);

OBJECT_DEFINE_GET(Verb, const, HTTP_REQUEST, const String *, verb);
OBJECT_DEFINE_GET(Uri, const, HTTP_REQUEST, const String *, uri);
OBJECT_DEFINE_GET(Query, const, HTTP_REQUEST, const HttpQuery *, query);
OBJECT_DEFINE_GET(Header, const, HTTP_REQUEST, const HttpHeader *, header);

/***********************************************************************************************************************************
Process the request
***********************************************************************************************************************************/
static HttpResponse *
httpRequestProcess(HttpRequest *this, bool requestOnly, bool contentCache)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(HTTP_REQUEST, this);
        FUNCTION_LOG_PARAM(BOOL, requestOnly);
        FUNCTION_LOG_PARAM(BOOL, contentCache);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // HTTP Response
    HttpResponse *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        bool retry;
        Wait *wait = waitNew(httpClientTimeout(this->client));

        do
        {
            // Assume there will be no retry
            retry = false;

            TRY_BEGIN()
            {
                MEM_CONTEXT_TEMP_BEGIN()
                {
                    HttpSession *session = NULL;

                    // If a session is saved then the request was already sent
                    if (this->session != NULL)
                    {
                        session = httpSessionMove(this->session, memContextCurrent());
                        this->session = NULL;
                    }
                    // Else the request has not been sent yet or this is a retry
                    else
                    {
                        session = httpClientOpen(this->client);

                        // Write the request
                        String *queryStr = httpQueryRender(this->query);

                        ioWriteStrLine(
                            httpSessionIoWrite(session),
                            strNewFmt(
                                "%s %s%s%s " HTTP_VERSION "\r", strPtr(this->verb), strPtr(httpUriEncode(this->uri, true)),
                                queryStr == NULL ? "" : "?", queryStr == NULL ? "" : strPtr(queryStr)));

                        // Write headers
                        if (this->header != NULL)
                        {
                            const StringList *headerList = httpHeaderList(this->header);

                            for (unsigned int headerIdx = 0; headerIdx < strLstSize(headerList); headerIdx++)
                            {
                                const String *headerKey = strLstGet(headerList, headerIdx);
                                ioWriteStrLine(
                                    httpSessionIoWrite(session),
                                    strNewFmt("%s:%s\r", strPtr(headerKey), strPtr(httpHeaderGet(this->header, headerKey))));
                            }
                        }

                        // Write out blank line to end the headers
                        ioWriteLine(httpSessionIoWrite(session), CR_BUF);

                        // Write out content if any
                        if (this->content != NULL)
                            ioWrite(httpSessionIoWrite(session), this->content);

                        // Flush all writes
                        ioWriteFlush(httpSessionIoWrite(session));

                        // If only performing the request then move the session to the object context
                        if (requestOnly)
                            this->session = httpSessionMove(session, this->memContext);
                    }

                    // Wait for response
                    if (!requestOnly)
                    {
                        result = httpResponseNew(session, this->verb, contentCache);

                        // Retry when response code is 5xx.  These errors generally represent a server error for a request that
                        // looks valid. There are a few errors that might be permanently fatal but they are rare and it seems best
                        // not to try and pick and choose errors in this class to retry.
                        if (httpResponseCode(result) / 100 == HTTP_RESPONSE_CODE_RETRY_CLASS)
                            THROW_FMT(ServiceError, "[%u] %s", httpResponseCode(result), strPtr(httpResponseReason(result)));

                        // Move response to prior context
                        httpResponseMove(result, memContextPrior());
                    }
                }
                MEM_CONTEXT_END();
            }
            CATCH_ANY()
            {
                // Retry if wait time has not expired
                if (waitMore(wait))
                {
                    LOG_DEBUG_FMT("retry %s: %s", errorTypeName(errorType()), errorMessage());
                    retry = true;

                    httpClientStat.retry++;
                }
                else
                    RETHROW();
            }
            TRY_END();
        }
        while (retry);

        // Move response to prior context
        httpResponseMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(HTTP_RESPONSE, result);
}

/**********************************************************************************************************************************/
HttpRequest *
httpRequestNew(HttpClient *client, const String *verb, const String *uri, HttpRequestNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(HTTP_CLIENT, client);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, uri);
        FUNCTION_LOG_PARAM(HTTP_QUERY, param.query);
        FUNCTION_LOG_PARAM(HTTP_HEADER, param.header);
        FUNCTION_LOG_PARAM(BUFFER, param.content);
    FUNCTION_LOG_END();

    ASSERT(verb != NULL);
    ASSERT(uri != NULL);

    HttpRequest *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpRequest")
    {
        this = memNew(sizeof(HttpRequest));

        *this = (HttpRequest)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .client = client,
            .verb = strDup(verb),
            .uri = strDup(uri),
            .query = httpQueryDup(param.query),
            .header = httpHeaderDup(param.header, NULL),
            .content = param.content == NULL ? NULL : bufDup(param.content),
        };

        // Send the request
        httpRequestProcess(this, true, false);
        httpClientStat.request++;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_REQUEST, this);
}

/**********************************************************************************************************************************/
HttpResponse *
httpRequest(HttpRequest *this, bool contentCache)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(HTTP_REQUEST, this);
        FUNCTION_LOG_PARAM(BOOL, contentCache);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(HTTP_RESPONSE, httpRequestProcess(this, false, contentCache));
}

/**********************************************************************************************************************************/
String *
httpRequestToLog(const HttpRequest *this)
{
    return strNewFmt(
        "{verb: %s, uri: %s, query: %s, header: %s, contentSize: %zu",
        strPtr(this->verb), strPtr(this->uri), this->query == NULL ? "null" : strPtr(httpQueryToLog(this->query)),
        this->header == NULL ? "null" : strPtr(httpHeaderToLog(this->header)), this->content == NULL ? 0 : bufUsed(this->content));
}
