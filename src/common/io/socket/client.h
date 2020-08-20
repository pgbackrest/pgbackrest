/***********************************************************************************************************************************
Socket Client

A simple socket client intended to allow access to services that are exposed via a socket.
***********************************************************************************************************************************/
#ifndef COMMON_IO_SOCKET_CLIENT_H
#define COMMON_IO_SOCKET_CLIENT_H

#include "common/io/client.h"
#include "common/time.h"

/***********************************************************************************************************************************
Io client type
***********************************************************************************************************************************/
#define IO_CLIENT_SOCKET_TYPE                                       "socket"
    STRING_DECLARE(IO_CLIENT_SOCKET_TYPE_STR);

/***********************************************************************************************************************************
Statistics constants
***********************************************************************************************************************************/
#define SOCKET_STAT_CLIENT                                          "socket.client"         // Clients created
    STRING_DECLARE(SOCKET_STAT_CLIENT_STR);
#define SOCKET_STAT_RETRY                                           "socket.retry"          // Connection retries
    STRING_DECLARE(SOCKET_STAT_RETRY_STR);
#define SOCKET_STAT_SESSION                                         "socket.session"        // Sessions created
    STRING_DECLARE(SOCKET_STAT_SESSION_STR);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoClient *sckClientNew(const String *host, unsigned int port, TimeMSec timeout);

#endif
