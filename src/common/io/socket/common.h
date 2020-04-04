/***********************************************************************************************************************************
Socket Common Functions
***********************************************************************************************************************************/
#ifndef COMMON_IO_SOCKET_COMMON_H
#define COMMON_IO_SOCKET_COMMON_H

#include <time.h>

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Initialize settings for socket connections (some are used only for TCP)
void sckInit(bool keepAlive, int tcpKeepAliveCount, int tcpKeepAliveIdle, int tcpKeepAliveInterval);

// Set options on a socket
void sckOptionSet(int fd);

// !!!
bool sckPoll(int fd, bool read, bool write, uint64_t timeout);

#endif
