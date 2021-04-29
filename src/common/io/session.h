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
    MemContext *memContext;                                         // Mem context
    void *driver;                                                   // Driver object
    const IoSessionInterface *interface;                            // Driver interface
} IoSessionPub;

// Session file descriptor, -1 if none
int ioSessionFd(IoSession *this);

// Read interface
__attribute__((always_inline)) static inline IoRead *
ioSessionIoRead(IoSession *const this)
{
    return THIS_PUB(IoSession)->interface->ioRead(THIS_PUB(IoSession)->driver);
}

// Write interface
__attribute__((always_inline)) static inline IoWrite *
ioSessionIoWrite(IoSession *const this)
{
    return THIS_PUB(IoSession)->interface->ioWrite(THIS_PUB(IoSession)->driver);
}

// Session role
__attribute__((always_inline)) static inline IoSessionRole
ioSessionRole(const IoSession *const this)
{
    return THIS_PUB(IoSession)->interface->role(THIS_PUB(IoSession)->driver);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Close the session
__attribute__((always_inline)) static inline void
ioSessionClose(IoSession *const this)
{
    return THIS_PUB(IoSession)->interface->close(THIS_PUB(IoSession)->driver);
}

// Move to a new parent mem context
__attribute__((always_inline)) static inline IoSession *
ioSessionMove(IoSession *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
ioSessionFree(IoSession *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *ioSessionToLog(const IoSession *this);

#define FUNCTION_LOG_IO_SESSION_TYPE                                                                                               \
    IoSession *
#define FUNCTION_LOG_IO_SESSION_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, ioSessionToLog, buffer, bufferSize)

#endif
