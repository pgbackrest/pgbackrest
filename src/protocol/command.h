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
Command types
***********************************************************************************************************************************/
typedef enum
{
    protocolCommandTypeOpen = STRID5("opn", 0x3a0f0),               // Open command for processing
    protocolCommandTypeProcess = STRID5("prc", 0xe500),             // Process command
    protocolCommandTypeClose = STRID5("cls", 0x4d830),              // Close command
    protocolCommandTypeCancel = STRID5("cnc", 0xdc30),              // Cancel command
} ProtocolCommandType;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct ProtocolCommandNewParam
{
    VAR_PARAM_HEADER;
    ProtocolCommandType type;                                       // Command type (defaults to protocolCommandTypeProcess)
    uint64_t sessionId;                                             // Session id
} ProtocolCommandNewParam;

#define protocolCommandNewP(command, ...)                                                                                          \
    protocolCommandNew(command, (ProtocolCommandNewParam){__VA_ARGS__})

FN_EXTERN ProtocolCommand *protocolCommandNew(const StringId command, ProtocolCommandNewParam param);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
FN_INLINE_ALWAYS ProtocolCommand *
protocolCommandMove(ProtocolCommand *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Read the command output
FN_EXTERN PackWrite *protocolCommandParam(ProtocolCommand *this);

// Write protocol command
FN_EXTERN void protocolCommandPut(ProtocolCommand *this, IoWrite *write);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
protocolCommandFree(ProtocolCommand *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void protocolCommandToLog(const ProtocolCommand *this, StringStatic *debugLog);

#define FUNCTION_LOG_PROTOCOL_COMMAND_TYPE                                                                                         \
    ProtocolCommand *
#define FUNCTION_LOG_PROTOCOL_COMMAND_FORMAT(value, buffer, bufferSize)                                                            \
    FUNCTION_LOG_OBJECT_FORMAT(value, protocolCommandToLog, buffer, bufferSize)

#endif
