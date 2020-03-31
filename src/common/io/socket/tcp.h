/***********************************************************************************************************************************
TCP Functions
***********************************************************************************************************************************/
#ifndef COMMON_IO_SOCKET_TCP_H
#define COMMON_IO_SOCKET_TCP_H

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Initialize settings to be used for all TCP connections
void tcpInit(bool keepAlive, int keepAliveCount, int keepAliveIdle, int keepAliveInterval);

// Set TCP options on a socket
void tcpOptionSet(int fd);

#endif
