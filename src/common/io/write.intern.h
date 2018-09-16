/***********************************************************************************************************************************
IO Write Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_WRITE_INTERN_H
#define COMMON_IO_WRITE_INTERN_H

#include "common/io/write.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef void (*IoWriteInterfaceClose)(void *driver);
typedef void (*IoWriteInterfaceOpen)(void *driver);
typedef void (*IoWriteInterfaceWrite)(void *driver, const Buffer *buffer);

typedef struct IoWriteInterface
{
    IoWriteInterfaceClose close;
    IoWriteInterfaceOpen open;
    IoWriteInterfaceWrite write;
} IoWriteInterface;

#define ioWriteNewP(driver, ...)                                                                                                   \
    ioWriteNew(driver, (IoWriteInterface){__VA_ARGS__})

IoWrite *ioWriteNew(void *driver, IoWriteInterface interface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_IO_WRITE_INTERFACE_TYPE                                                                                     \
    IoWriteInterface
#define FUNCTION_DEBUG_IO_WRITE_INTERFACE_FORMAT(value, buffer, bufferSize)                                                        \
    objToLog(&value, "IoWriteInterface", buffer, bufferSize)

#endif
