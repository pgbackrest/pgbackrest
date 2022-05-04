/***********************************************************************************************************************************
Io Session Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_SESSION_INTERN_H
#define COMMON_IO_SESSION_INTERN_H

#include "common/io/session.h"
#include "common/io/write.h"

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef struct IoSessionInterface
{
    // Type used to identify the session
    StringId type;

    // Close the session
    void (*close)(void *driver);

    // Session file descriptor, if any
    int (*fd)(void *driver);

    // IoRead interface for the session
    IoRead *(*ioRead)(void *driver, bool ignoreUnexpectedEof);

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
Getters/Setters
***********************************************************************************************************************************/
// Has the session been authenticated?
void ioSessionAuthenticatedSet(IoSession *this, bool authenticated);

// Set the peer name
void ioSessionPeerNameSet(IoSession *this, const String *peerName);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_SESSION_INTERFACE_TYPE                                                                                     \
    IoSessionInterface *
#define FUNCTION_LOG_IO_SESSION_INTERFACE_FORMAT(value, buffer, bufferSize)                                                        \
    objToLog(&value, "IoSessionInterface", buffer, bufferSize)

#endif
