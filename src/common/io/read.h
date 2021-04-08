/***********************************************************************************************************************************
IO Read Interface

Objects that read from some IO source (file, socket, etc.) are implemented using this interface.  All objects are required to
implement IoReadProcess and can optionally implement IoReadOpen, IoReadClose, or IoReadEof.  IoReadOpen and IoReadClose can be used
to allocate/open or deallocate/free resources.  If IoReadEof is not implemented then ioReadEof() will always return false.  An
example of an IoRead object is IoBufferRead.
***********************************************************************************************************************************/
#ifndef COMMON_IO_READ_H
#define COMMON_IO_READ_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoRead IoRead;

#include "common/io/filter/group.h"
#include "common/type/buffer.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open the IO
bool ioReadOpen(IoRead *this);

// Read data from IO and process filters
size_t ioRead(IoRead *this, Buffer *buffer);

// Same as ioRead() but optimized for small reads (intended for making repetitive reads that are smaller than ioBufferSize())
size_t ioReadSmall(IoRead *this, Buffer *buffer);

// Read linefeed-terminated string
String *ioReadLine(IoRead *this);

// Read linefeed-terminated string and optionally error on eof
String *ioReadLineParam(IoRead *this, bool allowEof);

// Are there bytes ready to read immediately? There are no guarantees on how much data is available to read but it must be at least
// one byte.
typedef struct IoReadReadyParam
{
    VAR_PARAM_HEADER;
    bool error;                                                     // Error when read not ready
} IoReadReadyParam;

#define ioReadReadyP(this, ...)                                                                                                    \
    ioReadReady(this, (IoReadReadyParam){VAR_PARAM_INIT, __VA_ARGS__})

bool ioReadReady(IoRead *this, IoReadReadyParam param);

// Close the IO
void ioReadClose(IoRead *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Do reads block when more bytes are requested than are available to read?
bool ioReadBlock(const IoRead *this);

// Is IO at EOF? All driver reads are complete and all data has been flushed from the filters (if any).
bool ioReadEof(const IoRead *this);

// Get filter group if filters need to be added
IoFilterGroup *ioReadFilterGroup(const IoRead *this);

// File descriptor for the read object. Not all read objects have a file descriptor and -1 will be returned in that case.
int ioReadFd(const IoRead *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
ioReadFree(IoRead *this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_READ_TYPE                                                                                                  \
    IoRead *
#define FUNCTION_LOG_IO_READ_FORMAT(value, buffer, bufferSize)                                                                     \
    objToLog(value, "IoRead", buffer, bufferSize)

#endif
