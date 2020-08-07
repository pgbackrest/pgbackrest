/***********************************************************************************************************************************
Io Session Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_SESSION_INTERN_H
#define COMMON_IO_SESSION_INTERN_H

#include "common/io/session.h"

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef struct IoSessionInterface
{
    // Type used to identify the session. This is stored as a pointer to a String pointer so it can be used with an existing String
    // constant (e.g. created with STRING_EXTERN()) without needing to be copied.
    const String *const *type;

    // Close the session
    void (*close)(void *driver);

    // Session file descriptor, if any
    int (*fd)(void *driver);

    // IoRead interface for the session
    IoRead *(*ioRead)(void *driver);

    // IoWrite interface for the session
    IoWrite *(*ioWrite)(void *driver);

    // Session role
    IoSessionRole (*role)(const void *driver);

    // Driver log function
    String *(*toLog)(const void *driver);
} IoSessionInterface;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoSession *ioSessionNew(void *driver, const IoSessionInterface *interface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_SESSION_INTERFACE_TYPE                                                                                     \
    IoSessionInterface *
#define FUNCTION_LOG_IO_SESSION_INTERFACE_FORMAT(value, buffer, bufferSize)                                                        \
    objToLog(&value, "IoSessionInterface", buffer, bufferSize)

#endif
