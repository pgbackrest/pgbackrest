/***********************************************************************************************************************************
Protocol Client
***********************************************************************************************************************************/
#ifndef PROTOCOL_CLIENT_H
#define PROTOCOL_CLIENT_H

/***********************************************************************************************************************************
Message types used by the protocol
***********************************************************************************************************************************/
typedef enum
{
    // Data passed between client and server in either direction. This can be used as many times as needed.
    protocolMessageTypeData = 0,

    // Indicates no more data for the server to return to the client and ends the command
    protocolMessageTypeDataEnd = 1,

    // Command sent from the client to the server
    protocolMessageTypeCommand = 2,

    // An error occurred on the server and the command ended abnormally. protocolMessageTypeDataEnd will not be sent to the client.
    protocolMessageTypeError = 3,
} ProtocolMessageType;

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
#define PROTOCOL_GREETING_NAME                                      STRID5("name", 0x2b42e0)
#define PROTOCOL_GREETING_SERVICE                                   STRID5("service", 0x1469b48b30)
#define PROTOCOL_GREETING_VERSION                                   STRID5("version", 0x39e99c8b60)

#define PROTOCOL_COMMAND_CONFIG                                     STRID5("config", 0xe9339e30)
#define PROTOCOL_COMMAND_EXIT                                       STRID5("exit", 0xa27050)
#define PROTOCOL_COMMAND_NOOP                                       STRID5("noop", 0x83dee0)

/***********************************************************************************************************************************
This size should be safe for most pack data without wasting a lot of space. If binary data is being transferred then this size can
be added to the expected binary size to account for overhead.
***********************************************************************************************************************************/
#define PROTOCOL_PACK_DEFAULT_SIZE                                  1024

// Pack large enough for standard data. Note that the buffer will automatically resize when required.
FN_INLINE_ALWAYS PackWrite *
protocolPackNew(void)
{
    return pckWriteNewP(.size = PROTOCOL_PACK_DEFAULT_SIZE);
}

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN ProtocolClient *protocolClientNew(const String *name, const String *service, IoRead *read, IoWrite *write);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct ProtocolClientPub
{
    IoRead *read;                                                   // Read interface
} ProtocolClientPub;

// Read file descriptor
FN_INLINE_ALWAYS int
protocolClientIoReadFd(ProtocolClient *const this)
{
    return ioReadFd(THIS_PUB(ProtocolClient)->read);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Execute a command and get the result
FN_EXTERN PackRead *protocolClientExecute(ProtocolClient *this, ProtocolCommand *command, bool resultRequired);

// Move to a new parent mem context
FN_INLINE_ALWAYS ProtocolClient *
protocolClientMove(ProtocolClient *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Do not send exit command to the server when the client is freed
FN_INLINE_ALWAYS void
protocolClientNoExit(ProtocolClient *const this)
{
    memContextCallbackClear(objMemContext(this));
}

// Send noop to test connection or keep it alive
FN_EXTERN void protocolClientNoOp(ProtocolClient *this);

// Get data put by the server
FN_EXTERN PackRead *protocolClientDataGet(ProtocolClient *this);
FN_EXTERN void protocolClientDataEndGet(ProtocolClient *this);

// Put command to the server
FN_EXTERN void protocolClientCommandPut(ProtocolClient *this, ProtocolCommand *command, const bool dataPut);

// Put data to the server
FN_EXTERN void protocolClientDataPut(ProtocolClient *this, PackWrite *data);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
protocolClientFree(ProtocolClient *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN String *protocolClientToLog(const ProtocolClient *this);

#define FUNCTION_LOG_PROTOCOL_CLIENT_TYPE                                                                                          \
    ProtocolClient *
#define FUNCTION_LOG_PROTOCOL_CLIENT_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, protocolClientToLog, buffer, bufferSize)

#endif
