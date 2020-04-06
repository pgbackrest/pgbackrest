/***********************************************************************************************************************************
Protocol Command
***********************************************************************************************************************************/
#ifndef PROTOCOL_COMMAND_H
#define PROTOCOL_COMMAND_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_TYPE                                       ProtocolCommand
#define PROTOCOL_COMMAND_PREFIX                                     protocolCommand

typedef struct ProtocolCommand ProtocolCommand;

#include "common/type/variant.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_KEY_COMMAND                                        "cmd"
    STRING_DECLARE(PROTOCOL_KEY_COMMAND_STR);
#define PROTOCOL_KEY_PARAMETER                                      "param"
    STRING_DECLARE(PROTOCOL_KEY_PARAMETER_STR);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
ProtocolCommand *protocolCommandNew(const String *command);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
ProtocolCommand *protocolCommandMove(ProtocolCommand *this, MemContext *parentNew);

// Read the command output
ProtocolCommand *protocolCommandParamAdd(ProtocolCommand *this, const Variant *param);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Command JSON
String *protocolCommandJson(const ProtocolCommand *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void protocolCommandFree(ProtocolCommand *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *protocolCommandToLog(const ProtocolCommand *this);

#define FUNCTION_LOG_PROTOCOL_COMMAND_TYPE                                                                                         \
    ProtocolCommand *
#define FUNCTION_LOG_PROTOCOL_COMMAND_FORMAT(value, buffer, bufferSize)                                                            \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, protocolCommandToLog, buffer, bufferSize)

#endif
