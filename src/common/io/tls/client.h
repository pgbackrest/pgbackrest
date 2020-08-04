/***********************************************************************************************************************************
TLS Client

A simple, secure TLS client intended to allow access to services that are exposed via HTTPS. We call it TLS instead of SSL because
SSL methods are disabled so only TLS connections are allowed.

This object is intended to be used for multiple TLS sessions so tlsClientOpen() can be called each time a new session is needed.
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
#include "common/io/session.h"

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
// Open TLS session
IoSession *tlsClientOpen(TlsClient *this);

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
