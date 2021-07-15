/***********************************************************************************************************************************
File Descriptor Read

Read from a file descriptor using the IoRead interface.
***********************************************************************************************************************************/
#ifndef COMMON_IO_FDREAD_H
#define COMMON_IO_FDREAD_H

#include "common/io/read.h"
#include "common/time.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoRead *ioFdReadNew(const String *name, int fd, TimeMSec timeout);

// Construct and open read fd
__attribute__((always_inline)) static inline IoRead *
ioFdReadNewOpen(const String *const name, const int fd, const TimeMSec timeout)
{
    IoRead *const result = ioFdReadNew(name, fd, timeout);
    ioReadOpen(result);
    return result;
}

#endif
