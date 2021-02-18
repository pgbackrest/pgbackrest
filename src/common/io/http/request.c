/***********************************************************************************************************************************
HTTP Request
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/common.h"
#include "common/io/http/request.h"
#include "common/log.h"
#include "common/stat.h"
#include "common/type/object.h"
#include "common/wait.h"
#include "version.h"

/***********************************************************************************************************************************
HTTP constants
***********************************************************************************************************************************/
STRING_EXTERN(HTTP_VERSION_STR,                                     HTTP_VERSION);
STRING_EXTERN(HTTP_VERSION_10_STR,                                  HTTP_VERSION_10);

STRING_EXTERN(HTTP_VERB_DELETE_STR,                                 HTTP_VERB_DELETE);
STRING_EXTERN(HTTP_VERB_GET_STR,                                    HTTP_VERB_GET);
STRING_EXTERN(HTTP_VERB_HEAD_STR,                                   HTTP_VERB_HEAD);
STRING_EXTERN(HTTP_VERB_POST_STR,                                   HTTP_VERB_POST);
STRING_EXTERN(HTTP_VERB_PUT_STR,                                    HTTP_VERB_PUT);

STRING_EXTERN(HTTP_HEADER_AUTHORIZATION_STR,                        HTTP_HEADER_AUTHORIZATION);
STRING_EXTERN(HTTP_HEADER_CONTENT_LENGTH_STR,                       HTTP_HEADER_CONTENT_LENGTH);
STRING_EXTERN(HTTP_HEADER_CONTENT_MD5_STR,                          HTTP_HEADER_CONTENT_MD5);
STRING_EXTERN(HTTP_HEADER_CONTENT_RANGE_STR,                        HTTP_HEADER_CONTENT_RANGE);
STRING_EXTERN(HTTP_HEADER_ETAG_STR,                                 HTTP_HEADER_ETAG);
STRING_EXTERN(HTTP_HEADER_DATE_STR,                                 HTTP_HEADER_DATE);
STRING_EXTERN(HTTP_HEADER_HOST_STR,                                 HTTP_HEADER_HOST);
STRING_EXTERN(HTTP_HEADER_LAST_MODIFIED_STR,                        HTTP_HEADER_LAST_MODIFIED);
#define HTTP_HEADER_USER_AGENT                                      "user-agent"

// 5xx errors that should always be retried
#define HTTP_RESPONSE_CODE_RETRY_CLASS                              5

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
httpRequestProcess(HttpRequest *this, bool waitForResponse, bool contentCache)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(HTTP_REQUEST, this);
        FUNCTION_LOG_PARAM(BOOL, waitForResponse);
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

                    // If a session is saved then the request was already successfully sent
                    if (this->session != NULL)
                    {
                        session = httpSessionMove(this->session, memContextCurrent());
                        this->session = NULL;
                    }
                    // Else the request has not been sent yet or this is a retry
                    else
                    {
                        session = httpClientOpen(this->client);

                        // Format the request and user agent
                        String *requestStr =
                            strNewFmt(
                                "%s %s%s%s " HTTP_VERSION CRLF_Z HTTP_HEADER_USER_AGENT ":" PROJECT_NAME "/" PROJECT_VERSION CRLF_Z,
                                strZ(this->verb), strZ(httpUriEncode(this->uri, true)), this->query == NULL ? "" : "?",
                                this->query == NULL ? "" : strZ(httpQueryRenderP(this->query)));

                        // Add headers
                        const StringList *headerList = httpHeaderList(this->header);

                        for (unsigned int headerIdx = 0; headerIdx < strLstSize(headerList); headerIdx++)
                        {
                            const String *headerKey = strLstGet(headerList, headerIdx);

                            strCatFmt(requestStr, "%s:%s" CRLF_Z, strZ(headerKey), strZ(httpHeaderGet(this->header, headerKey)));
                        }

                        // Add blank line to end of headers and write the request as a buffer so secrets do not show up in logs
                        strCat(requestStr, CRLF_STR);
                        ioWrite(httpSessionIoWrite(session), BUFSTR(requestStr));

                        // Write out content if any
                        if (this->content != NULL)
                            ioWrite(httpSessionIoWrite(session), this->content);

                        // Flush all writes
                        ioWriteFlush(httpSessionIoWrite(session));

                        // If not waiting for the response then move the session to the object context
                        if (!waitForResponse)
                            this->session = httpSessionMove(session, this->memContext);
                    }

                    // Wait for response
                    if (waitForResponse)
                    {
                        result = httpResponseNew(session, this->verb, contentCache);

                        // Retry when response code is 5xx.  These errors generally represent a server error for a request that
                        // looks valid. There are a few errors that might be permanently fatal but they are rare and it seems best
                        // not to try and pick and choose errors in this class to retry.
                        if (httpResponseCode(result) / 100 == HTTP_RESPONSE_CODE_RETRY_CLASS)
                            THROW_FMT(ServiceError, "[%u] %s", httpResponseCode(result), strZ(httpResponseReason(result)));

                        // Move response to outer temp context
                        httpResponseMove(result, memContextPrior());
                    }
                }
                MEM_CONTEXT_TEMP_END();
            }
            CATCH_ANY()
            {
                // Sleep and then retry unless the total wait time has expired
                if (waitMore(wait))
                {
                    LOG_DEBUG_FMT("retry %s: %s", errorTypeName(errorType()), errorMessage());
                    retry = true;

                    statInc(HTTP_STAT_RETRY_STR);
                }
                else
                    RETHROW();
            }
            TRY_END();
        }
        while (retry);

        // Move response to calling context
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
            .query = httpQueryDupP(param.query),
            .header = param.header == NULL ? httpHeaderNew(NULL) : httpHeaderDup(param.header, NULL),
            .content = param.content == NULL ? NULL : bufDup(param.content),
        };

        // Send the request
        httpRequestProcess(this, false, false);
        statInc(HTTP_STAT_REQUEST_STR);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_REQUEST, this);
}

