/***********************************************************************************************************************************
Socket Common Functions
***********************************************************************************************************************************/
#ifndef COMMON_IO_SOCKET_COMMON_H
#define COMMON_IO_SOCKET_COMMON_H

#include <netdb.h>

#include "common/time.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Initialize settings for socket connections (some are used only for TCP)
FN_EXTERN void sckInit(bool block, bool keepAlive, int tcpKeepAliveCount, int tcpKeepAliveIdle, int tcpKeepAliveInterval);

// Get address info for a host/address. The caller is responsible for freeing addrinfo.
FN_EXTERN struct addrinfo *sckHostLookup(const String *const host, unsigned int port);

// Set options on a socket
FN_EXTERN void sckOptionSet(int fd);

// Connect socket to an IP address
FN_EXTERN void sckConnect(int fd, const String *host, unsigned int port, const struct addrinfo *hostAddress, TimeMSec timeout);

#endif
