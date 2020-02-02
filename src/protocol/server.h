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

/***********************************************************************************************************************************
Protocol process handler type
***********************************************************************************************************************************/
typedef bool (*ProtocolServerProcessHandler)(const String *command, const VariantList *paramList, ProtocolServer *server);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
ProtocolServer *protocolServerNew(const String *name, const String *service, IoRead *read, IoWrite *write);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void protocolServerError(ProtocolServer *this, int code, const String *message, const String *stack);
void protocolServerProcess(ProtocolServer *this);
void protocolServerResponse(ProtocolServer *this, const Variant *output);
void protocolServerHandlerAdd(ProtocolServer *this, ProtocolServerProcessHandler handler);
ProtocolServer *protocolServerMove(ProtocolServer *this, MemContext *parentNew);
void protocolServerWriteLine(const ProtocolServer *this, const String *line);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
IoRead *protocolServerIoRead(const ProtocolServer *this);
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
