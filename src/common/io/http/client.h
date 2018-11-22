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
typedef struct HttpClient HttpClient;

#include "common/io/http/header.h"
#include "common/io/http/query.h"
#include "common/io/read.h"
#include "common/time.h"
#include "common/type/stringList.h"

/***********************************************************************************************************************************
HTTP Constants
***********************************************************************************************************************************/
#define HTTP_VERB_GET                                               "GET"
    STRING_DECLARE(HTTP_VERB_GET_STR);

#define HTTP_HEADER_CONTENT_LENGTH                                  "content-length"
    STRING_DECLARE(HTTP_HEADER_CONTENT_LENGTH_STR);

#define HTTP_RESPONSE_CODE_OK                                       200
#define HTTP_RESPONSE_CODE_FORBIDDEN                                403
#define HTTP_RESPONSE_CODE_NOT_FOUND                                404

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
HttpClient *httpClientNew(
    const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Buffer *httpClientRequest(
    HttpClient *this, const String *verb, const String *uri, const HttpQuery *query, const HttpHeader *requestHeader,
    bool returnContent);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
IoRead *httpClientIoRead(const HttpClient *this);
unsigned int httpClientResponseCode(const HttpClient *this);
const HttpHeader *httpClientReponseHeader(const HttpClient *this);
const String *httpClientResponseMessage(const HttpClient *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void httpClientFree(HttpClient *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_HTTP_CLIENT_TYPE                                                                                            \
    HttpClient *
#define FUNCTION_DEBUG_HTTP_CLIENT_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "HttpClient", buffer, bufferSize)

#endif
