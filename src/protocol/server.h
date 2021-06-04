/***********************************************************************************************************************************
Protocol Server
***********************************************************************************************************************************/
#ifndef PROTOCOL_SERVER_H
#define PROTOCOL_SERVER_H

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
typedef enum
{
    // One of possibly many results to be returned to the client
    protocolServerTypeResult = 0,

    // Data sent to the server in addition to command params
    protocolServerTypeData = 1,

    // Final response that indicates the end of command processing
    protocolServerTypeResponse = 2,

    // An error occurred and the command process was terminated
    protocolServerTypeError = 3,
} ProtocolServerType;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct ProtocolServer ProtocolServer;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/json.h"
#include "common/type/object.h"
#include "common/type/pack.h"
#include "common/type/stringId.h"
#include "protocol/client.h"

/***********************************************************************************************************************************
Protocol command handler type and structure

An array of this struct must be passed to protocolServerProcess() for the server to process commands. Each command handler should
implement a single command, as defined by the command string.
***********************************************************************************************************************************/
typedef void (*ProtocolServerCommandHandler)(PackRead *const param, ProtocolServer *const server);

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
Getters/Setters
***********************************************************************************************************************************/
typedef struct ProtocolServerPub
{
    MemContext *memContext;                                         // Mem context
    IoRead *read;                                                   // Read interface
    IoWrite *write;                                                 // Write interface
} ProtocolServerPub;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Return an error
void protocolServerError(ProtocolServer *this, int code, const String *message, const String *stack);

// Get a command. This is used when the first noop command needs to be processed before running protocolServerProcess(), which
// allows an error to be returned to the client if initialization fails.
typedef struct ProtocolServerCommandGetResult
{
    StringId id;                                                    // Command identifier
    Buffer *param;                                                  // Parameter pack
} ProtocolServerCommandGetResult;

ProtocolServerCommandGetResult protocolServerCommandGet(ProtocolServer *const this);

// Process requests
void protocolServerProcess(
    ProtocolServer *this, const VariantList *retryInterval, const ProtocolServerHandler *const handlerList,
    const unsigned int handlerListSize);

// !!! Respond to request with output if provided
void protocolServerResult(ProtocolServer *this, PackWrite *resultPack);

// !!!
PackRead *protocolServerDataGet(ProtocolServer *const this);

void protocolServerResponse(ProtocolServer *this);

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
