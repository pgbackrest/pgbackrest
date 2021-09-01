/***********************************************************************************************************************************
Io Client Interface

Create sessions for protocol clients. For example, a TLS client can be created with tlsClientNew() and then new TLS sessions can be
opened with ioClientOpen().
***********************************************************************************************************************************/
#ifndef COMMON_IO_CLIENT_H
#define COMMON_IO_CLIENT_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoClient IoClient;

#include "common/io/client.intern.h"
#include "common/io/session.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct IoClientPub
{
    MemContext *memContext;                                         // Mem context
    void *driver;                                                   // Driver object
    const IoClientInterface *interface;                             // Driver interface
} IoClientPub;

// Name that identifies the client
__attribute__((always_inline)) static inline const String *
ioClientName(const IoClient *const this)
{
    return THIS_PUB(IoClient)->interface->name(THIS_PUB(IoClient)->driver);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
__attribute__((always_inline)) static inline IoClient *
ioClientMove(IoClient *const this, MemContext *const parentNew)
{
    return objMoveContext(this, parentNew);
}

// Open session
__attribute__((always_inline)) static inline IoSession *
ioClientOpen(IoClient *const this)
{
    return THIS_PUB(IoClient)->interface->open(THIS_PUB(IoClient)->driver);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
ioClientFree(IoClient *const this)
{
    objFreeContext(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *ioClientToLog(const IoClient *this);

#define FUNCTION_LOG_IO_CLIENT_TYPE                                                                                                \
    IoClient *
#define FUNCTION_LOG_IO_CLIENT_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, ioClientToLog, buffer, bufferSize)

#endif
