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
#define IO_CLIENT_TYPE                                             IoClient
#define IO_CLIENT_PREFIX                                           ioClient

typedef struct IoClient IoClient;

#include "common/io/session.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
IoClient *ioClientMove(IoClient *this, MemContext *parentNew);

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
void ioClientFree(IoClient *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *ioClientToLog(const IoClient *this);

#define FUNCTION_LOG_IO_CLIENT_TYPE                                                                                                \
    IoClient *
#define FUNCTION_LOG_IO_CLIENT_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, ioClientToLog, buffer, bufferSize)

#endif
