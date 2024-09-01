/***********************************************************************************************************************************
IO Write Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_WRITE_INTERN_H
#define COMMON_IO_WRITE_INTERN_H

#include "common/io/write.h"
#include "common/type/buffer.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct IoWriteInterface
{
    void (*close)(void *driver);
    int (*fd)(const void *driver);
    void (*open)(void *driver);

    // Can bytes be written immediately? There are no guarantees on how much data can be written but it must be at least one byte.
    // Optionally error when write is not ready.
    bool (*ready)(void *driver, bool error);

    void (*write)(void *driver, const Buffer *buffer);
} IoWriteInterface;

#define ioWriteNewP(driver, ...)                                                                                                   \
    ioWriteNew(driver, (IoWriteInterface){__VA_ARGS__})

FN_EXTERN IoWrite *ioWriteNew(void *driver, IoWriteInterface interface);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct IoWritePub
{
    void *driver;                                                   // Driver object
    IoWriteInterface interface;                                     // Driver interface
    IoFilterGroup *filterGroup;                                     // IO filters
} IoWritePub;

// Driver for the write object
FN_INLINE_ALWAYS void *
ioWriteDriver(const IoWrite *const this)
{
    return THIS_PUB(IoWrite)->driver;
}

// Interface for the write object
FN_INLINE_ALWAYS const IoWriteInterface *
ioWriteInterface(const IoWrite *const this)
{
    return &THIS_PUB(IoWrite)->interface;
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_WRITE_INTERFACE_TYPE                                                                                       \
    IoWriteInterface
#define FUNCTION_LOG_IO_WRITE_INTERFACE_FORMAT(value, buffer, bufferSize)                                                          \
    objNameToLog(&value, "IoWriteInterface", buffer, bufferSize)

#endif
