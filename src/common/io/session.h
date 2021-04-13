/***********************************************************************************************************************************
Io Session Interface

Provides access to IoRead and IoWrite interfaces for interacting with the session returned by ioClientOpen(). Sessions should always
be closed when work with them is done but they also contain destructors to do cleanup if there is an error.
***********************************************************************************************************************************/
#ifndef COMMON_IO_SESSION_H
#define COMMON_IO_SESSION_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoSession IoSession;

/***********************************************************************************************************************************
Session roles
***********************************************************************************************************************************/
typedef enum
{
    ioSessionRoleClient,                                            // Client session
    ioSessionRoleServer,                                            // Server session
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
    ASSERT_INLINE(this != NULL);
    return ((IoSessionPub *)this)->interface->ioRead(((IoSessionPub *)this)->driver);
}

// Write interface
__attribute__((always_inline)) static inline IoWrite *
ioSessionIoWrite(IoSession *const this)
{
    ASSERT_INLINE(this != NULL);
    return ((IoSessionPub *)this)->interface->ioWrite(((IoSessionPub *)this)->driver);
}

// Session role
__attribute__((always_inline)) static inline IoSessionRole
ioSessionRole(const IoSession *const this)
{
    ASSERT_INLINE(this != NULL);
    return ((const IoSessionPub *)this)->interface->role(((const IoSessionPub *)this)->driver);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Close the session
__attribute__((always_inline)) static inline void
ioSessionClose(IoSession *const this)
{
    ASSERT_INLINE(this != NULL);
    return ((IoSessionPub *)this)->interface->close(((IoSessionPub *)this)->driver);
}

// Move to a new parent mem context
__attribute__((always_inline)) static inline IoSession *
ioSessionMove(IoSession *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
ioSessionFree(IoSession *this)
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
