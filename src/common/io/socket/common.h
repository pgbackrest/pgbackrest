/***********************************************************************************************************************************
Socket Common Functions
***********************************************************************************************************************************/
#ifndef COMMON_IO_SOCKET_COMMON_H
#define COMMON_IO_SOCKET_COMMON_H

#include <netdb.h>

#include "common/time.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Initialize settings for socket connections (some are used only for TCP)
void sckInit(bool keepAlive, int tcpKeepAliveCount, int tcpKeepAliveIdle, int tcpKeepAliveInterval);

// Set options on a socket
void sckOptionSet(int fd);

// Connect socket to an IP address
void sckConnect(int fd, const String *host, unsigned int port, const struct addrinfo *hostAddress, TimeMSec timeout);

// Wait until the socket is ready to read/write or timeout
bool sckReady(int fd, bool read, bool write, TimeMSec timeout);
bool sckReadyRead(int fd, TimeMSec timeout);
bool sckReadyWrite(int fd, TimeMSec timeout);

#endif
