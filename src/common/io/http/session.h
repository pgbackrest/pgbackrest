/***********************************************************************************************************************************
HTTP Session

HTTP sessions are created by calling httpClientOpen().
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_SESSION_H
#define COMMON_IO_HTTP_SESSION_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define HTTP_SESSION_TYPE                                           HttpSession
#define HTTP_SESSION_PREFIX                                         httpSession

typedef struct HttpSession HttpSession;

#include "common/io/read.h"
#include "common/io/http/client.h"
#include "common/io/tls/session.h"
#include "common/io/write.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
HttpSession *httpSessionNew(HttpClient *client, TlsSession *session);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
HttpSession *httpSessionMove(HttpSession *this, MemContext *parentNew);

// Work with the session has finished cleanly and it can be reused
void httpSessionDone(HttpSession *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Read interface
IoRead *httpSessionIoRead(HttpSession *this);

// Write interface
IoWrite *httpSessionIoWrite(HttpSession *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void httpSessionFree(HttpSession *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_HTTP_SESSION_TYPE                                                                                             \
    HttpSession *
#define FUNCTION_LOG_HTTP_SESSION_FORMAT(value, buffer, bufferSize)                                                                \
    objToLog(value, "HttpSession", buffer, bufferSize)

#endif
