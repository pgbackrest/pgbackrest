/***********************************************************************************************************************************
Http Request

???
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_REQUEST_H
#define COMMON_IO_HTTP_REQUEST_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define HTTP_REQUEST_TYPE                                          HttpRequest
#define HTTP_REQUEST_PREFIX                                        httpRequest

typedef struct HttpRequest HttpRequest;

#include "common/io/http/header.h"
#include "common/io/http/query.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
HttpRequest *httpRequestNew(
    const String *verb, const String *uri, const HttpQuery *query, const HttpHeader *header, const Buffer *body);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// // Is this response code OK, i.e. 2XX?
// bool httpRequestCodeOk(const HttpRequest *this);
//
// // Get response content. Content will be cached so it can be retrieved again without additional cost.
// const Buffer *httpRequestContent(HttpRequest *this);
//
// // No longer need to load new content. Cached content, headers, etc. will still be available.
// void httpRequestDone(HttpRequest *this);

// Move to a new parent mem context
HttpRequest *httpRequestMove(HttpRequest *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// // Is the response still being read?
// bool httpRequestBusy(const HttpRequest *this);
//
// // Read interface
// IoRead *httpRequestIoRead(HttpRequest *this);
//
// // Get the response code
// unsigned int httpRequestCode(const HttpRequest *this);
//
// // Response headers
// const HttpHeader *httpRequestHeader(const HttpRequest *this);
//
// // Response reason
// const String *httpRequestReason(const HttpRequest *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void httpRequestFree(HttpRequest *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *httpRequestToLog(const HttpRequest *this);

#define FUNCTION_LOG_HTTP_REQUEST_TYPE                                                                                            \
    HttpRequest *
#define FUNCTION_LOG_HTTP_REQUEST_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, httpRequestToLog, buffer, bufferSize)

#endif
