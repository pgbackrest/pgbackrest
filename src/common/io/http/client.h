/***********************************************************************************************************************************
Http Client

A robust HTTP client with connection reuse and automatic retries.

Using a single object to make multiple requests is more efficient because connections are reused whenever possible.  Requests are
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
#include "common/io/http/request.h"
#include "common/io/http/response.h"
#include "common/io/read.h"
#include "common/time.h"
#include "common/type/stringList.h"

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
#define HTTP_HEADER_ETAG                                            "etag"
    STRING_DECLARE(HTTP_HEADER_ETAG_STR);
#define HTTP_HEADER_HOST                                            "host"
    STRING_DECLARE(HTTP_HEADER_HOST_STR);
#define HTTP_HEADER_LAST_MODIFIED                                   "last-modified"
    STRING_DECLARE(HTTP_HEADER_LAST_MODIFIED_STR);

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
Constructors
***********************************************************************************************************************************/
HttpClient *httpClientNew(
    const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Is the http object busy?
bool httpClientBusy(const HttpClient *this);

// Mark the client as done if read is complete
void httpClientDone(HttpClient *this, bool close, bool closeRequired);

// Perform a request
HttpResponse *httpClientRequest(HttpClient *this, HttpRequest *request, bool contentCache);

// Format statistics to a string
String *httpClientStatStr(void);

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
