/***********************************************************************************************************************************
IO Filter Interface Internal
***********************************************************************************************************************************/
#ifndef COMMON_IO_FILTER_FILTER_INTERN_H
#define COMMON_IO_FILTER_FILTER_INTERN_H

#include "common/io/filter/filter.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef bool (*IoFilterInterfaceDone)(const void *driver);
typedef void (*IoFilterInterfaceProcessIn)(void *driver, const Buffer *);
typedef void (*IoFilterInterfaceProcessInOut)(void *driver, const Buffer *, Buffer *);
typedef bool (*IoFilterInterfaceInputSame)(const void *driver);
typedef Variant *(*IoFilterInterfaceResult)(const void *driver);

typedef struct IoFilterInterface
{
    IoFilterInterfaceDone done;
    IoFilterInterfaceProcessIn in;
    IoFilterInterfaceProcessInOut inOut;
    IoFilterInterfaceInputSame inputSame;
    IoFilterInterfaceResult result;
} IoFilterInterface;

#define ioFilterNewP(type, driver, ...)                                                                                            \
    ioFilterNew(type, driver, (IoFilterInterface){__VA_ARGS__})

IoFilter *ioFilterNew(const String *type, void *driver, IoFilterInterface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_IO_FILTER_INTERFACE_TYPE                                                                                    \
    IoFilterInterface *
#define FUNCTION_DEBUG_IO_FILTER_INTERFACE_FORMAT(value, buffer, bufferSize)                                                       \
    objToLog(&value, "IoFilterInterface", buffer, bufferSize)

#endif
