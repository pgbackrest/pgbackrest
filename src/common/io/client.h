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

#include "common/io/session.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
__attribute__((always_inline)) static inline IoClient *
ioClientMove(IoClient *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

// Open session
IoSession *ioClientOpen(IoClient *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Name that identifies the client
const String *ioClientName(IoClient *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
ioClientFree(IoClient *this)
{
    objFree(this);
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
