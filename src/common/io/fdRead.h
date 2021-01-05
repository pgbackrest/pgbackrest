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
IoRead *ioFdReadNewOpen(const String *name, int fd, TimeMSec timeout);

#endif