/**********************************************************************************************************************************/
HttpResponse *
httpRequestResponse(HttpRequest *this, bool contentCache)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(HTTP_REQUEST, this);
        FUNCTION_LOG_PARAM(BOOL, contentCache);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(HTTP_RESPONSE, httpRequestProcess(this, true, contentCache));
}

/**********************************************************************************************************************************/
void
httpRequestError(const HttpRequest *this, HttpResponse *response)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM(HTTP_REQUEST, this);
        FUNCTION_LOG_PARAM(HTTP_RESPONSE, response);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(response != NULL);

    // Error code
    String *error = strNewFmt("HTTP request failed with %u", httpResponseCode(response));

    // Add reason when present
    if (strSize(httpResponseReason(response)) > 0)
        strCatFmt(error, " (%s)", strZ(httpResponseReason(response)));

    // Output uri/query
    strCatZ(error, ":\n*** URI/Query ***:");

    strCatFmt(error, "\n%s", strZ(httpUriEncode(this->uri, true)));

    if (this->query != NULL)
        strCatFmt(error, "?%s", strZ(httpQueryRenderP(this->query, .redact = true)));

    // Output request headers
    const StringList *requestHeaderList = httpHeaderList(this->header);

    if (!strLstEmpty(requestHeaderList))
    {
        strCatZ(error, "\n*** Request Headers ***:");

        for (unsigned int requestHeaderIdx = 0; requestHeaderIdx < strLstSize(requestHeaderList); requestHeaderIdx++)
        {
            const String *key = strLstGet(requestHeaderList, requestHeaderIdx);

            strCatFmt(
                error, "\n%s: %s", strZ(key),
                httpHeaderRedact(this->header, key) ? "<redacted>" : strZ(httpHeaderGet(this->header, key)));
        }
    }

    // Output response headers
    const HttpHeader *responseHeader = httpResponseHeader(response);
    const StringList *responseHeaderList = httpHeaderList(responseHeader);

    if (!strLstEmpty(responseHeaderList))
    {
        strCatZ(error, "\n*** Response Headers ***:");

        for (unsigned int responseHeaderIdx = 0; responseHeaderIdx < strLstSize(responseHeaderList); responseHeaderIdx++)
        {
            const String *key = strLstGet(responseHeaderList, responseHeaderIdx);
            strCatFmt(error, "\n%s: %s", strZ(key), strZ(httpHeaderGet(responseHeader, key)));
        }
    }

    // Add response content, if any
    if (!bufEmpty(httpResponseContent(response)))
    {
        strCatZ(error, "\n*** Response Content ***:\n");
        strCat(error, strNewBuf(httpResponseContent(response)));
    }

    THROW(ProtocolError, strZ(error));
}

/**********************************************************************************************************************************/
String *
httpRequestToLog(const HttpRequest *this)
{
    return strNewFmt(
        "{verb: %s, uri: %s, query: %s, header: %s, contentSize: %zu}", strZ(this->verb), strZ(this->uri),
        this->query == NULL ? "null" : strZ(httpQueryToLog(this->query)), strZ(httpHeaderToLog(this->header)),
        this->content == NULL ? 0 : bufUsed(this->content));
}
