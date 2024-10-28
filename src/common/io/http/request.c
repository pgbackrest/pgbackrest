/***********************************************************************************************************************************
HTTP Request
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/error/retry.h"
#include "common/io/http/common.h"
#include "common/io/http/request.h"
#include "common/log.h"
#include "common/stat.h"
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
STRING_EXTERN(HTTP_HEADER_CONTENT_ID_STR,                           HTTP_HEADER_CONTENT_ID);
STRING_EXTERN(HTTP_HEADER_CONTENT_LENGTH_STR,                       HTTP_HEADER_CONTENT_LENGTH);
STRING_EXTERN(HTTP_HEADER_CONTENT_MD5_STR,                          HTTP_HEADER_CONTENT_MD5);
STRING_EXTERN(HTTP_HEADER_CONTENT_RANGE_STR,                        HTTP_HEADER_CONTENT_RANGE);
STRING_EXTERN(HTTP_HEADER_CONTENT_TYPE_STR,                         HTTP_HEADER_CONTENT_TYPE);
STRING_EXTERN(HTTP_HEADER_CONTENT_TYPE_APP_FORM_URL_STR,            HTTP_HEADER_CONTENT_TYPE_APP_FORM_URL);
STRING_EXTERN(HTTP_HEADER_CONTENT_TYPE_HTTP_STR,                    HTTP_HEADER_CONTENT_TYPE_HTTP);
STRING_EXTERN(HTTP_HEADER_CONTENT_TYPE_JSON_STR,                    HTTP_HEADER_CONTENT_TYPE_JSON);
STRING_EXTERN(HTTP_HEADER_CONTENT_TYPE_XML_STR,                     HTTP_HEADER_CONTENT_TYPE_XML);
STRING_EXTERN(HTTP_HEADER_ETAG_STR,                                 HTTP_HEADER_ETAG);
STRING_EXTERN(HTTP_HEADER_DATE_STR,                                 HTTP_HEADER_DATE);
STRING_EXTERN(HTTP_HEADER_HOST_STR,                                 HTTP_HEADER_HOST);
STRING_EXTERN(HTTP_HEADER_LAST_MODIFIED_STR,                        HTTP_HEADER_LAST_MODIFIED);
STRING_EXTERN(HTTP_HEADER_RANGE_STR,                                HTTP_HEADER_RANGE);
#define HTTP_HEADER_USER_AGENT                                      "user-agent"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpRequest
{
    HttpRequestPub pub;                                             // Publicly accessible variables
    HttpClient *client;                                             // HTTP client
    const Buffer *content;                                          // HTTP content

    HttpSession *session;                                           // Session for async requests
};

struct HttpRequestMulti
{
    List *contentList;                                              // Multipart content
    size_t contentSize;                                             // Size of all content (excluding boundaries)
    Buffer *boundaryRaw;                                            // Multipart boundary (not yet formatted for output)
};

/***********************************************************************************************************************************
Format the request (excluding content)
***********************************************************************************************************************************/
static String *
httpRequestFmt(
    const String *const verb, const String *const path, const HttpQuery *const query, const HttpHeader *const header,
    const bool agent)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, verb);
        FUNCTION_TEST_PARAM(STRING, path);
        FUNCTION_TEST_PARAM(HTTP_QUERY, query);
        FUNCTION_TEST_PARAM(HTTP_HEADER, header);
        FUNCTION_TEST_PARAM(BOOL, agent);
    FUNCTION_TEST_END();

    ASSERT(verb != NULL);
    ASSERT(path != NULL);
    ASSERT(header != NULL);

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Add verb, path, and query
        strCatFmt(
            result, "%s %s%s%s " HTTP_VERSION "\r\n", strZ(verb), strZ(path), query == NULL ? "" : "?",
            query == NULL ? "" : strZ(httpQueryRenderP(query)));

        // Add user agent
        if (agent)
            strCatZ(result, HTTP_HEADER_USER_AGENT ":" PROJECT_NAME "/" PROJECT_VERSION "\r\n");

        // Add headers
        StringList *const headerList = httpHeaderList(header);

        for (unsigned int headerIdx = 0; headerIdx < strLstSize(headerList); headerIdx++)
        {
            const String *const headerKey = strLstGet(headerList, headerIdx);

            strCatFmt(result, "%s:%s\r\n", strZ(headerKey), strZ(httpHeaderGet(header, headerKey)));
        }

        // Add blank line to end of headers
        strCat(result, CRLF_STR);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Process the request
