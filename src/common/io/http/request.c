/***********************************************************************************************************************************
Http Request
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/request.h"
#include "common/log.h"
#include "common/type/object.h"

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
httpRequestNew(const String *verb, const String *uri, HttpRequestNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
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
String *
httpRequestToLog(const HttpRequest *this)
{
    return strNewFmt(
        "{verb: %s, uri: %s, query: %s, header: %s, contentSize: %zu",
        strPtr(this->verb), strPtr(this->uri), this->query == NULL ? "null" : strPtr(httpQueryToLog(this->query)),
        this->header == NULL ? "null" : strPtr(httpHeaderToLog(this->header)), this->content == NULL ? 0 : bufUsed(this->content));
}
