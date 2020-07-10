/***********************************************************************************************************************************
HTTP Request

Send a request to an HTTP server and get a response. The interface is natively asynchronous, i.e. httpRequestNew() sends a request
and httpRequest() waits for a response. These can be called together for synchronous behavior or separately for asynchronous
behavior.
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_REQUEST_H
#define COMMON_IO_HTTP_REQUEST_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define HTTP_REQUEST_TYPE                                          HttpRequest
#define HTTP_REQUEST_PREFIX                                        httpRequest

typedef struct HttpRequest HttpRequest;

#include "common/io/http/header.h"
#include "common/io/http/query.h"
#include "common/io/http/response.h"

/***********************************************************************************************************************************
HTTP Constants
***********************************************************************************************************************************/
#define HTTP_VERSION                                                "HTTP/1.1"
    STRING_DECLARE(HTTP_VERSION_STR);

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

#define httpRequestNewP(client, verb, uri, ...)                                                                                    \
    httpRequestNew(client, verb, uri, (HttpRequestNewParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpRequest *httpRequestNew(HttpClient *client, const String *verb, const String *uri, HttpRequestNewParam param);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Send a request to the server
HttpResponse *httpRequest(HttpRequest *this, bool contentCache);

// Throw an error if the request failed
void httpRequestError(const HttpRequest *this, HttpResponse *response) __attribute__((__noreturn__));

// Move to a new parent mem context
HttpRequest *httpRequestMove(HttpRequest *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Request verb
const String *httpRequestVerb(const HttpRequest *this);

// Request URI
const String *httpRequestUri(const HttpRequest *this);

// Request query
const HttpQuery *httpRequestQuery(const HttpRequest *this);

// Request headers
const HttpHeader *httpRequestHeader(const HttpRequest *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void httpRequestFree(HttpRequest *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *httpRequestToLog(const HttpRequest *this);

#define FUNCTION_LOG_HTTP_REQUEST_TYPE                                                                                            \
    HttpRequest *
#define FUNCTION_LOG_HTTP_REQUEST_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, httpRequestToLog, buffer, bufferSize)

#endif
