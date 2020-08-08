/***********************************************************************************************************************************
Socket Session

A simple socket session intended to allow access to services that are exposed via a socket.

Currently this is not a full-featured session and is only intended to isolate socket functionality from the tls code.
***********************************************************************************************************************************/
#ifndef COMMON_IO_SOCKET_SESSION_H
#define COMMON_IO_SOCKET_SESSION_H

/***********************************************************************************************************************************
Test result operations
***********************************************************************************************************************************/
typedef enum
{
    sckSessionTypeClient,
    sckSessionTypeServer,
} SocketSessionType;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define SOCKET_SESSION_TYPE                                         SocketSession
#define SOCKET_SESSION_PREFIX                                       sckSession

typedef struct SocketSession SocketSession;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/time.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
SocketSession *sckSessionNew(SocketSessionType type, int fd, const String *host, unsigned int port, TimeMSec timeout);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Read interface
IoRead *sckSessionIoRead(SocketSession *this);

// Write interface
IoWrite *sckSessionIoWrite(SocketSession *this);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
SocketSession *sckSessionMove(SocketSession *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Socket file descriptor
int sckSessionFd(SocketSession *this);

// Socket type
SocketSessionType sckSessionType(const SocketSession *this);

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
