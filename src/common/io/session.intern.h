/***********************************************************************************************************************************
IO Session Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_SESSION_INTERN_H
#define COMMON_IO_SESSION_INTERN_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#include "common/io/session.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct IoSessionInterface
{
    bool block;                                               // Do reads block when buffer is larger than available bytes?

    bool (*eof)(void *driver);
    void (*close)(void *driver);
    bool (*open)(void *driver);
    int (*handle)(const void *driver);
    size_t (*read)(void *driver, Buffer *buffer, bool block);
} IoSessionInterface;

#define ioSessionNewP(driver, ...)                                                                                                 \
    ioSessionNew(driver, (IoSessionInterface){__VA_ARGS__})

IoSession *ioSessionNew(void *driver, IoSessionInterface interface);

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
    IoSessionInterface
#define FUNCTION_LOG_IO_SESSION_INTERFACE_FORMAT(value, buffer, bufferSize)                                                        \
    objToLog(&value, "IoSessionInterface", buffer, bufferSize)

#endif
