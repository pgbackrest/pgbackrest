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
#define PROTOCOL_GREETING_NAME                                      "name"
    STRING_DECLARE(PROTOCOL_GREETING_NAME_STR);
#define PROTOCOL_GREETING_SERVICE                                   "service"
    STRING_DECLARE(PROTOCOL_GREETING_SERVICE_STR);
#define PROTOCOL_GREETING_VERSION                                   "version"
    STRING_DECLARE(PROTOCOL_GREETING_VERSION_STR);

#define PROTOCOL_COMMAND_CONFIG                                     STRID5("config", 0xe9339e30)
#define PROTOCOL_COMMAND_EXIT                                       STRID5("exit", 0xa27050)
#define PROTOCOL_COMMAND_NOOP                                       STRID5("noop", 0x83dee0)
#define PROTOCOL_COMMAND_TLS                                        STRID5("tls", 0x4d940)

/***********************************************************************************************************************************
This size should be safe for most pack data without wasting a lot of space. If binary data is being transferred then this size can
be added to the expected binary size to account for overhead.
***********************************************************************************************************************************/
#define PROTOCOL_PACK_DEFAULT_SIZE                                  1024

// Pack large enough for standard data. Note that the buffer will automatically resize when required.
__attribute__((always_inline)) static inline PackWrite *
protocolPackNew(void)
{
    return pckWriteNewBuf(bufNew(PROTOCOL_PACK_DEFAULT_SIZE));
}

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
} ProtocolClientPub;

// Read file descriptor
__attribute__((always_inline)) static inline int
protocolClientIoReadFd(ProtocolClient *const this)
{
    return ioReadFd(THIS_PUB(ProtocolClient)->read);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Execute a command and get the result
PackRead *protocolClientExecute(ProtocolClient *this, ProtocolCommand *command, bool resultRequired);

// Move to a new parent mem context
__attribute__((always_inline)) static inline ProtocolClient *
protocolClientMove(ProtocolClient *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Send noop to test connection or keep it alive
void protocolClientNoOp(ProtocolClient *this);

// Get data put by the server
PackRead *protocolClientDataGet(ProtocolClient *this);
void protocolClientDataEndGet(ProtocolClient *this);

// Put command to the server
void protocolClientCommandPut(ProtocolClient *this, ProtocolCommand *command);

// Put data to the server
void protocolClientDataPut(ProtocolClient *this, PackWrite *data);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
protocolClientFree(ProtocolClient *const this)
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
