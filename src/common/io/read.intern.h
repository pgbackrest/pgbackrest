/***********************************************************************************************************************************
IO Read Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_READ_INTERN_H
#define COMMON_IO_READ_INTERN_H

#include "common/io/read.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef bool (*IoReadInterfaceEof)(void *driver);
typedef void (*IoReadInterfaceClose)(void *driver);
typedef bool (*IoReadInterfaceOpen)(void *driver);
typedef int (*IoReadInterfaceHandle)(void *driver);
typedef size_t (*IoReadInterfaceRead)(void *driver, Buffer *buffer, bool block);

typedef struct IoReadInterface
{
    bool block;                                               // Do reads block when buffer is larger than available bytes?
    IoReadInterfaceEof eof;
    IoReadInterfaceClose close;
    IoReadInterfaceHandle handle;
    IoReadInterfaceOpen open;
    IoReadInterfaceRead read;
} IoReadInterface;

#define ioReadNewP(driver, ...)                                                                                                    \
    ioReadNew(driver, (IoReadInterface){__VA_ARGS__})

IoRead *ioReadNew(void *driver, IoReadInterface interface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_READ_INTERFACE_TYPE                                                                                        \
    IoReadInterface
#define FUNCTION_LOG_IO_READ_INTERFACE_FORMAT(value, buffer, bufferSize)                                                           \
    objToLog(&value, "IoReadInterface", buffer, bufferSize)

#endif
