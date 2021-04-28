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
typedef struct HttpResponse HttpResponse;

#include "common/io/http/header.h"
#include "common/io/http/session.h"
#include "common/io/read.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
HTTP Response Constants
***********************************************************************************************************************************/
#define HTTP_RESPONSE_CODE_PERMANENT_REDIRECT                       308
#define HTTP_RESPONSE_CODE_FORBIDDEN                                403
#define HTTP_RESPONSE_CODE_NOT_FOUND                                404

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
HttpResponse *httpResponseNew(HttpSession *session, const String *verb, bool contentCache);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct HttpResponsePub
{
    MemContext *memContext;                                         // Mem context
    IoRead *contentRead;                                            // Read interface for response content
    unsigned int code;                                              // Response code (e.g. 200, 404)
    HttpHeader *header;                                             // Response headers
    String *reason;                                                 // Response reason e.g. (OK, Not Found)
} HttpResponsePub;

// Read interface used to get the response content. This is intended for reading content that may be very large and will not be held
// in memory all at once. If the content must be loaded completely for processing (e.g. XML) then httpResponseContent() is simpler.
__attribute__((always_inline)) static inline IoRead *
httpResponseIoRead(HttpResponse *const this)
{
    return THIS_PUB(HttpResponse)->contentRead;
}

// Response code
__attribute__((always_inline)) static inline unsigned int
httpResponseCode(const HttpResponse *const this)
{
    return THIS_PUB(HttpResponse)->code;
}

// Response headers
__attribute__((always_inline)) static inline const HttpHeader *
httpResponseHeader(const HttpResponse *const this)
{
    return THIS_PUB(HttpResponse)->header;
}

// Response reason
__attribute__((always_inline)) static inline const String *
httpResponseReason(const HttpResponse *const this)
{
    return THIS_PUB(HttpResponse)->reason;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Is this response code OK, i.e. 2XX?
__attribute__((always_inline)) static inline bool
httpResponseCodeOk(const HttpResponse *const this)
{
    return httpResponseCode(this) / 100 == 2;
}

// Fetch all response content. Content will be cached so it can be retrieved again without additional cost.
const Buffer *httpResponseContent(HttpResponse *this);

// Move to a new parent mem context
__attribute__((always_inline)) static inline HttpResponse *
httpResponseMove(HttpResponse *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
httpResponseFree(HttpResponse *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *httpResponseToLog(const HttpResponse *this);

#define FUNCTION_LOG_HTTP_RESPONSE_TYPE                                                                                            \
    HttpResponse *
#define FUNCTION_LOG_HTTP_RESPONSE_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, httpResponseToLog, buffer, bufferSize)

#endif
