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

#include "common/io/read.h"
#include "common/io/http/client.h"
#include "common/io/session.h"
#include "common/io/write.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
HttpSession *httpSessionNew(HttpClient *client, IoSession *session);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
__attribute__((always_inline)) static inline HttpSession *
httpSessionMove(HttpSession *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Work with the session has finished cleanly and it can be reused
void httpSessionDone(HttpSession *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Read interface
IoRead *httpSessionIoRead(HttpSession *this, bool ignoreUnexpectedEof);

// Write interface
IoWrite *httpSessionIoWrite(HttpSession *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
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
    objToLog(value, "HttpSession", buffer, bufferSize)

#endif
