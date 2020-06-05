/***********************************************************************************************************************************
Http Response

???
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_RESPONSE_H
#define COMMON_IO_HTTP_RESPONSE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define HTTP_RESPONSE_TYPE                                          HttpResponse
#define HTTP_RESPONSE_PREFIX                                        httpResponse

typedef struct HttpResponse HttpResponse;

#include "common/io/http/header.h"
#include "common/io/read.h"

/***********************************************************************************************************************************
HTTP Response Constants
***********************************************************************************************************************************/
#define HTTP_RESPONSE_CODE_FORBIDDEN                                403
#define HTTP_RESPONSE_CODE_NOT_FOUND                                404

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
HttpResponse *httpResponseNew(HttpClient *client, IoRead *read, const String *verb, bool contentCache);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Is this response code OK, i.e. 2XX?
bool httpResponseCodeOk(const HttpResponse *this);

// Get response content. Content will be cached so it can be retrieved again without additional cost.
const Buffer *httpResponseContent(HttpResponse *this);

// No longer need to load new content. Cached content, headers, etc. will still be available.
void httpResponseDone(HttpResponse *this);

// Move to a new parent mem context
HttpResponse *httpResponseMove(HttpResponse *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Read interface
IoRead *httpResponseIoRead(HttpResponse *this);

// Get the response code
unsigned int httpResponseCode(const HttpResponse *this);

// Response headers
const HttpHeader *httpResponseHeader(const HttpResponse *this);

// Response reason
const String *httpResponseReason(const HttpResponse *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void httpResponseFree(HttpResponse *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *httpResponseToLog(const HttpResponse *this);

#define FUNCTION_LOG_HTTP_RESPONSE_TYPE                                                                                            \
    HttpResponse *
#define FUNCTION_LOG_HTTP_RESPONSE_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, httpResponseToLog, buffer, bufferSize)

#endif
