/***********************************************************************************************************************************
Io Server Interface

Create sessions for protocol servers. For example, a TLS server can be created with tlsServerNew() and then new TLS sessions can be
accepted with ioServerAccept().
***********************************************************************************************************************************/
#ifndef COMMON_IO_SERVER_H
#define COMMON_IO_SERVER_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoServer IoServer;

#include "common/io/server.intern.h"
#include "common/io/session.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct IoServerPub
{
    MemContext *memContext;                                         // Mem context
    void *driver;                                                   // Driver object
    const IoServerInterface *interface;                             // Driver interface
} IoServerPub;

// Name that identifies the server
__attribute__((always_inline)) static inline const String *
ioServerName(const IoServer *const this)
{
    return THIS_PUB(IoServer)->interface->name(THIS_PUB(IoServer)->driver);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
__attribute__((always_inline)) static inline IoServer *
ioServerMove(IoServer *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Open session
__attribute__((always_inline)) static inline IoSession *
ioServerAccept(IoServer *const this, IoSession *const session)
{
    return THIS_PUB(IoServer)->interface->accept(THIS_PUB(IoServer)->driver, session);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
ioServerFree(IoServer *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *ioServerToLog(const IoServer *this);

#define FUNCTION_LOG_IO_SERVER_TYPE                                                                                                \
    IoServer *
#define FUNCTION_LOG_IO_SERVER_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, ioServerToLog, buffer, bufferSize)

#endif
