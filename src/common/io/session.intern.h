/***********************************************************************************************************************************
IO Session Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_SESSION_INTERN_H
#define COMMON_IO_SESSION_INTERN_H

#include "common/io/session.h"

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef struct IoSessionInterface
{
    void (*close)(void *driver);
    IoRead *(*ioRead)(void *driver);
    IoWrite *(*ioWrite)(void *driver);
} IoSessionInterface;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoSession *ioSessionNew(void *driver, const IoSessionInterface *interface);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Driver for the read object
// !!! HOPEFULLY WE DON'T NEED THIS
// void *ioSessionDriver(IoSession *this);

// Interface for the read object
// !!! HOPEFULLY WE DON'T NEED THIS
// const IoSessionInterface *ioSessionInterface(const IoSession *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_SESSION_INTERFACE_TYPE                                                                                     \
    IoSessionInterface *
#define FUNCTION_LOG_IO_SESSION_INTERFACE_FORMAT(value, buffer, bufferSize)                                                        \
    objToLog(&value, "IoSessionInterface", buffer, bufferSize)

#endif
