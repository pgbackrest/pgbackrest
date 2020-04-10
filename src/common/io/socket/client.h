/***********************************************************************************************************************************
Socket Client

A simple socket client intended to allow access to services that are exposed via a socket.

Currently this is not a full-featured client and is only intended to isolate socket functionality from the tls code.
***********************************************************************************************************************************/
#ifndef COMMON_IO_SOCKET_CLIENT_H
#define COMMON_IO_SOCKET_CLIENT_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define SOCKET_CLIENT_TYPE                                          SocketClient
#define SOCKET_CLIENT_PREFIX                                        sckClient

typedef struct SocketClient SocketClient;

#include "common/io/read.h"
#include "common/io/socket/session.h"
#include "common/io/write.h"
#include "common/time.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Statistics
***********************************************************************************************************************************/
typedef struct SocketClientStat
{
    uint64_t object;                                                // Objects created
    uint64_t session;                                               // Sessions created
    uint64_t retry;                                                 // Connection retries
} SocketClientStat;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
SocketClient *sckClientNew(const String *host, unsigned int port, TimeMSec timeout);
SocketClient *sckClientNewServer(int fd, const String *host, unsigned int port, TimeMSec timeout);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open the connection
SocketSession *sckClientOpen(SocketClient *this);

// Move to a new parent mem context
SocketClient *sckClientMove(SocketClient *this, MemContext *parentNew);

// Statistics as a formatted string
String *sckClientStatStr(void);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *sckClientToLog(const SocketClient *this);

#define FUNCTION_LOG_SOCKET_CLIENT_TYPE                                                                                            \
    SocketClient *
#define FUNCTION_LOG_SOCKET_CLIENT_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, sckClientToLog, buffer, bufferSize)

#endif
