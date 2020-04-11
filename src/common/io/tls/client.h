/***********************************************************************************************************************************
TLS Client

A simple, secure TLS client intended to allow access to services that are exposed via HTTPS.  We call it TLS instead of SSL because
SSL methods are disabled so only TLS connections are allowed.

This object is intended to be used for multiple TLS connections against a service so tlsClientOpen() can be called each time a new
connection is needed.  By default, an open connection will be reused so the user must be prepared to retry their transaction on a
read/write error if the server closes the connection before it is reused.  If this behavior is not desirable then tlsClientClose()
may be used to ensure that the next call to tlsClientOpen() will create a new TLS session.

Note that tlsClientRead() is non-blocking unless there are *zero* bytes to be read from the session in which case it will raise an
error after the defined timeout.  In any case the tlsClientRead()/tlsClientWrite()/tlsClientEof() functions should not generally
be called directly.  Instead use the read/write interfaces available from tlsClientIoRead()/tlsClientIoWrite().
***********************************************************************************************************************************/
#ifndef COMMON_IO_TLS_CLIENT_H
#define COMMON_IO_TLS_CLIENT_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define TLS_CLIENT_TYPE                                             TlsClient
#define TLS_CLIENT_PREFIX                                           tlsClient

typedef struct TlsClient TlsClient;

#include "common/io/socket/client.h"
#include "common/io/tls/session.h"

/***********************************************************************************************************************************
Statistics
***********************************************************************************************************************************/
typedef struct TlsClientStat
{
    uint64_t object;                                                // Objects created
    uint64_t session;                                               // Sessions created
    uint64_t retry;                                                 // Connection retries
} TlsClientStat;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
TlsClient *tlsClientNew(SocketClient *socket, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open tls session
TlsSession *tlsClientOpen(TlsClient *this);

// Statistics as a formatted string
String *tlsClientStatStr(void);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void tlsClientFree(TlsClient *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_TLS_CLIENT_TYPE                                                                                               \
    TlsClient *
#define FUNCTION_LOG_TLS_CLIENT_FORMAT(value, buffer, bufferSize)                                                                  \
    objToLog(value, "TlsClient", buffer, bufferSize)

#endif
