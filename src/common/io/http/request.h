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
typedef struct HttpRequestNewParam
{
    VAR_PARAM_HEADER;
    const HttpQuery *query;
    const HttpHeader *header;
    const Buffer *content;
} HttpRequestNewParam;

#define httpRequestNewP(verb, uri, ...)                                                                                      \
    httpRequestNew(verb, uri, (HttpRequestNewParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpRequest *httpRequestNew(const String *verb, const String *uri, HttpRequestNewParam param);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
HttpRequest *httpRequestMove(HttpRequest *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Request verb
const String *httpRequestVerb(const HttpRequest *this);

// Request URI
const String *httpRequestUri(const HttpRequest *this);

// Request query
const HttpQuery *httpRequestQuery(const HttpRequest *this);

// Request headers
const HttpHeader *httpRequestHeader(const HttpRequest *this);

// Request content
const Buffer *httpRequestContent(const HttpRequest *this);

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
