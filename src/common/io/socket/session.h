/***********************************************************************************************************************************
Socket Session

A simple socket session intended to allow access to services that are exposed via a socket.

Currently this is not a full-featured session and is only intended to isolate socket functionality from the tls code.
***********************************************************************************************************************************/
#ifndef COMMON_IO_SOCKET_SESSION_H
#define COMMON_IO_SOCKET_SESSION_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define SOCKET_SESSION_TYPE                                         SocketSession
#define SOCKET_SESSION_PREFIX                                       sckSession

typedef struct SocketSession SocketSession;

#include "common/time.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
SocketSession *sckSessionNew(int fd, const String *host, unsigned int port, TimeMSec timeout);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Check if there is data ready to read/write on the socket
void sckSessionReadyRead(SocketSession *this);
void sckSessionReadyWrite(SocketSession *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Socket file descriptor
int sckSessionFd(SocketSession *this);

// Socket host
const String *sckSessionHost(const SocketSession *this);

// Socket port
unsigned int sckSessionPort(const SocketSession *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void sckSessionFree(SocketSession *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *sckSessionToLog(const SocketSession *this);

#define FUNCTION_LOG_SOCKET_SESSION_TYPE                                                                                           \
    SocketSession *
#define FUNCTION_LOG_SOCKET_SESSION_FORMAT(value, buffer, bufferSize)                                                              \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, sckSessionToLog, buffer, bufferSize)

#endif
