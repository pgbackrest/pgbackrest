/***********************************************************************************************************************************
File Descriptor Functions
***********************************************************************************************************************************/
#ifndef COMMON_IO_FD_H
#define COMMON_IO_FD_H

#include "common/time.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Wait until the file descriptor is ready to read/write or timeout
bool fdReady(int fd, bool read, bool write, TimeMSec timeout);

// Wait until the file descriptor is ready to read or timeout
__attribute__((always_inline)) static inline bool
fdReadyRead(int fd, TimeMSec timeout)
{
    return fdReady(fd, true, false, timeout);
}

// Wait until the file descriptor is ready to write or timeout
__attribute__((always_inline)) static inline bool
fdReadyWrite(int fd, TimeMSec timeout)
{
    return fdReady(fd, false, true, timeout);
}

#endif
