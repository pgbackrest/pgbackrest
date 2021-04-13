/***********************************************************************************************************************************
Protocol Client
***********************************************************************************************************************************/
#ifndef PROTOCOL_CLIENT_H
#define PROTOCOL_CLIENT_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct ProtocolClient ProtocolClient;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/object.h"
#include "protocol/command.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_GREETING_NAME                                      "name"
    STRING_DECLARE(PROTOCOL_GREETING_NAME_STR);
#define PROTOCOL_GREETING_SERVICE                                   "service"
    STRING_DECLARE(PROTOCOL_GREETING_SERVICE_STR);
#define PROTOCOL_GREETING_VERSION                                   "version"
    STRING_DECLARE(PROTOCOL_GREETING_VERSION_STR);

#define PROTOCOL_COMMAND_EXIT                                       "exit"
    STRING_DECLARE(PROTOCOL_COMMAND_EXIT_STR);
#define PROTOCOL_COMMAND_NOOP                                       "noop"
    STRING_DECLARE(PROTOCOL_COMMAND_NOOP_STR);

#define PROTOCOL_ERROR                                              "err"
    STRING_DECLARE(PROTOCOL_ERROR_STR);
#define PROTOCOL_ERROR_STACK                                        "errStack"
    STRING_DECLARE(PROTOCOL_ERROR_STACK_STR);

#define PROTOCOL_OUTPUT                                             "out"
    STRING_DECLARE(PROTOCOL_OUTPUT_STR);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
ProtocolClient *protocolClientNew(const String *name, const String *service, IoRead *read, IoWrite *write);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct ProtocolClientPub
{
    MemContext *memContext;                                         // Mem context
    IoRead *read;                                                   // Read interface
    IoWrite *write;                                                 // Write interface
} ProtocolClientPub;

// Read interface
__attribute__((always_inline)) static inline IoRead *
protocolClientIoRead(ProtocolClient *this)
{
    return THIS_PUB(ProtocolClient)->read;
}

// Write interface
__attribute__((always_inline)) static inline IoWrite *
protocolClientIoWrite(ProtocolClient *this)
{
    return THIS_PUB(ProtocolClient)->write;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Execute a protocol command and get the output
const Variant *protocolClientExecute(ProtocolClient *this, const ProtocolCommand *command, bool outputRequired);

// Move to a new parent mem context
__attribute__((always_inline)) static inline ProtocolClient *
protocolClientMove(ProtocolClient *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

// Send noop to test connection or keep it alive
void protocolClientNoOp(ProtocolClient *this);

// Read a line
String *protocolClientReadLine(ProtocolClient *this);

// Read the command output
const Variant *protocolClientReadOutput(ProtocolClient *this, bool outputRequired);

// Write the protocol command
void protocolClientWriteCommand(ProtocolClient *this, const ProtocolCommand *command);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
protocolClientFree(ProtocolClient *this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *protocolClientToLog(const ProtocolClient *this);

#define FUNCTION_LOG_PROTOCOL_CLIENT_TYPE                                                                                          \
    ProtocolClient *
#define FUNCTION_LOG_PROTOCOL_CLIENT_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, protocolClientToLog, buffer, bufferSize)

#endif
