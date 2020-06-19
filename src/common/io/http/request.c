/***********************************************************************************************************************************
Http Request
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/request.h"
#include "common/log.h"
#include "common/type/object.h"

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
Object type
***********************************************************************************************************************************/
struct HttpRequest
{
    MemContext *memContext;                                         // Mem context

    const String *verb;                                             // HTTP verb (GET, POST, etc.)
    const String *uri;                                              // HTTP URI
    const HttpQuery *query;                                         // HTTP query
    const HttpHeader *header;                                       // HTTP headers
    const Buffer *content;                                          // HTTP content
};

OBJECT_DEFINE_MOVE(HTTP_REQUEST);
OBJECT_DEFINE_FREE(HTTP_REQUEST);

OBJECT_DEFINE_GET(Verb, const, HTTP_REQUEST, const String *, verb);
OBJECT_DEFINE_GET(Uri, const, HTTP_REQUEST, const String *, uri);
OBJECT_DEFINE_GET(Query, const, HTTP_REQUEST, const HttpQuery *, query);
OBJECT_DEFINE_GET(Header, const, HTTP_REQUEST, const HttpHeader *, header);
OBJECT_DEFINE_GET(Content, const, HTTP_REQUEST, const Buffer *, content);

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
            .verb = strDup(verb),
            .uri = strDup(uri),
            .query = httpQueryDup(param.query),
            .header = httpHeaderDup(param.header, NULL),
            .content = param.content == NULL ? NULL : bufDup(param.content),
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_REQUEST, this);
}

/**********************************************************************************************************************************/
// HttpResponse *
// httpClientRequest(HttpClient *this, HttpRequest *request, bool contentCache)
// {
//     FUNCTION_LOG_BEGIN(logLevelDebug)
//         FUNCTION_LOG_PARAM(HTTP_CLIENT, this);
//         FUNCTION_LOG_PARAM(HTTP_REQUEST, request);
//         FUNCTION_LOG_PARAM(BOOL, contentCache);
//     FUNCTION_LOG_END();
//
//     ASSERT(this != NULL);
//     ASSERT(!this->response);
//
//     // HTTP Response
//     HttpResponse *result = NULL;
//
//     MEM_CONTEXT_TEMP_BEGIN()
//     {
//         bool retry;
//         Wait *wait = waitNew(this->timeout);
//
//         do
//         {
//             // Assume there will be no retry
//             retry = false;
//
//             TRY_BEGIN()
//             {
//                 // Get TLS session
//                 if (this->tlsSession == NULL)
//                 {
//                     MEM_CONTEXT_BEGIN(this->memContext)
//                     {
//                         this->tlsSession = tlsClientOpen(this->tlsClient);
//                         httpClientStatLocal.session++;
//                     }
//                     MEM_CONTEXT_END();
//                 }
//
//                 // Write the request
//                 String *queryStr = httpQueryRender(httpRequestQuery(request));
//
//                 ioWriteStrLine(
//                     tlsSessionIoWrite(this->tlsSession),
//                     strNewFmt(
//                         "%s %s%s%s " HTTP_VERSION "\r", strPtr(httpRequestVerb(request)),
//                         strPtr(httpUriEncode(httpRequestUri(request), true)), queryStr == NULL ? "" : "?",
//                         queryStr == NULL ? "" : strPtr(queryStr)));
//
//                 // Write headers
//                 if (httpRequestHeader(request) != NULL)
//                 {
//                     const StringList *headerList = httpHeaderList(httpRequestHeader(request));
//
//                     for (unsigned int headerIdx = 0; headerIdx < strLstSize(headerList); headerIdx++)
//                     {
//                         const String *headerKey = strLstGet(headerList, headerIdx);
//                         ioWriteStrLine(
//                             tlsSessionIoWrite(this->tlsSession),
//                             strNewFmt("%s:%s\r", strPtr(headerKey), strPtr(httpHeaderGet(httpRequestHeader(request), headerKey))));
//                     }
//                 }
//
//                 // Write out blank line to end the headers
//                 ioWriteLine(tlsSessionIoWrite(this->tlsSession), CR_BUF);
//
//                 // Write out content if any
//                 if (httpRequestContent(request) != NULL)
//                     ioWrite(tlsSessionIoWrite(this->tlsSession), httpRequestContent(request));
//
//                 // Flush all writes
//                 ioWriteFlush(tlsSessionIoWrite(this->tlsSession));
//
//                 // Wait for response
//                 result = httpResponseNew(this, tlsSessionIoRead(this->tlsSession), httpRequestVerb(request), contentCache);
//
//                 // Retry when response code is 5xx.  These errors generally represent a server error for a request that looks valid.
//                 // There are a few errors that might be permanently fatal but they are rare and it seems best not to try and pick
//                 // and choose errors in this class to retry.
//                 if (httpResponseCode(result) / 100 == HTTP_RESPONSE_CODE_RETRY_CLASS)
//                     THROW_FMT(ServiceError, "[%u] %s", httpResponseCode(result), strPtr(httpResponseReason(result)));
//             }
//             CATCH_ANY()
//             {
//                 // Close the client since we don't want to reuse the same client on error
//                 httpClientDone(this, true, false);
//
//                 // Retry if wait time has not expired
//                 if (waitMore(wait))
//                 {
//                     LOG_DEBUG_FMT("retry %s: %s", errorTypeName(errorType()), errorMessage());
//                     retry = true;
//
//                     httpClientStatLocal.retry++;
//                 }
//                 else
//                     RETHROW();
//             }
//             TRY_END();
//         }
//         while (retry);
//
//         // Move response to prior context
//         httpResponseMove(result, memContextPrior());
//
//         // If the response is still busy make sure it gets marked done
//         if (httpResponseBusy(result))
//         {
//             this->response = result;
//             memContextCallbackSet(this->memContext, httpClientFreeResource, this);
//         }
//
//         httpClientStatLocal.request++;
//     }
//     MEM_CONTEXT_TEMP_END();
//
//     FUNCTION_LOG_RETURN(HTTP_RESPONSE, result);
// }

/**********************************************************************************************************************************/
String *
httpRequestToLog(const HttpRequest *this)
{
    return strNewFmt(
        "{verb: %s, uri: %s, query: %s, header: %s, contentSize: %zu",
        strPtr(this->verb), strPtr(this->uri), this->query == NULL ? "null" : strPtr(httpQueryToLog(this->query)),
        this->header == NULL ? "null" : strPtr(httpHeaderToLog(this->header)), this->content == NULL ? 0 : bufUsed(this->content));
}
