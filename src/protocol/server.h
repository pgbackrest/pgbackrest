/***********************************************************************************************************************************
Protocol Server
***********************************************************************************************************************************/
#ifndef PROTOCOL_SERVER_H
#define PROTOCOL_SERVER_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct ProtocolServer ProtocolServer;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/object.h"
#include "common/type/pack.h"
#include "common/type/stringId.h"
#include "protocol/client.h"

/***********************************************************************************************************************************
Protocol command handler type and structure

An array of this struct must be passed to protocolServerProcess() for the server to process commands. Each command handler should
implement a single command, as defined by the command string.
***********************************************************************************************************************************/
typedef void (*ProtocolServerCommandHandler)(PackRead *param, ProtocolServer *server);

typedef struct ProtocolServerHandler
{
    StringId command;                                               // 5-bit StringId that identifies the protocol command
    ProtocolServerCommandHandler handler;                           // Function that handles the protocol command
} ProtocolServerHandler;

#define PROTOCOL_SERVER_HANDLER_LIST_SIZE(list)                     (sizeof(list) / sizeof(ProtocolServerHandler))

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
ProtocolServer *protocolServerNew(const String *name, const String *service, IoRead *read, IoWrite *write);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get command from the client. Outside ProtocolServer, this is used when the first noop command needs to be processed before
// running protocolServerProcess(), which allows an error to be returned to the client if initialization fails.
typedef struct ProtocolServerCommandGetResult
{
    StringId id;                                                    // Command identifier
    Buffer *param;                                                  // Parameter pack
} ProtocolServerCommandGetResult;

ProtocolServerCommandGetResult protocolServerCommandGet(ProtocolServer *this);

// Get data from the client
PackRead *protocolServerDataGet(ProtocolServer *this);

// Put data to the client
void protocolServerDataPut(ProtocolServer *this, PackWrite *resultPack);

// Put data end to the client. This ends command processing and no more data should be sent.
void protocolServerDataEndPut(ProtocolServer *this);

// Return an error
void protocolServerError(ProtocolServer *this, int code, const String *message, const String *stack);

// Process requests
void protocolServerProcess(
    ProtocolServer *this, const VariantList *retryInterval, const ProtocolServerHandler *handlerList,
    const unsigned int handlerListSize);

// Move to a new parent mem context
__attribute__((always_inline)) static inline ProtocolServer *
protocolServerMove(ProtocolServer *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
protocolServerFree(ProtocolServer *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *protocolServerToLog(const ProtocolServer *this);

#define FUNCTION_LOG_PROTOCOL_SERVER_TYPE                                                                                          \
    ProtocolServer *
#define FUNCTION_LOG_PROTOCOL_SERVER_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, protocolServerToLog, buffer, bufferSize)

#endif
