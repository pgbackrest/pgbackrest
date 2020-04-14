/***********************************************************************************************************************************
TLS Session

TLS sessions are created by calling tlsClientOpen().

TLS sessions are generally reused so the user must be prepared to retry their transaction on a read/write error if the server closes
the session before it is reused. If this behavior is not desirable then tlsSessionFree()/tlsClientOpen() can be called to get a new
session.
***********************************************************************************************************************************/
#ifndef COMMON_IO_TLS_SESSION_H
#define COMMON_IO_TLS_SESSION_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define TLS_SESSION_TYPE                                            TlsSession
#define TLS_SESSION_PREFIX                                          tlsSession

typedef struct TlsSession TlsSession;

#include "common/io/read.h"
#include "common/io/socket/session.h"
#include "common/io/write.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
TlsSession *tlsSessionMove(TlsSession *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Read interface
IoRead *tlsSessionIoRead(TlsSession *this);

// Write interface
IoWrite *tlsSessionIoWrite(TlsSession *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void tlsSessionFree(TlsSession *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_TLS_SESSION_TYPE                                                                                              \
    TlsSession *
#define FUNCTION_LOG_TLS_SESSION_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "TlsSession", buffer, bufferSize)

#endif
