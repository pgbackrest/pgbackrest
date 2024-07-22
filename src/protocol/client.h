/***********************************************************************************************************************************
Protocol Client

There are three ways to make a request using the client:

1. Synchronous

This is the most common way to make a request. Simply call protocolClientRequestP() and process the result.

2. Synchronous with Session

In some cases it is useful to keep session state between requests. An example of this is queries against a database where it is more
efficient to maintain the connection to the database between queries. The session is created with protocolClientSessionNewP(),
opened with protocolClientSessionOpenP(), and closed with protocolClientSessionClose(). Requests are made with
protocolClientSessionRequest().

3. Asynchronous with Session

Asynchronous requests provide better performance by allowing the client and server to process at the same time. An example of this
is reading a file where a asynchronous request for the next block of data can be sent before the current block is being processed.
The next block should be waiting when processing of the current block is complete. The same session functions are used for creation,
open, and close, except that .async = true is passed to protocolClientSessionNewP(). Asynchronous requests are made with
protocolClientSessionRequestAsync() and the responses are read with protocolClientSessionResponse(). Note that there can only be one
outstanding asynchronous request, therefore protocolClientSessionResponse() must be called before
protocolClientSessionRequestAsync() can be called again.
***********************************************************************************************************************************/
#ifndef PROTOCOL_CLIENT_H
#define PROTOCOL_CLIENT_H

/***********************************************************************************************************************************
Message types used by the protocol
***********************************************************************************************************************************/
typedef enum
{
    // Data passed from server to client
    protocolMessageTypeResponse = 0,

    // Request sent from the client to the server
    protocolMessageTypeRequest = 1,

    // An error occurred on the server and the request ended abnormally. protocolMessageTypeResponse will not be sent to the client.
    protocolMessageTypeError = 2,
} ProtocolMessageType;

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
Object type
***********************************************************************************************************************************/
typedef struct ProtocolClient ProtocolClient;
typedef struct ProtocolClientSession ProtocolClientSession;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/object.h"

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
Client Constructors
***********************************************************************************************************************************/
FN_EXTERN ProtocolClient *protocolClientNew(const String *name, const String *service, IoRead *read, IoWrite *write);

/***********************************************************************************************************************************
Client Getters/Setters
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
Client Functions
***********************************************************************************************************************************/
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

// Simple request that does not require a session or async
typedef struct ProtocolClientRequestParam
{
    VAR_PARAM_HEADER;
    PackWrite *param;
} ProtocolClientRequestParam;

#define protocolClientRequestP(this, command, ...)                                                                                          \
    protocolClientRequest(this, command, (ProtocolClientRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN PackRead *protocolClientRequest(ProtocolClient *this, StringId command, ProtocolClientRequestParam param);

/***********************************************************************************************************************************
Client Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
protocolClientFree(ProtocolClient *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Session Constructors
***********************************************************************************************************************************/
// New session
typedef struct ProtocolClientSessionNewParam
{
    VAR_PARAM_HEADER;
    bool async;                                                     // Async requests allowed?
} ProtocolClientSessionNewParam;

#define protocolClientSessionNewP(client, command, ...)                                                                            \
    protocolClientSessionNew(client, command, (ProtocolClientSessionNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN ProtocolClientSession *protocolClientSessionNew(
    ProtocolClient *client, StringId command, ProtocolClientSessionNewParam param);

/***********************************************************************************************************************************
Session Getters/Setters
***********************************************************************************************************************************/
typedef struct ProtocolClientSessionPub
{
    bool open;                                                      // Is the session open?
    bool queued;                                                    // Is a response currently queued?
} ProtocolClientSessionPub;

// Is a response currently queued?
FN_INLINE_ALWAYS bool
protocolClientSessionQueued(const ProtocolClientSession *const this)
{
    return THIS_PUB(ProtocolClientSession)->queued;
}

// Is the session closed?
FN_INLINE_ALWAYS bool
protocolClientSessionClosed(const ProtocolClientSession *const this)
{
    return !THIS_PUB(ProtocolClientSession)->open;
}

/***********************************************************************************************************************************
Session Functions
***********************************************************************************************************************************/
// Session open
typedef struct ProtocolClientSessionOpenParam
{
    VAR_PARAM_HEADER;
    PackWrite *param;
} ProtocolClientSessionOpenParam;

#define protocolClientSessionOpenP(this, ...)                                                                                      \
    protocolClientSessionOpen(this, (ProtocolClientSessionOpenParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN PackRead *protocolClientSessionOpen(ProtocolClientSession *const this, ProtocolClientSessionOpenParam param);

// Session request
#define protocolClientSessionRequestP(this, ...)                                                                                   \
    protocolClientSessionRequest(this, (ProtocolClientRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN PackRead *protocolClientSessionRequest(ProtocolClientSession *const this, ProtocolClientRequestParam param);

// Session async request
#define protocolClientSessionRequestAsyncP(this, ...)                                                                              \
    protocolClientSessionRequestAsync(this, (ProtocolClientRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN void protocolClientSessionRequestAsync(ProtocolClientSession *const this, ProtocolClientRequestParam param);

// Session response after a call to protocolClientSessionRequestAsyncP()
FN_EXTERN PackRead *protocolClientSessionResponse(ProtocolClientSession *const this);

// Session close
FN_EXTERN PackRead *protocolClientSessionClose(ProtocolClientSession *const this);

// Session cancel
FN_EXTERN void protocolClientSessionCancel(ProtocolClientSession *const this);

/***********************************************************************************************************************************
Session Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
protocolClientSessionFree(ProtocolClientSession *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void protocolClientToLog(const ProtocolClient *this, StringStatic *debugLog);

#define FUNCTION_LOG_PROTOCOL_CLIENT_TYPE                                                                                          \
    ProtocolClient *
#define FUNCTION_LOG_PROTOCOL_CLIENT_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_OBJECT_FORMAT(value, protocolClientToLog, buffer, bufferSize)

FN_EXTERN void protocolClientSessionToLog(const ProtocolClientSession *this, StringStatic *debugLog);

#define FUNCTION_LOG_PROTOCOL_CLIENT_SESSION_TYPE                                                                                  \
    ProtocolClientSession *
#define FUNCTION_LOG_PROTOCOL_CLIENT_SESSION_FORMAT(value, buffer, bufferSize)                                                     \
    FUNCTION_LOG_OBJECT_FORMAT(value, protocolClientSessionToLog, buffer, bufferSize)

#endif
