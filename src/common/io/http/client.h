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

#include "common/io/http/session.h"
#include "common/time.h"

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

    TimeMSec writeRequestMs;                                        // Total time spent sending write requests
    TimeMSec writeResponseMs;                                       // Total time spent waiting for write sync responses
    TimeMSec writeResponseAsyncMs;                                  // Total time spent waiting for write async responses
} HttpClientStat;

extern HttpClientStat httpClientStat;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
HttpClient *httpClientNew(
    const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open a new session
HttpSession *httpClientOpen(HttpClient *this);

// Request/response finished cleanly so session can be reused
void httpClientReuse(HttpClient *this, HttpSession *session);

// Format statistics to a string
String *httpClientStatStr(void);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
TimeMSec httpClientTimeout(const HttpClient *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_HTTP_CLIENT_TYPE                                                                                              \
    HttpClient *
#define FUNCTION_LOG_HTTP_CLIENT_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "HttpClient", buffer, bufferSize)

#endif
