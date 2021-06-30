/***********************************************************************************************************************************
Protocol Command
***********************************************************************************************************************************/
#ifndef PROTOCOL_COMMAND_H
#define PROTOCOL_COMMAND_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct ProtocolCommand ProtocolCommand;

#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
ProtocolCommand *protocolCommandNew(const StringId command);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
// Move to a new parent mem context
__attribute__((always_inline)) static inline ProtocolCommand *
protocolCommandMove(ProtocolCommand *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Read the command output
PackWrite *protocolCommandParam(ProtocolCommand *this);

// Write protocol command
void protocolCommandPut(ProtocolCommand *this, IoWrite *write);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
protocolCommandFree(ProtocolCommand *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *protocolCommandToLog(const ProtocolCommand *this);

#define FUNCTION_LOG_PROTOCOL_COMMAND_TYPE                                                                                         \
    ProtocolCommand *
#define FUNCTION_LOG_PROTOCOL_COMMAND_FORMAT(value, buffer, bufferSize)                                                            \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, protocolCommandToLog, buffer, bufferSize)

#endif
