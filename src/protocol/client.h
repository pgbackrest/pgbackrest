/***********************************************************************************************************************************
Protocol Client
***********************************************************************************************************************************/
#ifndef PROTOCOL_CLIENT_H
#define PROTOCOL_CLIENT_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define PROTOCOL_CLIENT_TYPE                                        ProtocolClient
#define PROTOCOL_CLIENT_PREFIX                                      protocolClient

typedef struct ProtocolClient ProtocolClient;

#include "common/io/read.h"
#include "common/io/write.h"
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

#define PROTOCOL_COMMAND_EXIT                                       STRID4('e', 'x', 'i', 't')
#define PROTOCOL_COMMAND_NOOP                                       STRID4('n', 'o', 'o', 'p')

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
Functions
***********************************************************************************************************************************/
// Execute a protocol command and get the output
const Variant *protocolClientExecute(ProtocolClient *this, const ProtocolCommand *command, bool outputRequired);
ProtocolClient *protocolClientMove(ProtocolClient *this, MemContext *parentNew);

// Send noop to test connection or keep it alive
void protocolClientNoOp(ProtocolClient *this);

// Read a line
String *protocolClientReadLine(ProtocolClient *this);

// Read the command output
const Variant *protocolClientReadOutput(ProtocolClient *this, bool outputRequired);

// Write the protocol command
void protocolClientWriteCommand(ProtocolClient *this, const ProtocolCommand *command);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Read interface
IoRead *protocolClientIoRead(const ProtocolClient *this);

// Write interface
IoWrite *protocolClientIoWrite(const ProtocolClient *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void protocolClientFree(ProtocolClient *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *protocolClientToLog(const ProtocolClient *this);

#define FUNCTION_LOG_PROTOCOL_CLIENT_TYPE                                                                                          \
    ProtocolClient *
#define FUNCTION_LOG_PROTOCOL_CLIENT_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, protocolClientToLog, buffer, bufferSize)

#endif
