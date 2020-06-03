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
HttpResponse *httpResponseNew(HttpClient *client, IoRead *read);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Is this response code OK, i.e. 2XX?
bool httpResponseCodeOk(const HttpResponse *this);

// Move to a new parent mem context
HttpHeader *httpResponseMove(HttpResponse *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Read interface
IoRead *httpResponseIoRead(const HttpResponse *this);

// Get the response code
unsigned int httpResponseCode(const HttpResponse *this);

// Response headers
const HttpHeader *httpResponseHeader(const HttpResponse *this);

// Response message
const String *httpResponseMessage(const HttpResponse *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void httpResponseFree(HttpResponse *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_HTTP_RESPONSE_TYPE                                                                                            \
    HttpResponse *
#define FUNCTION_LOG_HTTP_RESPONSE_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "HttpResponse", buffer, bufferSize)

#endif
