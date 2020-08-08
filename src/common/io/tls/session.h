/***********************************************************************************************************************************
TLS Session

TLS sessions are created by calling ioClientOpen().

TLS sessions are generally reused so the user must be prepared to retry their transaction on a read/write error if the server closes
the session before it is reused. If this behavior is not desirable then ioSessionFree()/ioClientOpen() can be called to get a new
session.
***********************************************************************************************************************************/
#ifndef COMMON_IO_TLS_SESSION_H
#define COMMON_IO_TLS_SESSION_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#include <openssl/ssl.h>

#include "common/io/session.h"
#include "common/io/socket/session.h"
#include "common/time.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
// Only called by TLS client/server code
IoSession *tlsSessionNew(SSL *session, SocketSession *socketSession, TimeMSec timeout);

#endif
