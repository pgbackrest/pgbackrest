/***********************************************************************************************************************************
Http Client

A robust HTTP client with pipelining support and automatic retries.

Using a single object to make multiple requests is more efficient because requests are piplelined whenever possible.  Requests are
automatically retried when the connection has been closed by the server.  Any 5xx response is also retried.

Only the HTTPS protocol is currently supported.
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_CLIENT_H
#define COMMON_IO_HTTP_CLIENT_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define HTTP_CLIENT_TYPE                                            HttpClient
#define HTTP_CLIENT_PREFIX                                          httpClient

typedef struct HttpClient HttpClient;

#include "common/io/http/header.h"
#include "common/io/http/query.h"
#include "common/io/read.h"
#include "common/time.h"
#include "common/type/stringList.h"

/***********************************************************************************************************************************
HTTP Constants
***********************************************************************************************************************************/
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

#define HTTP_HEADER_CONTENT_LENGTH                                  "content-length"
    STRING_DECLARE(HTTP_HEADER_CONTENT_LENGTH_STR);
#define HTTP_HEADER_CONTENT_MD5                                     "content-md5"
    STRING_DECLARE(HTTP_HEADER_CONTENT_MD5_STR);
#define HTTP_HEADER_ETAG                                            "etag"
    STRING_DECLARE(HTTP_HEADER_ETAG_STR);

#define HTTP_RESPONSE_CODE_FORBIDDEN                                403
#define HTTP_RESPONSE_CODE_NOT_FOUND                                404

/***********************************************************************************************************************************
Statistics
***********************************************************************************************************************************/
typedef struct HttpClientStat
{
    uint64_t object;                                                // Objects created
    uint64_t session;                                               // TLS sessions created
    uint64_t request;                                               // Requests (i.e. calls to httpClientRequest())
    uint64_t retry;                                                 // Request retries
    uint64_t close;                                                 // Closes forced by server
} HttpClientStat;

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
HttpClient *httpClientNew(
    const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void httpClientDone(HttpClient *this);
Buffer *httpClientRequest(
    HttpClient *this, const String *verb, const String *uri, const HttpQuery *query, const HttpHeader *requestHeader,
    const Buffer *body, bool returnContent);
String *httpClientStatStr(void);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool httpClientBusy(const HttpClient *this);
IoRead *httpClientIoRead(const HttpClient *this);
unsigned int httpClientResponseCode(const HttpClient *this);
const HttpHeader *httpClientResponseHeader(const HttpClient *this);
const String *httpClientResponseMessage(const HttpClient *this);
bool httpClientResponseCodeOk(const HttpClient *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void httpClientFree(HttpClient *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_HTTP_CLIENT_TYPE                                                                                              \
    HttpClient *
#define FUNCTION_LOG_HTTP_CLIENT_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "HttpClient", buffer, bufferSize)

#endif
