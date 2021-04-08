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

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Session roles
***********************************************************************************************************************************/
typedef enum
{
    ioSessionRoleClient,                                            // Client session
    ioSessionRoleServer,                                            // Server session
} IoSessionRole;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Close the session
void ioSessionClose(IoSession *this);

// Move to a new parent mem context
__attribute__((always_inline)) static inline IoSession *
ioSessionMove(IoSession *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Session file descriptor, -1 if none
int ioSessionFd(IoSession *this);

// Read interface
IoRead *ioSessionIoRead(IoSession *this);

// Write interface
IoWrite *ioSessionIoWrite(IoSession *this);

// Session role
IoSessionRole ioSessionRole(const IoSession *this);

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
