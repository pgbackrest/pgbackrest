/***********************************************************************************************************************************
Io Server Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_SERVER_INTERN_H
#define COMMON_IO_SERVER_INTERN_H

#include "common/io/server.h"
#include "common/io/session.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef struct IoServerInterface
{
    // Type used to identify the server
    StringId type;

    // Server name, usually address:port or some other unique identifier
    const String *(*name)(void *driver);

    // Accept a session
    IoSession *(*accept)(void *driver, IoSession *session);

    // Driver log function
    void (*toLog)(const void *driver, StringStatic *debugLog);
} IoServerInterface;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN IoServer *ioServerNew(void *driver, const IoServerInterface *interface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_SERVER_INTERFACE_TYPE                                                                                      \
    IoServerInterface *
#define FUNCTION_LOG_IO_SERVER_INTERFACE_FORMAT(value, buffer, bufferSize)                                                         \
    objNameToLog(&value, "IoServerInterface", buffer, bufferSize)

#endif
