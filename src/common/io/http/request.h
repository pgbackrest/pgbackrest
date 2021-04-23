/***********************************************************************************************************************************
HTTP Request

Send a request to an HTTP server and get a response. The interface is natively asynchronous, i.e. httpRequestNew() sends a request
and httpRequestResponse() waits for a response. These can be called together for synchronous behavior or separately for asynchronous
behavior.
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_REQUEST_H
#define COMMON_IO_HTTP_REQUEST_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct HttpRequest HttpRequest;

#include "common/io/http/header.h"
#include "common/io/http/query.h"
#include "common/io/http/response.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
HTTP Constants
***********************************************************************************************************************************/
#define HTTP_VERSION                                                "HTTP/1.1"
    STRING_DECLARE(HTTP_VERSION_STR);
#define HTTP_VERSION_10                                             "HTTP/1.0"
    STRING_DECLARE(HTTP_VERSION_10_STR);

#define HTTP_VERB_DELETE                                            "DELETE"
    STRING_DECLARE(HTTP_VERB_DELETE_STR);
#define HTTP_VERB_GET                                               "GET"
    STRING_DECLARE(HTTP_VERB_GET_STR);
#define HTTP_VERB_HEAD                                              "HEAD"
    STRING_DECLARE(HTTP_VERB_HEAD_STR);
#define HTTP_VERB_POST                                              "POST"
    STRING_DECLARE(HTTP_VERB_POST_STR);
#define HTTP_VERB_PUT                                               "PUT"
    STRING_DECLARE(HTTP_VERB_PUT_STR);

#define HTTP_HEADER_AUTHORIZATION                                   "authorization"
    STRING_DECLARE(HTTP_HEADER_AUTHORIZATION_STR);
#define HTTP_HEADER_CONTENT_LENGTH                                  "content-length"
    STRING_DECLARE(HTTP_HEADER_CONTENT_LENGTH_STR);
#define HTTP_HEADER_CONTENT_MD5                                     "content-md5"
    STRING_DECLARE(HTTP_HEADER_CONTENT_MD5_STR);
#define HTTP_HEADER_CONTENT_RANGE                                   "content-range"
    STRING_DECLARE(HTTP_HEADER_CONTENT_RANGE_STR);
#define HTTP_HEADER_CONTENT_TYPE                                    "content-type"
    STRING_DECLARE(HTTP_HEADER_CONTENT_TYPE_STR);
#define HTTP_HEADER_CONTENT_TYPE_APP_FORM_URL                       "application/x-www-form-urlencoded"
    STRING_DECLARE(HTTP_HEADER_CONTENT_TYPE_APP_FORM_URL_STR);
#define HTTP_HEADER_CONTENT_RANGE_BYTES                             "bytes"
#define HTTP_HEADER_DATE                                            "date"
    STRING_DECLARE(HTTP_HEADER_DATE_STR);
#define HTTP_HEADER_ETAG                                            "etag"
    STRING_DECLARE(HTTP_HEADER_ETAG_STR);
#define HTTP_HEADER_HOST                                            "host"
    STRING_DECLARE(HTTP_HEADER_HOST_STR);
#define HTTP_HEADER_LAST_MODIFIED                                   "last-modified"
    STRING_DECLARE(HTTP_HEADER_LAST_MODIFIED_STR);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct HttpRequestNewParam
{
    VAR_PARAM_HEADER;
    const HttpQuery *query;
    const HttpHeader *header;
    const Buffer *content;
} HttpRequestNewParam;

#define httpRequestNewP(client, verb, path, ...)                                                                                   \
    httpRequestNew(client, verb, path, (HttpRequestNewParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpRequest *httpRequestNew(HttpClient *client, const String *verb, const String *path, HttpRequestNewParam param);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct HttpRequestPub
{
    MemContext *memContext;                                         // Mem context
    const String *verb;                                             // HTTP verb (GET, POST, etc.)
    const String *path;                                             // HTTP path
    const HttpQuery *query;                                         // HTTP query
    const HttpHeader *header;                                       // HTTP headers
} HttpRequestPub;

// Request path
__attribute__((always_inline)) static inline const String *
httpRequestPath(const HttpRequest *const this)
{
    return THIS_PUB(HttpRequest)->path;
}

// Request query
__attribute__((always_inline)) static inline const HttpQuery *
httpRequestQuery(const HttpRequest *const this)
{
    return THIS_PUB(HttpRequest)->query;
}

// Request headers
__attribute__((always_inline)) static inline const HttpHeader *
httpRequestHeader(const HttpRequest *const this)
{
    return THIS_PUB(HttpRequest)->header;
}

// Request verb
__attribute__((always_inline)) static inline const String *
httpRequestVerb(const HttpRequest *const this)
{
    return THIS_PUB(HttpRequest)->verb;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Wait for a response from the request
HttpResponse *httpRequestResponse(HttpRequest *this, bool contentCache);

// Throw an error if the request failed
void httpRequestError(const HttpRequest *this, HttpResponse *response) __attribute__((__noreturn__));

// Move to a new parent mem context
__attribute__((always_inline)) static inline HttpRequest *
httpRequestMove(HttpRequest *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
httpRequestFree(HttpRequest *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *httpRequestToLog(const HttpRequest *this);

#define FUNCTION_LOG_HTTP_REQUEST_TYPE                                                                                            \
    HttpRequest *
#define FUNCTION_LOG_HTTP_REQUEST_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, httpRequestToLog, buffer, bufferSize)

#endif
