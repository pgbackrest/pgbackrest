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
typedef struct HttpRequestMulti HttpRequestMulti;

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
#define HTTP_HEADER_CONTENT_ID                                      "content-id"
STRING_DECLARE(HTTP_HEADER_CONTENT_ID_STR);
#define HTTP_HEADER_CONTENT_LENGTH                                  "content-length"
STRING_DECLARE(HTTP_HEADER_CONTENT_LENGTH_STR);
#define HTTP_HEADER_CONTENT_MD5                                     "content-md5"
STRING_DECLARE(HTTP_HEADER_CONTENT_MD5_STR);
#define HTTP_HEADER_CONTENT_RANGE                                   "content-range"
STRING_DECLARE(HTTP_HEADER_CONTENT_RANGE_STR);
#define HTTP_HEADER_CONTENT_TRANSFER_ENCODING                       "content-transfer-encoding"
#define HTTP_HEADER_CONTENT_TRANSFER_ENCODING_BINARY                "binary"
#define HTTP_HEADER_CONTENT_TYPE                                    "content-type"
STRING_DECLARE(HTTP_HEADER_CONTENT_TYPE_STR);
#define HTTP_HEADER_CONTENT_TYPE_APP_FORM_URL                       "application/x-www-form-urlencoded"
STRING_DECLARE(HTTP_HEADER_CONTENT_TYPE_APP_FORM_URL_STR);
#define HTTP_HEADER_CONTENT_TYPE_HTTP                               "application/http"
STRING_DECLARE(HTTP_HEADER_CONTENT_TYPE_HTTP_STR);
#define HTTP_HEADER_CONTENT_TYPE_JSON                               "application/json"
STRING_DECLARE(HTTP_HEADER_CONTENT_TYPE_JSON_STR);
#define HTTP_HEADER_CONTENT_TYPE_MULTIPART                          "multipart/mixed; boundary="
#define HTTP_HEADER_CONTENT_TYPE_XML                                "application/xml"
STRING_DECLARE(HTTP_HEADER_CONTENT_TYPE_XML_STR);
#define HTTP_HEADER_CONTENT_RANGE_BYTES                             "bytes"
#define HTTP_HEADER_DATE                                            "date"
STRING_DECLARE(HTTP_HEADER_DATE_STR);
#define HTTP_HEADER_ETAG                                            "etag"
STRING_DECLARE(HTTP_HEADER_ETAG_STR);
#define HTTP_HEADER_HOST                                            "host"
STRING_DECLARE(HTTP_HEADER_HOST_STR);
#define HTTP_HEADER_LAST_MODIFIED                                   "last-modified"
STRING_DECLARE(HTTP_HEADER_LAST_MODIFIED_STR);
#define HTTP_HEADER_RANGE                                           "range"
STRING_DECLARE(HTTP_HEADER_RANGE_STR);
#define HTTP_HEADER_RANGE_BYTES                                     "bytes"

#define HTTP_MULTIPART_BOUNDARY_INIT                                "QKX4EYg4"
#define HTTP_MULTIPART_BOUNDARY_NEXT                                4
#define HTTP_MULTIPART_BOUNDARY_EXTRA                               "LARJ52gF4F239iNVrrC5w5aYskcGrWCXIFlMp5IxswggIhcX2A0gF9nrgN8q"
#define HTTP_MULTIPART_BOUNDARY_PRE                                 "\r\n--"
#define HTTP_MULTIPART_BOUNDARY_POST                                "\r\n"
#define HTTP_MULTIPART_BOUNDARY_POST_LAST                           "--\r\n"

/***********************************************************************************************************************************
Request Constructors
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

FN_EXTERN HttpRequest *httpRequestNew(HttpClient *client, const String *verb, const String *path, HttpRequestNewParam param);

/***********************************************************************************************************************************
Request Getters/Setters
***********************************************************************************************************************************/
typedef struct HttpRequestPub
{
    const String *verb;                                             // HTTP verb (GET, POST, etc.)
    const String *path;                                             // HTTP path
    const HttpQuery *query;                                         // HTTP query
    const HttpHeader *header;                                       // HTTP headers
} HttpRequestPub;

// Request path
FN_INLINE_ALWAYS const String *
httpRequestPath(const HttpRequest *const this)
{
    return THIS_PUB(HttpRequest)->path;
}

// Request query
FN_INLINE_ALWAYS const HttpQuery *
httpRequestQuery(const HttpRequest *const this)
{
    return THIS_PUB(HttpRequest)->query;
}

// Request headers
FN_INLINE_ALWAYS const HttpHeader *
httpRequestHeader(const HttpRequest *const this)
{
    return THIS_PUB(HttpRequest)->header;
}

// Request verb
FN_INLINE_ALWAYS const String *
httpRequestVerb(const HttpRequest *const this)
{
    return THIS_PUB(HttpRequest)->verb;
}

/***********************************************************************************************************************************
Request Functions
***********************************************************************************************************************************/
// Wait for a response from the request
FN_EXTERN HttpResponse *httpRequestResponse(HttpRequest *this, bool contentCache);

// Throw an error if the request failed
FN_EXTERN FN_NO_RETURN void httpRequestError(const HttpRequest *this, HttpResponse *response);

// Move to a new parent mem context
FN_INLINE_ALWAYS HttpRequest *
httpRequestMove(HttpRequest *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Request Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
httpRequestFree(HttpRequest *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Request Multi Constructors
***********************************************************************************************************************************/
FN_EXTERN HttpRequestMulti *httpRequestMultiNew(void);

/***********************************************************************************************************************************
Request Multi Functions
***********************************************************************************************************************************/
// Add a request to multipart content
#define httpRequestMultiAddP(this, contentId, verb, path, ...)                                                                                \
    httpRequestMultiAdd(this, contentId, verb, path, (HttpRequestNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN void httpRequestMultiAdd(
    HttpRequestMulti *this, const String *contentId, const String *verb, const String *path, HttpRequestNewParam param);

// Concatenate multipart content
FN_EXTERN Buffer *httpRequestMultiContent(HttpRequestMulti *this);

// Add multipart header
FN_EXTERN HttpHeader *httpRequestMultiHeaderAdd(HttpRequestMulti *this, HttpHeader *header);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void httpRequestToLog(const HttpRequest *this, StringStatic *debugLog);

#define FUNCTION_LOG_HTTP_REQUEST_TYPE                                                                                             \
    HttpRequest *
#define FUNCTION_LOG_HTTP_REQUEST_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_OBJECT_FORMAT(value, httpRequestToLog, buffer, bufferSize)

#define FUNCTION_LOG_HTTP_REQUEST_MULTI_TYPE                                                                                       \
    HttpRequestMulti *
#define FUNCTION_LOG_HTTP_REQUEST_MULTI_FORMAT(value, buffer, bufferSize)                                                          \
    objNameToLog(value, "HttpRequestMulti", buffer, bufferSize)

#endif
