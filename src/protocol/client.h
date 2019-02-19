/***********************************************************************************************************************************
Protocol Client
***********************************************************************************************************************************/
#ifndef PROTOCOL_CLIENT_H
#define PROTOCOL_CLIENT_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct ProtocolClient ProtocolClient;

/***********************************************************************************************************************************
Remote tyoe enum
***********************************************************************************************************************************/
typedef enum
{
    remoteTypeRepo,
    remoteTypeDb,
} RemoteType;

#include "common/io/read.h"
#include "common/io/write.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_GREETING_NAME                                      "name"
    STRING_DECLARE(PROTOCOL_GREETING_NAME_STR);
#define PROTOCOL_GREETING_SERVICE                                   "service"
    STRING_DECLARE(PROTOCOL_GREETING_SERVICE_STR);
#define PROTOCOL_GREETING_VERSION                                   "version"
    STRING_DECLARE(PROTOCOL_GREETING_VERSION_STR);

#define PROTOCOL_COMMAND                                            "cmd"
    STRING_DECLARE(PROTOCOL_COMMAND_STR);
#define PROTOCOL_COMMAND_EXIT                                       "exit"
    STRING_DECLARE(PROTOCOL_COMMAND_EXIT_STR);
#define PROTOCOL_COMMAND_NOOP                                       "noop"
    STRING_DECLARE(PROTOCOL_COMMAND_NOOP_STR);

#define PROTOCOL_ERROR                                              "err"
    STRING_DECLARE(PROTOCOL_ERROR_STR);

#define PROTOCOL_OUTPUT                                             "out"
    STRING_DECLARE(PROTOCOL_OUTPUT_STR);

#define PROTOCOL_PARAMETER                                          "param"
    STRING_DECLARE(PROTOCOL_PARAMETER_STR);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
ProtocolClient *protocolClientNew(const String *name, const String *service, IoRead *read, IoWrite *write);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
const Variant *protocolClientExecute(ProtocolClient *this, const KeyValue *command, bool outputRequired);
ProtocolClient *protocolClientMove(ProtocolClient *this, MemContext *parentNew);
void protocolClientNoOp(ProtocolClient *this);
const Variant *protocolClientReadOutput(ProtocolClient *this, bool outputRequired);
void protocolClientWriteCommand(ProtocolClient *this, const KeyValue *command);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
IoRead *protocolClientIoRead(const ProtocolClient *this);
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
