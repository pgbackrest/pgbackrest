/***********************************************************************************************************************************
Io Client Interface

Create sessions for protocol clients. For example, a TLS client can be created with tlsClientNewP() and then new TLS sessions can be
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
    void *driver;                                                   // Driver object
    const IoClientInterface *interface;                             // Driver interface
} IoClientPub;

// Name that identifies the client
FN_INLINE_ALWAYS const String *
ioClientName(const IoClient *const this)
{
    return THIS_PUB(IoClient)->interface->name(THIS_PUB(IoClient)->driver);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
FN_INLINE_ALWAYS IoClient *
ioClientMove(IoClient *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Open session
FN_INLINE_ALWAYS IoSession *
ioClientOpen(IoClient *const this)
{
    return THIS_PUB(IoClient)->interface->open(THIS_PUB(IoClient)->driver);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
ioClientFree(IoClient *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void ioClientToLog(const IoClient *this, StringStatic *debugLog);

#define FUNCTION_LOG_IO_CLIENT_TYPE                                                                                                \
    IoClient *
#define FUNCTION_LOG_IO_CLIENT_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_OBJECT_FORMAT(value, ioClientToLog, buffer, bufferSize)

#endif
