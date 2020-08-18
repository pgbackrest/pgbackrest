/***********************************************************************************************************************************
TLS Client

A simple, secure TLS client intended to allow access to services that are exposed via HTTPS. We call it TLS instead of SSL because
SSL methods are disabled so only TLS connections are allowed.

This object is intended to be used for multiple TLS sessions so ioClientOpen() can be called each time a new session is needed.
***********************************************************************************************************************************/
#ifndef COMMON_IO_TLS_CLIENT_H
#define COMMON_IO_TLS_CLIENT_H

#include "common/io/client.h"

/***********************************************************************************************************************************
Io client type
***********************************************************************************************************************************/
#define IO_CLIENT_TLS_TYPE                                          "tls"
    STRING_DECLARE(IO_CLIENT_TLS_TYPE_STR);

/***********************************************************************************************************************************
Statistics constants
***********************************************************************************************************************************/
#define TLS_STAT_CLIENT                                             "tls.client"        // Clients created
    STRING_DECLARE(TLS_STAT_CLIENT_STR);
#define TLS_STAT_RETRY                                              "tls.retry"        // Connection retries
    STRING_DECLARE(TLS_STAT_RETRY_STR);
#define TLS_STAT_SESSION                                            "tls.session"      // Sessions created
    STRING_DECLARE(TLS_STAT_SESSION_STR);

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
IoClient *tlsClientNew(
    IoClient *ioClient, const String *host, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Statistics as a formatted string
String *tlsClientStatStr(void);

#endif
