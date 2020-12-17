/***********************************************************************************************************************************
Protocol Server
***********************************************************************************************************************************/
#ifndef PROTOCOL_SERVER_H
#define PROTOCOL_SERVER_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define PROTOCOL_SERVER_TYPE                                        ProtocolServer
#define PROTOCOL_SERVER_PREFIX                                      protocolServer

typedef struct ProtocolServer ProtocolServer;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Protocol process handler type
***********************************************************************************************************************************/
typedef bool (*ProtocolServerProcessHandler)(const String *command, PackRead *param, ProtocolServer *server);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
ProtocolServer *protocolServerNew(const String *name, const String *service, IoRead *read, IoWrite *write);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Return an error
void protocolServerError(ProtocolServer *this, int code, const String *message, const String *stack);

// Process requests
void protocolServerProcess(ProtocolServer *this, const VariantList *retryInterval);

// Respond to request with output if provided
void protocolServerResponse(ProtocolServer *this, const Variant *output);

// Add a new handler
void protocolServerHandlerAdd(ProtocolServer *this, ProtocolServerProcessHandler handler);

// Move to a new parent mem context
ProtocolServer *protocolServerMove(ProtocolServer *this, MemContext *parentNew);

// Write a line
void protocolServerWriteLine(const ProtocolServer *this, const String *line);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Read interface
IoRead *protocolServerIoRead(const ProtocolServer *this);

// Write interface
IoWrite *protocolServerIoWrite(const ProtocolServer *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void protocolServerFree(ProtocolServer *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *protocolServerToLog(const ProtocolServer *this);

#define FUNCTION_LOG_PROTOCOL_SERVER_TYPE                                                                                          \
    ProtocolServer *
#define FUNCTION_LOG_PROTOCOL_SERVER_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, protocolServerToLog, buffer, bufferSize)

#endif
