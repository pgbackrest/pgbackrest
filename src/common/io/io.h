/***********************************************************************************************************************************
IO Functions

Common IO functions.
***********************************************************************************************************************************/
#ifndef COMMON_IO_IO_H
#define COMMON_IO_IO_H

#include <stddef.h>

#include <common/io/read.h>
#include <common/io/write.h>
#include <common/time.h>

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Copy data from source to destination (both must be open and neither will be closed)
void ioCopy(IoRead *source, IoWrite *destination);

// Read all IO into a buffer
Buffer *ioReadBuf(IoRead *read);

// Read all IO but don't store it. Useful for calculating checksums, size, etc.
bool ioReadDrain(IoRead *read);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Buffer size
size_t ioBufferSize(void);
void ioBufferSizeSet(size_t bufferSize);

// I/O timeout in milliseconds. Used to timeout on connections and read/write operations. Note that an *entire* read/write operation
// does not need to take place within this timeout but at least some progress needs to be made, even if it is only a byte.
TimeMSec ioTimeoutMs(void);
void ioTimeoutMsSet(TimeMSec timeout);

#endif
