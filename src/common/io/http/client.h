/***********************************************************************************************************************************
HTTP Client

A robust HTTP client with connection reuse and automatic retries.

Using a single object to make multiple requests is more efficient because connections are reused whenever possible.  Requests are
automatically retried when the connection has been closed by the server. Any 5xx response is also retried.

Only the HTTPS protocol is currently supported.

IMPORTANT NOTE: HttpClient should have a longer lifetime than any active HttpSession objects. This does not apply to HttpSession
objects that are freed, i.e. if an error occurs it does not matter in what order HttpClient and HttpSession objects are destroyed,
or HttpSession objects that have been returned to the client with httpClientReuse(). The danger is when an active HttpResponse
completes and tries to call httpClientReuse() on an HttpClient that has been freed thus causing a segfault.
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_CLIENT_H
#define COMMON_IO_HTTP_CLIENT_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define HTTP_CLIENT_TYPE                                            HttpClient
#define HTTP_CLIENT_PREFIX                                          httpClient

typedef struct HttpClient HttpClient;

#include "common/io/client.h"
#include "common/io/http/session.h"
#include "common/time.h"

/***********************************************************************************************************************************
Statistics
***********************************************************************************************************************************/
typedef struct HttpClientStat
{
    uint64_t object;                                                // Objects created
    uint64_t session;                                               // Sessions created
    uint64_t request;                                               // Requests (i.e. calls to httpRequestNew())
    uint64_t retry;                                                 // Request retries
    uint64_t close;                                                 // Closes forced by server
} HttpClientStat;

extern HttpClientStat httpClientStat;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
HttpClient *httpClientNew(IoClient *ioClient, TimeMSec timeout);

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
String *httpClientToLog(const HttpClient *this);

#define FUNCTION_LOG_HTTP_CLIENT_TYPE                                                                                              \
    HttpClient *
#define FUNCTION_LOG_HTTP_CLIENT_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, httpClientToLog, buffer, bufferSize)

#endif
