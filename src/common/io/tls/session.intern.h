/***********************************************************************************************************************************
TLS Session Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_TLS_SESSION_INTERN_H
#define COMMON_IO_TLS_SESSION_INTERN_H

#include <openssl/ssl.h>

#include "common/io/tls/session.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
// Only called by TLS client/server code
TlsSession *tlsSessionNew(SSL *session, SocketSession *socketSession, TimeMSec timeout);

#endif
