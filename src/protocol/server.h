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
#include "protocol/command.h"

/***********************************************************************************************************************************
Protocol command handler type and structure

An array of this struct must be passed to protocolServerProcess() for the server to process commands. Each command handler should
implement a single command, as defined by the command string.
***********************************************************************************************************************************/
typedef void *(*ProtocolServerCommandOpenHandler)(PackRead *param, ProtocolServer *server, uint64_t sessionId);
typedef void (*ProtocolServerCommandProcessHandler)(PackRead *param, ProtocolServer *server);
typedef bool (*ProtocolServerCommandProcessSessionHandler)(PackRead *param, ProtocolServer *server, void *sessionData);
typedef void (*ProtocolServerCommandCloseHandler)(PackRead *param, ProtocolServer *server, void *sessionData);

typedef struct ProtocolServerHandler
{
    StringId command;                                               // 5-bit StringId that identifies the protocol command
    ProtocolServerCommandOpenHandler open;                          // Function that opens the protocol session
    ProtocolServerCommandProcessHandler process;                    // Function that processes the protocol command
    ProtocolServerCommandProcessSessionHandler processSession;      // Function that processes the protocol command for a session
    ProtocolServerCommandCloseHandler close;                        // Function that closes the protocol session
} ProtocolServerHandler;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN ProtocolServer *protocolServerNew(const String *name, const String *service, IoRead *read, IoWrite *write);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get command from the client. Outside ProtocolServer, this is used when the first noop command needs to be processed before
// running protocolServerProcess(), which allows an error to be returned to the client if initialization fails.
typedef struct ProtocolServerCommandGetResult
{
    StringId id;                                                    // Command identifier
    ProtocolCommandType type;                                       // Command type
    uint64_t sessionId;                                             // Session id
    Pack *param;                                                    // Parameter pack
} ProtocolServerCommandGetResult;

FN_EXTERN ProtocolServerCommandGetResult protocolServerCommandGet(ProtocolServer *this);

// Put data to the client
FN_EXTERN void protocolServerDataPut(ProtocolServer *this, PackWrite *data);

// Return an error
FN_EXTERN void protocolServerError(ProtocolServer *this, int code, const String *message, const String *stack);

// Process requests
FN_EXTERN void protocolServerProcess(
    ProtocolServer *this, const VariantList *retryInterval, const ProtocolServerHandler *handlerList,
    const unsigned int handlerListSize);

// Move to a new parent mem context
FN_INLINE_ALWAYS ProtocolServer *
protocolServerMove(ProtocolServer *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
protocolServerFree(ProtocolServer *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void protocolServerToLog(const ProtocolServer *this, StringStatic *debugLog);

#define FUNCTION_LOG_PROTOCOL_SERVER_TYPE                                                                                          \
    ProtocolServer *
#define FUNCTION_LOG_PROTOCOL_SERVER_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_OBJECT_FORMAT(value, protocolServerToLog, buffer, bufferSize)

#endif
