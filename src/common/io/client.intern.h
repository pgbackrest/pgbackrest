/***********************************************************************************************************************************
Io Client Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_CLIENT_INTERN_H
#define COMMON_IO_CLIENT_INTERN_H

#include "common/io/client.h"

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef struct IoClientInterface
{
    // Type used to identify the client
    const String *const *type;

    // Open a session
    IoSession *(*open)(void *driver);

    // Driver log function
    String *(*toLog)(const void *driver);
} IoClientInterface;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoClient *ioClientNew(void *driver, const IoClientInterface *interface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_CLIENT_INTERFACE_TYPE                                                                                      \
    IoClientInterface *
#define FUNCTION_LOG_IO_CLIENT_INTERFACE_FORMAT(value, buffer, bufferSize)                                                         \
    objToLog(&value, "IoClientInterface", buffer, bufferSize)

#endif
