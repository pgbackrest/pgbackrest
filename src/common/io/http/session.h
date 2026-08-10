/***********************************************************************************************************************************
HTTP Session

HTTP sessions are created by calling httpClientOpen(), which is currently done exclusively by the HttpRequest object.
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_SESSION_H
#define COMMON_IO_HTTP_SESSION_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct HttpSession HttpSession;

#include "common/io/http/client.h"
#include "common/io/read.h"
#include "common/io/session.h"
#include "common/io/write.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN HttpSession *httpSessionNew(HttpClient *client, IoSession *session);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
FN_INLINE_ALWAYS HttpSession *
httpSessionMove(HttpSession *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Work with the session has finished cleanly and it can be reused
FN_EXTERN void httpSessionDone(HttpSession *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Read interface
typedef struct HttpSessionIoReadParam
{
    VAR_PARAM_HEADER;
    bool ignoreUnexpectedEof;
} HttpSessionIoReadParam;

#define httpSessionIoReadP(this, ...)                                                                                              \
    httpSessionIoRead(this, (HttpSessionIoReadParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN IoRead *httpSessionIoRead(HttpSession *this, HttpSessionIoReadParam param);

// Write interface
FN_EXTERN IoWrite *httpSessionIoWrite(HttpSession *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
httpSessionFree(HttpSession *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_HTTP_SESSION_TYPE                                                                                             \
    HttpSession *
#define FUNCTION_LOG_HTTP_SESSION_FORMAT(value, buffer, bufferSize)                                                                \
    objNameToLog(value, "HttpSession", buffer, bufferSize)

#endif
