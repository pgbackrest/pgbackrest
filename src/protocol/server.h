/***********************************************************************************************************************************
Protocol Server
***********************************************************************************************************************************/
#ifndef PROTOCOL_SERVER_H
#define PROTOCOL_SERVER_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct ProtocolServer ProtocolServer;
typedef struct ProtocolServerResult ProtocolServerResult;

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
typedef ProtocolServerResult *(*ProtocolServerCommandOpenHandler)(PackRead *param);
typedef ProtocolServerResult *(*ProtocolServerCommandProcessHandler)(PackRead *param);
typedef ProtocolServerResult *(*ProtocolServerCommandProcessSessionHandler)(PackRead *param, void *sessionData);
typedef ProtocolServerResult *(*ProtocolServerCommandCloseHandler)(PackRead *param, void *sessionData);

typedef struct ProtocolServerHandler
{
    StringId command;                                               // 5-bit StringId that identifies the protocol command
    ProtocolServerCommandOpenHandler open;                          // Function that opens the protocol session
    ProtocolServerCommandProcessHandler process;                    // Function that processes the protocol command
    ProtocolServerCommandProcessSessionHandler processSession;      // Function that processes the protocol command for a session
    ProtocolServerCommandCloseHandler close;                        // Function that closes the protocol session
} ProtocolServerHandler;

/***********************************************************************************************************************************
Server Constructors
***********************************************************************************************************************************/
FN_EXTERN ProtocolServer *protocolServerNew(const String *name, const String *service, IoRead *read, IoWrite *write);

/***********************************************************************************************************************************
Server Functions
***********************************************************************************************************************************/
// Get command from the client. Outside ProtocolServer, this is used when the first noop command needs to be processed before
// running protocolServerProcess(), which allows an error to be returned to the client if initialization fails.
typedef struct ProtocolServerRequestResult
{
    StringId id;                                                    // Command identifier
    ProtocolCommandType type;                                       // Command type
    bool sessionRequired;                                           // Session with more than one command
    Pack *param;                                                    // Parameter pack
} ProtocolServerRequestResult;

FN_EXTERN ProtocolServerRequestResult protocolServerRequest(ProtocolServer *this);

// Send response to client
typedef struct ProtocolServerResponseParam
{
    VAR_PARAM_HEADER;
    bool close;                                                     // Has the session been closed?
    ProtocolMessageType type;                                       // Message type
    PackWrite *data;                                                // Response data
} ProtocolServerResponseParam;

#define protocolServerResponseP(this, ...)                                                                                         \
    protocolServerResponse(this, (ProtocolServerResponseParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN void protocolServerResponse(ProtocolServer *const this, ProtocolServerResponseParam param);

// Send an error to client
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
Server Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
protocolServerFree(ProtocolServer *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Result Constructors
***********************************************************************************************************************************/
typedef struct ProtocolServerResultNewParam
{
    VAR_PARAM_HEADER;
    size_t extra;
} ProtocolServerResultNewParam;

#define protocolServerResultNewP(...)                                                                                              \
    protocolServerResultNew((ProtocolServerResultNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN ProtocolServerResult *protocolServerResultNew(ProtocolServerResultNewParam param);

/***********************************************************************************************************************************
Result Getters/Setters
***********************************************************************************************************************************/
// Create PackWrite object required to send data to the client
FN_EXTERN PackWrite *protocolServerResultData(ProtocolServerResult *this);

// Set session data
FN_EXTERN void protocolServerResultSessionDataSet(ProtocolServerResult *const this, void *const sessionData);

// Close session
FN_EXTERN void protocolServerResultCloseSet(ProtocolServerResult *const this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void protocolServerToLog(const ProtocolServer *this, StringStatic *debugLog);

#define FUNCTION_LOG_PROTOCOL_SERVER_TYPE                                                                                          \
    ProtocolServer *
#define FUNCTION_LOG_PROTOCOL_SERVER_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_OBJECT_FORMAT(value, protocolServerToLog, buffer, bufferSize)

FN_EXTERN void protocolServerResultToLog(const ProtocolServerResult *this, StringStatic *debugLog);

#define FUNCTION_LOG_PROTOCOL_SERVER_RESULT_TYPE                                                                                   \
    ProtocolServerResult *
#define FUNCTION_LOG_PROTOCOL_SERVER_RESULT_FORMAT(value, buffer, bufferSize)                                                      \
    FUNCTION_LOG_OBJECT_FORMAT(value, protocolServerResultToLog, buffer, bufferSize)

#endif
