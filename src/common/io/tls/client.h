/***********************************************************************************************************************************
TLS Client

This is a simple, secure TLS client intended to allow access to services that are exposed via HTTPS.  We call is TLS instead of SSL
because SSL methods are disabled so only TLS connections are allowed.

This object is intended to be used for multiple TLS connections against a service so tlsClientOpen() can be called each time a new
connection is needed.  By default, an open connection will be reused for pipelining so the user must be prepared to retry their
transaction on a read/write error if the server closes the connection before it can be reused.  If this behavior is not desirable
then tlsClientClose() may be used to ensure that the next call to tlsClientOpen() will create a new TLS session.

Note that tlsClientRead() is non-blocking unless there are *zero* bytes to be read from the session in which case it will raise an
error after the defined timeout.  In any case the tlsClientRead()/tlsClientWrite()/tlsClientEof() functions should not generally
be called directly.  Instead use the read/write interfaces available from tlsClientIoRead()/tlsClientIoWrite().
***********************************************************************************************************************************/
#ifndef COMMON_IO_TLS_CLIENT_H
#define COMMON_IO_TLS_CLIENT_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct TlsClient TlsClient;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/time.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
TlsClient *tlsClientNew(
    const String *host, unsigned int port, TimeMSec timeout, bool verifySsl, const String *caFile, const String *caPath);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void tlsClientOpen(TlsClient *this);
size_t tlsClientRead(TlsClient *this, Buffer *buffer);
void tlsClientWrite(TlsClient *this, const Buffer *buffer);
void tlsClientClose(TlsClient *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool tlsClientEof(const TlsClient *this);
IoRead *tlsClientIoRead(const TlsClient *this);
IoWrite *tlsClientIoWrite(const TlsClient *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void tlsClientFree(TlsClient *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_TLS_CLIENT_TYPE                                                                                             \
    TlsClient *
#define FUNCTION_DEBUG_TLS_CLIENT_FORMAT(value, buffer, bufferSize)                                                                \
    objToLog(value, "TlsClient", buffer, bufferSize)

#endif
