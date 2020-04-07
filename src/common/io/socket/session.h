/***********************************************************************************************************************************
Socket Session
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
#define SOCKET_SESSION_TYPE                                          SocketSession
#define SOCKET_SESSION_PREFIX                                        sckSession

typedef struct SocketSession SocketSession;

#include "common/time.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
SocketSession *sckSessionNew(SocketSessionType type, int fd, const String *host, unsigned int port, TimeMSec timeout);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Check if there is data ready to read or write on the socket
void sckSessionReady(SocketSession *this, bool read, bool write);

// Move the socket to a new parent mem context
SocketSession *sckSessionMove(SocketSession *this, MemContext *parentNew);

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

#define FUNCTION_LOG_SOCKET_SESSION_TYPE                                                                                            \
    SocketSession *
#define FUNCTION_LOG_SOCKET_SESSION_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, sckSessionToLog, buffer, bufferSize)

#endif
