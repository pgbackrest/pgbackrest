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

IoRead *ioReadNew(void *driver, IoReadInterface interface);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Driver for the read object
void *ioReadDriver(IoRead *this);

// Interface for the read object
const IoReadInterface *ioReadInterface(const IoRead *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_READ_INTERFACE_TYPE                                                                                        \
    IoReadInterface
#define FUNCTION_LOG_IO_READ_INTERFACE_FORMAT(value, buffer, bufferSize)                                                           \
    objToLog(&value, "IoReadInterface", buffer, bufferSize)

#endif
