/***********************************************************************************************************************************
File Descriptor Io Write

Write to a file descriptor using the IoWrite interface.
***********************************************************************************************************************************/
#ifndef COMMON_IO_FDWRITE_H
#define COMMON_IO_FDWRITE_H

#include "common/io/write.h"
#include "common/time.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN IoWrite *ioFdWriteNew(const String *name, int fd, TimeMSec timeout);

// Construct and open write fd
FN_INLINE_ALWAYS IoWrite *
ioFdWriteNewOpen(const String *const name, const int fd, const TimeMSec timeout)
{
    IoWrite *const result = ioFdWriteNew(name, fd, timeout);
    ioWriteOpen(result);
    return result;
}

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Write a string to the specified file descriptor
FN_EXTERN void ioFdWriteOneStr(int fd, const String *string);

#endif
