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
#include "common/type/stringId.h"

/***********************************************************************************************************************************
Protocol command handler type and structure

An array of this struct must be passed to protocolServerProcess() for the server to process commands. Each command handler should
implement a single command, as defined by the command string.
***********************************************************************************************************************************/
typedef void (*ProtocolServerCommandHandler)(const VariantList *paramList, ProtocolServer *server);

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
protocolServerIoRead(ProtocolServer *this)
{
    ASSERT_INLINE(this != NULL);
    return ((ProtocolServerPub *)this)->read;
}

// Write interface
__attribute__((always_inline)) static inline IoWrite *
protocolServerIoWrite(ProtocolServer *this)
{
    ASSERT_INLINE(this != NULL);
    return ((ProtocolServerPub *)this)->write;
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

// Respond to request with output if provided
void protocolServerResponse(ProtocolServer *this, const Variant *output);

// Move to a new parent mem context
__attribute__((always_inline)) static inline ProtocolServer *
protocolServerMove(ProtocolServer *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

// Write a line
void protocolServerWriteLine(ProtocolServer *this, const String *line);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
protocolServerFree(ProtocolServer *this)
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
