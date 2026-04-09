/***********************************************************************************************************************************
IO Read Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_READ_INTERN_H
#define COMMON_IO_READ_INTERN_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#include "common/io/read.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct IoReadInterface
{
    bool block;                                               // Do reads block when buffer is larger than available bytes?

    bool (*eof)(void *driver);
    void (*close)(void *driver);
    bool (*open)(void *driver);
    int (*fd)(const void *driver);
    size_t (*read)(void *driver, Buffer *buffer, bool block);

    // Are there bytes ready to read immediately? There are no guarantees on how much data is available to read but it must be at
    // least one byte. Optionally error when read is not ready.
    bool (*ready)(void *driver, bool error);
} IoReadInterface;

#define ioReadNewP(driver, ...)                                                                                                    \
    ioReadNew(driver, (IoReadInterface){__VA_ARGS__})

FN_EXTERN IoRead *ioReadNew(void *driver, IoReadInterface interface);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct IoReadPub
{
    void *driver;                                                   // Driver object
    IoReadInterface interface;                                      // Driver interface
    IoFilterGroup *filterGroup;                                     // IO filters
    bool eofAll;                                                    // Is the read done (read and filters complete)?

#ifdef DEBUG
    bool opened;                                                    // Has the io been opened?
    bool closed;                                                    // Has the io been closed?
#endif
} IoReadPub;

// Driver for the read object
FN_INLINE_ALWAYS void *
ioReadDriver(const IoRead *const this)
{
    return THIS_PUB(IoRead)->driver;
}

// Interface for the read object
FN_INLINE_ALWAYS const IoReadInterface *
ioReadInterface(const IoRead *const this)
{
    return &THIS_PUB(IoRead)->interface;
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_READ_INTERFACE_TYPE                                                                                        \
    IoReadInterface
#define FUNCTION_LOG_IO_READ_INTERFACE_FORMAT(value, buffer, bufferSize)                                                           \
    objNameToLog(&value, "IoReadInterface", buffer, bufferSize)

#endif
