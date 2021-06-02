/***********************************************************************************************************************************
Protocol Server
***********************************************************************************************************************************/
#ifndef PROTOCOL_SERVER_H
#define PROTOCOL_SERVER_H

/***********************************************************************************************************************************
Commands may have multiple processes that work together to implement their functionality.  These roles allow each process to know
what it is supposed to do.
***********************************************************************************************************************************/
typedef enum
{
    // One of possibly many results to be returned to the client
    protocolServerTypeResult = 0,

    // Final response that indicates the end of command processing and may also have a result
    protocolServerTypeResponse = 1,
} ProtocolServerType;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct ProtocolServer ProtocolServer;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/object.h"
#include "common/type/pack.h"
#include "common/type/stringId.h"

/***********************************************************************************************************************************
This size should be safe for most results without wasting a lot of space. If binary data is being transferred then this size can be
added to the expected binary size to account for overhead.
***********************************************************************************************************************************/
#define PROTOCOL_SERVER_RESULT_SIZE                                 1024

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

// Read interface
__attribute__((always_inline)) static inline IoRead *
protocolServerIoRead(ProtocolServer *const this)
{
    return THIS_PUB(ProtocolServer)->read;
}

// Write interface
__attribute__((always_inline)) static inline IoWrite *
protocolServerIoWrite(ProtocolServer *const this)
{
    return THIS_PUB(ProtocolServer)->write;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Return an error
void protocolServerError(ProtocolServer *this, int code, const String *message, const String *stack);

// Process requests
void protocolServerProcess(
    ProtocolServer *this, const VariantList *retryInterval, const ProtocolServerHandler *const handlerList,
    const unsigned int handlerListSize);

// Result pack large enough for standard results. Note that the buffer will automatically resize when required.
__attribute__((always_inline)) static inline PackWrite *
protocolServerResultPack(void)
{
    return pckWriteNewBuf(bufNew(PROTOCOL_SERVER_RESULT_SIZE));
}

// !!! Respond to request with output if provided
void protocolServerResult(ProtocolServer *this, PackWrite *resultPack);

__attribute__((always_inline)) static inline void
protocolServerResultBool(ProtocolServer *this, bool result)
{
    PackWrite *resultPack = protocolServerResultPack();
    pckWriteBoolP(resultPack, result);
    protocolServerResult(this, resultPack);
}

void protocolServerResponse(ProtocolServer *this);
void protocolServerResponseVar(ProtocolServer *this, const Variant *output);

// Move to a new parent mem context
__attribute__((always_inline)) static inline ProtocolServer *
protocolServerMove(ProtocolServer *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Write a line
void protocolServerWriteLine(ProtocolServer *this, const String *line);

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
