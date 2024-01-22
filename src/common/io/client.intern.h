/***********************************************************************************************************************************
Io Client Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_CLIENT_INTERN_H
#define COMMON_IO_CLIENT_INTERN_H

#include "common/io/client.h"
#include "common/io/session.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef struct IoClientInterface
{
    // Type used to identify the client
    StringId type;

    // Client name, usually host:port or some other unique identifier
    const String *(*name)(void *driver);

    // Open a session
    IoSession *(*open)(void *driver);

    // Driver log function
    void (*toLog)(const void *driver, StringStatic *debugLog);
} IoClientInterface;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN IoClient *ioClientNew(void *driver, const IoClientInterface *interface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_CLIENT_INTERFACE_TYPE                                                                                      \
    IoClientInterface *
#define FUNCTION_LOG_IO_CLIENT_INTERFACE_FORMAT(value, buffer, bufferSize)                                                         \
    objNameToLog(&value, "IoClientInterface", buffer, bufferSize)

#endif
