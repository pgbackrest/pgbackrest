/***********************************************************************************************************************************
Io Session Interface

Provides access to IoRead and IoWrite interfaces for interacting with the session returned by ioClientOpen(). Sessions should always
be closed when work with them is done but they also contain destructors to do cleanup if there is an error.
***********************************************************************************************************************************/
#ifndef COMMON_IO_SESSION_H
#define COMMON_IO_SESSION_H

#include "common/type/stringId.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoSession IoSession;

/***********************************************************************************************************************************
Session roles
***********************************************************************************************************************************/
typedef enum
{
    ioSessionRoleClient = STRID5("client", 0x28e2a5830),            // Client session
    ioSessionRoleServer = STRID5("server", 0x245b48b30),            // Server session
} IoSessionRole;

#include "common/io/read.h"
#include "common/io/session.intern.h"
#include "common/io/write.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct IoSessionPub
{
    void *driver;                                                   // Driver object
    const IoSessionInterface *interface;                            // Driver interface
    const String *peerName;                                         // Name of peer (exact meaning depends on driver)
    bool authenticated;                                             // Is the session authenticated?
} IoSessionPub;

// Is the session authenticated? The exact meaning of "authenticated" will vary by driver type.
FN_INLINE_ALWAYS bool
ioSessionAuthenticated(const IoSession *const this)
{
    return THIS_PUB(IoSession)->authenticated;
}

// Session file descriptor, -1 if none
FN_EXTERN int ioSessionFd(IoSession *this);

// Read interface
typedef struct IoSessionIoReadParam
{
    VAR_PARAM_HEADER;
    bool ignoreUnexpectedEof;                                       // Allow unexpected EOF, e.g. TLS session improperly terminated
} IoSessionIoReadParam;

#define ioSessionIoReadP(this, ...)                                                                                                \
    ioSessionIoRead(this, (IoSessionIoReadParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_INLINE_ALWAYS IoRead *
ioSessionIoRead(IoSession *const this, const IoSessionIoReadParam param)
{
    return THIS_PUB(IoSession)->interface->ioRead(THIS_PUB(IoSession)->driver, param.ignoreUnexpectedEof);
}

// Write interface
FN_INLINE_ALWAYS IoWrite *
ioSessionIoWrite(IoSession *const this)
{
    return THIS_PUB(IoSession)->interface->ioWrite(THIS_PUB(IoSession)->driver);
}

// The peer name, if any. The exact meaning will vary by driver type, for example the peer name will be the client CN for the
// TlsSession driver.
FN_INLINE_ALWAYS const String *
ioSessionPeerName(const IoSession *const this)
{
    return THIS_PUB(IoSession)->peerName;
}

// Session role
FN_INLINE_ALWAYS IoSessionRole
ioSessionRole(const IoSession *const this)
{
    return THIS_PUB(IoSession)->interface->role(THIS_PUB(IoSession)->driver);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Close the session
FN_INLINE_ALWAYS void
ioSessionClose(IoSession *const this)
{
    return THIS_PUB(IoSession)->interface->close(THIS_PUB(IoSession)->driver);
}

// Move to a new parent mem context
FN_INLINE_ALWAYS IoSession *
ioSessionMove(IoSession *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
ioSessionFree(IoSession *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void ioSessionToLog(const IoSession *this, StringStatic *debugLog);

#define FUNCTION_LOG_IO_SESSION_TYPE                                                                                               \
    IoSession *
#define FUNCTION_LOG_IO_SESSION_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_OBJECT_FORMAT(value, ioSessionToLog, buffer, bufferSize)

#endif
