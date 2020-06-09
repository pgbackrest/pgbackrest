/***********************************************************************************************************************************
Http Request
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/http/common.h"
#include "common/io/io.h"
#include "common/io/read.intern.h"
#include "common/io/tls/client.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpRequest
{
    MemContext *memContext;                                         // Mem context

    const String *verb;                                             // HTTP verb (GET, POST, etc.)
    const String *uri;                                              // HTTP uri
    const HttpQuery *query;                                         // HTTP query
    const HttpHeader *header;                                       // HTTP headers
    const Buffer *body                                              // HTTP body
};

OBJECT_DEFINE_MOVE(HTTP_REQUEST);
OBJECT_DEFINE_FREE(HTTP_REQUEST);

/**********************************************************************************************************************************/
HttpRequest *
httpRequestNew(const String *verb, const String *uri, const HttpQuery *query, const HttpHeader *header, const Buffer *body)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, uri);
        FUNCTION_LOG_PARAM(HTTP_QUERY, query);
        FUNCTION_LOG_PARAM(HTTP_HEADER, header);
        FUNCTION_LOG_PARAM(BUFFER, body);
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
            .query = httpQueryDup(query),
            .header = httpHeaderDup(header),
            .body = bufDup(header),
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
        "{verb: %s, uri: %s, query: %s, header: %s, bodySize: %s",
        strPtr(this->verb), strPtr(this->uri), strPtr(httpQueryToLog(this->query)), strPtr(httpQueryToLog(this->query)),
        cvtBoolToConstZ(this->contentChunked), this->contentSize, this->contentRemaining, cvtBoolToConstZ(this->closeOnContentEof),
        cvtBoolToConstZ(this->contentExists), cvtBoolToConstZ(this->contentEof), cvtBoolToConstZ(this->content != NULL));
}
