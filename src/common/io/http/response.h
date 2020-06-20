/***********************************************************************************************************************************
HTTP Response

Response created after a successful request. Once the content is read the underlying connection may be recycled but the headers,
cached content, etc. will still be available for the lifetime of the object.
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
#include "common/io/http/session.h"
#include "common/io/read.h"

/***********************************************************************************************************************************
HTTP Response Constants
***********************************************************************************************************************************/
#define HTTP_RESPONSE_CODE_FORBIDDEN                                403
#define HTTP_RESPONSE_CODE_NOT_FOUND                                404

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
HttpResponse *httpResponseNew(HttpSession *session, const String *verb, bool contentCache);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Is this response code OK, i.e. 2XX?
bool httpResponseCodeOk(const HttpResponse *this);

// Get response content. Content will be cached so it can be retrieved again without additional cost.
const Buffer *httpResponseContent(HttpResponse *this);

// Move to a new parent mem context
HttpResponse *httpResponseMove(HttpResponse *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Is the response still being read?
bool httpResponseBusy(const HttpResponse *this);

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