***********************************************************************************************************************************/
static HttpResponse *
httpRequestProcess(HttpRequest *const this, const bool waitForResponse, const bool contentCache)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
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
        ErrorRetry *const errRetry = errRetryNew();
        Wait *const wait = waitNew(httpClientTimeout(this->client));

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

                        // Write the request as a buffer so secrets do not show up in logs
                        ioWrite(
                            httpSessionIoWrite(session),
                            BUFSTR(
                                httpRequestFmt(
                                    httpRequestVerb(this), httpRequestPath(this), httpRequestQuery(this), httpRequestHeader(this),
                                    true)));

                        // Write out content if any
                        if (this->content != NULL)
                            ioWrite(httpSessionIoWrite(session), this->content);

                        // Flush all writes
                        ioWriteFlush(httpSessionIoWrite(session));

                        // If not waiting for the response then move the session to the object context
                        if (!waitForResponse)
                            this->session = httpSessionMove(session, objMemContext(this));
                    }

                    // Wait for response
                    if (waitForResponse)
                    {
                        result = httpResponseNew(session, httpRequestVerb(this), contentCache);

                        // Retry when response code is 5xx. These errors generally represent a server error for a request that looks
                        // valid. There are a few errors that might be permanently fatal but they are rare and it seems best not to
                        // try and pick and choose errors in this class to retry.
                        if (httpResponseCodeRetry(result))
                            THROW_FMT(ServiceError, "[%u] %s", httpResponseCode(result), strZ(httpResponseReason(result)));

                        // Move response to outer temp context
                        httpResponseMove(result, memContextPrior());
                    }
                }
                MEM_CONTEXT_TEMP_END();
            }
            CATCH_ANY()
            {
                // Add the error retry info
                errRetryAddP(errRetry);

                // Sleep and then retry unless the total wait time has expired
                if (waitMore(wait))
                {
                    LOG_DEBUG_FMT("retry %s: %s", errorTypeName(errorType()), errorMessage());
                    retry = true;

                    statInc(HTTP_STAT_RETRY_STR);
                }
                else
                    THROWP(errRetryType(errRetry), strZ(errRetryMessage(errRetry)));
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
FN_EXTERN HttpRequest *
httpRequestNew(HttpClient *const client, const String *const verb, const String *const path, const HttpRequestNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(HTTP_CLIENT, client);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(HTTP_QUERY, param.query);
        FUNCTION_LOG_PARAM(HTTP_HEADER, param.header);
        FUNCTION_LOG_PARAM(BUFFER, param.content);
    FUNCTION_LOG_END();

    ASSERT(verb != NULL);
    ASSERT(path != NULL);

    OBJ_NEW_BEGIN(HttpRequest, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HttpRequest)
        {
            .pub =
            {
                .verb = strDup(verb),
                .path = strDup(path),
                .query = httpQueryDupP(param.query),
                .header = param.header == NULL ? httpHeaderNew(NULL) : httpHeaderDup(param.header, NULL),
            },
            .client = client,
            .content = param.content == NULL ? NULL : bufDup(param.content),
        };
    }
    OBJ_NEW_END();

    // Send the request
    httpRequestProcess(this, false, false);
    statInc(HTTP_STAT_REQUEST_STR);

    FUNCTION_LOG_RETURN(HTTP_REQUEST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN HttpResponse *
httpRequestResponse(HttpRequest *const this, const bool contentCache)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(HTTP_REQUEST, this);
        FUNCTION_LOG_PARAM(BOOL, contentCache);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(HTTP_RESPONSE, httpRequestProcess(this, true, contentCache));
}

/**********************************************************************************************************************************/
FN_EXTERN void
httpRequestError(const HttpRequest *const this, HttpResponse *const response)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(HTTP_REQUEST, this);
        FUNCTION_LOG_PARAM(HTTP_RESPONSE, response);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(response != NULL);

    // Error code
    String *const error = strCatFmt(strNew(), "HTTP request failed with %u", httpResponseCode(response));

    // Add reason when present
    if (strSize(httpResponseReason(response)) > 0)
        strCatFmt(error, " (%s)", strZ(httpResponseReason(response)));

    // Output path/query
    strCatZ(error, ":\n*** Path/Query ***:");

    strCatFmt(error, "\n%s %s", strZ(httpRequestVerb(this)), strZ(httpRequestPath(this)));

    if (httpRequestQuery(this) != NULL)
        strCatFmt(error, "?%s", strZ(httpQueryRenderP(httpRequestQuery(this), .redact = true)));

    // Output request headers
    const StringList *const requestHeaderList = httpHeaderList(httpRequestHeader(this));

    if (!strLstEmpty(requestHeaderList))
    {
        strCatZ(error, "\n*** Request Headers ***:");

        for (unsigned int requestHeaderIdx = 0; requestHeaderIdx < strLstSize(requestHeaderList); requestHeaderIdx++)
        {
            const String *const key = strLstGet(requestHeaderList, requestHeaderIdx);

            strCatFmt(
                error, "\n%s: %s", strZ(key),
                httpHeaderRedact(httpRequestHeader(this), key) ? "<redacted>" : strZ(httpHeaderGet(httpRequestHeader(this), key)));
        }
    }

    // Output response headers
    const HttpHeader *const responseHeader = httpResponseHeader(response);
    const StringList *const responseHeaderList = httpHeaderList(responseHeader);

    if (!strLstEmpty(responseHeaderList))
    {
        strCatZ(error, "\n*** Response Headers ***:");

        for (unsigned int responseHeaderIdx = 0; responseHeaderIdx < strLstSize(responseHeaderList); responseHeaderIdx++)
        {
            const String *const key = strLstGet(responseHeaderList, responseHeaderIdx);
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
#define HTTP_MULTIPART_BOUNDARY_INIT_SIZE                           (sizeof(HTTP_MULTIPART_BOUNDARY_INIT) - 1)

FN_EXTERN HttpRequestMulti *
httpRequestMultiNew(void)
{
    FUNCTION_TEST_VOID();

    OBJ_NEW_BEGIN(HttpRequestMulti, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HttpRequestMulti)
        {
            .boundaryRaw = bufNewC(HTTP_MULTIPART_BOUNDARY_INIT, HTTP_MULTIPART_BOUNDARY_INIT_SIZE),
            .contentList = lstNewP(sizeof(Buffer *)),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(HTTP_REQUEST_MULTI, this);
}

/**********************************************************************************************************************************/
#define HTTP_MULTIPART_BOUNDARY_EXTRA_SIZE                          (sizeof(HTTP_MULTIPART_BOUNDARY_EXTRA) - 1)

FN_EXTERN void
httpRequestMultiAdd(
    HttpRequestMulti *const this, const String *const contentId, const String *const verb, const String *const path,
    const HttpRequestNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_REQUEST_MULTI, this);
        FUNCTION_TEST_PARAM(STRING, contentId);
        FUNCTION_TEST_PARAM(STRING, verb);
        FUNCTION_TEST_PARAM(STRING, path);
        FUNCTION_TEST_PARAM(HTTP_QUERY, param.query);
        FUNCTION_TEST_PARAM(HTTP_HEADER, param.header);
        FUNCTION_TEST_PARAM(BUFFER, param.content);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(contentId != NULL);
    ASSERT(verb != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Construct request header
        String *const request = strNew();

        strCatZ(request, HTTP_HEADER_CONTENT_TYPE ":" HTTP_HEADER_CONTENT_TYPE_HTTP "\r\n");
        strCatZ(request, HTTP_HEADER_CONTENT_TRANSFER_ENCODING ":" HTTP_HEADER_CONTENT_TRANSFER_ENCODING_BINARY "\r\n");
        strCatFmt(request, HTTP_HEADER_CONTENT_ID ":%s\r\n\r\n", strZ(contentId));
        strCat(request, httpRequestFmt(verb, path, param.query, param.header, false));

        MEM_CONTEXT_OBJ_BEGIN(this->contentList)
        {
            // Add content
            Buffer *const content = bufNew(strSize(request) + (param.content == NULL ? 0 : bufUsed(param.content)));

            bufCat(content, BUFSTR(request));
            bufCat(content, param.content);

            // Find a boundary that is not used in the content
            while (bufFindP(content, this->boundaryRaw) != NULL)
            {
                if (bufUsed(this->boundaryRaw) == HTTP_MULTIPART_BOUNDARY_INIT_SIZE + HTTP_MULTIPART_BOUNDARY_EXTRA_SIZE)
                    THROW(AssertError, "unable to construct unique boundary");

                bufCatC(
                    this->boundaryRaw, (const unsigned char *)HTTP_MULTIPART_BOUNDARY_EXTRA,
                    bufUsed(this->boundaryRaw) - HTTP_MULTIPART_BOUNDARY_INIT_SIZE, HTTP_MULTIPART_BOUNDARY_NEXT);
            }

            // Add to list
            lstAdd(this->contentList, &content);
            this->contentSize += bufUsed(content);
        }
        MEM_CONTEXT_OBJ_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
#define HTTP_MULTIPART_BOUNDARY_PRE_SIZE                            (sizeof(HTTP_MULTIPART_BOUNDARY_PRE) - 1)
#define HTTP_MULTIPART_BOUNDARY_POST_SIZE                           (sizeof(HTTP_MULTIPART_BOUNDARY_POST) - 1)
#define HTTP_MULTIPART_BOUNDARY_POST_LAST_SIZE                      (sizeof(HTTP_MULTIPART_BOUNDARY_POST_LAST) - 1)

FN_EXTERN Buffer *
httpRequestMultiContent(HttpRequestMulti *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_REQUEST_MULTI, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(!lstEmpty(this->contentList));

    const size_t boundarySize = bufUsed(this->boundaryRaw) + HTTP_MULTIPART_BOUNDARY_PRE_SIZE;
    Buffer *const result = bufNew(
        this->contentSize + (lstSize(this->contentList) * (boundarySize + HTTP_MULTIPART_BOUNDARY_POST_SIZE)) +
        HTTP_MULTIPART_BOUNDARY_POST_LAST_SIZE);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Generate boundary
        Buffer *const boundary = bufNew(boundarySize);
        bufCat(boundary, BUFSTRDEF(HTTP_MULTIPART_BOUNDARY_PRE));
        bufCat(boundary, this->boundaryRaw);

        // Add first boundary
        bufCat(result, boundary);
        bufCat(result, BUFSTRDEF(HTTP_MULTIPART_BOUNDARY_POST));

        // Add content and boundaries
        for (unsigned int contentIdx = 0; contentIdx < lstSize(this->contentList); contentIdx++)
        {
            bufCat(result, *(Buffer **)lstGet(this->contentList, contentIdx));
            bufCat(result, boundary);

            if (contentIdx < lstSize(this->contentList) - 1)
                bufCat(result, BUFSTRDEF(HTTP_MULTIPART_BOUNDARY_POST));
            else
                bufCat(result, BUFSTRDEF(HTTP_MULTIPART_BOUNDARY_POST_LAST));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(BUFFER, result);
}

/**********************************************************************************************************************************/
FN_EXTERN HttpHeader *
httpRequestMultiHeaderAdd(HttpRequestMulti *const this, HttpHeader *const header)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_REQUEST_MULTI, this);
        FUNCTION_TEST_PARAM(HTTP_HEADER, header);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(header != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        httpHeaderAdd(
            header, HTTP_HEADER_CONTENT_TYPE_STR,
            strNewFmt(HTTP_HEADER_CONTENT_TYPE_MULTIPART "%s", strZ(strNewBuf(this->boundaryRaw))));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(HTTP_HEADER, header);
}

/**********************************************************************************************************************************/
FN_EXTERN void
httpRequestToLog(const HttpRequest *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog, "{verb: %s, path: %s, contentSize: %zu, query: ", strZ(httpRequestVerb(this)),
        strZ(httpRequestPath(this)), this->content == NULL ? 0 : bufUsed(this->content));

    strStcResultSizeInc(
        debugLog,
        FUNCTION_LOG_OBJECT_FORMAT(
            httpRequestQuery(this), httpQueryToLog, strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcCat(debugLog, ", header: "),
    httpHeaderToLog(httpRequestHeader(this), debugLog);
    strStcCatChr(debugLog, '}');
}
