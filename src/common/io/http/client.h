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
typedef struct HttpClient HttpClient;

#include "common/io/client.h"
#include "common/io/http/session.h"
#include "common/time.h"

/***********************************************************************************************************************************
Statistics constants
***********************************************************************************************************************************/
#define HTTP_STAT_CLIENT                                            "http.client"       // Clients created
    STRING_DECLARE(HTTP_STAT_CLIENT_STR);
#define HTTP_STAT_CLOSE                                             "http.close"        // Closes forced by server
    STRING_DECLARE(HTTP_STAT_CLOSE_STR);
#define HTTP_STAT_REQUEST                                           "http.request"      // Requests (i.e. calls to httpRequestNew())
    STRING_DECLARE(HTTP_STAT_REQUEST_STR);
#define HTTP_STAT_RETRY                                             "http.retry"        // Request retries
    STRING_DECLARE(HTTP_STAT_RETRY_STR);
#define HTTP_STAT_SESSION                                           "http.session"      // Sessions created
    STRING_DECLARE(HTTP_STAT_SESSION_STR);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
HttpClient *httpClientNew(IoClient *ioClient, TimeMSec timeout);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct HttpClientPub
{
    TimeMSec timeout;                                               // Request timeout
} HttpClientPub;

__attribute__((always_inline)) static inline TimeMSec
httpClientTimeout(const HttpClient *this)
{
    ASSERT_INLINE(this != NULL);
    return ((const HttpClientPub *)this)->timeout;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open a new session
HttpSession *httpClientOpen(HttpClient *this);

// Request/response finished cleanly so session can be reused
void httpClientReuse(HttpClient *this, HttpSession *session);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *httpClientToLog(const HttpClient *this);

#define FUNCTION_LOG_HTTP_CLIENT_TYPE                                                                                              \
    HttpClient *
#define FUNCTION_LOG_HTTP_CLIENT_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, httpClientToLog, buffer, bufferSize)

#endif
