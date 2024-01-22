/***********************************************************************************************************************************
IO Read Interface

Objects that read from some IO source (file, socket, etc.) are implemented using this interface. All objects are required to
implement IoReadProcess and can optionally implement IoReadOpen, IoReadClose, or IoReadEof. IoReadOpen and IoReadClose can be used
to allocate/open or deallocate/free resources. If IoReadEof is not implemented then ioReadEof() will always return false. An example
of an IoRead object is IoBufferRead.
***********************************************************************************************************************************/
#ifndef COMMON_IO_READ_H
#define COMMON_IO_READ_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoRead IoRead;

#include "common/io/filter/group.h"
#include "common/io/read.intern.h"
#include "common/type/buffer.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Do reads block when more bytes are requested than are available to read?
FN_INLINE_ALWAYS bool
ioReadBlock(const IoRead *const this)
{
    return THIS_PUB(IoRead)->interface.block;
}

// Is IO at EOF? All driver reads are complete and all data has been flushed from the filters (if any).
FN_INLINE_ALWAYS bool
ioReadEof(const IoRead *const this)
{
    ASSERT_INLINE(THIS_PUB(IoRead)->opened && !THIS_PUB(IoRead)->closed);
    return THIS_PUB(IoRead)->eofAll;
}

// Get filter group if filters need to be added
FN_INLINE_ALWAYS IoFilterGroup *
ioReadFilterGroup(IoRead *const this)
{
    return THIS_PUB(IoRead)->filterGroup;
}

// File descriptor for the read object. Not all read objects have a file descriptor and -1 will be returned in that case.
FN_EXTERN int ioReadFd(const IoRead *this);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open the IO
FN_EXTERN bool ioReadOpen(IoRead *this);

// Read data from IO and process filters
FN_EXTERN size_t ioRead(IoRead *this, Buffer *buffer);

// Same as ioRead() but optimized for small reads (intended for making repetitive reads that are smaller than ioBufferSize())
FN_EXTERN size_t ioReadSmall(IoRead *this, Buffer *buffer);

// Read linefeed-terminated string and optionally error on eof
FN_EXTERN String *ioReadLineParam(IoRead *this, bool allowEof);

// Read linefeed-terminated string
FN_INLINE_ALWAYS String *
ioReadLine(IoRead *const this)
{
    return ioReadLineParam(this, false);
}

// Read varint-128 encoding
FN_EXTERN uint64_t ioReadVarIntU64(IoRead *this);

// Are there bytes ready to read immediately? There are no guarantees on how much data is available to read but it must be at least
// one byte.
typedef struct IoReadReadyParam
{
    VAR_PARAM_HEADER;
    bool error;                                                     // Error when read not ready
} IoReadReadyParam;

#define ioReadReadyP(this, ...)                                                                                                    \
    ioReadReady(this, (IoReadReadyParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN bool ioReadReady(IoRead *this, IoReadReadyParam param);

// Flush all remaining bytes and return bytes flushed. Optionally error when bytes are flushed.
typedef struct IoReadFlushParam
{
    VAR_PARAM_HEADER;
    bool errorOnBytes;                                              // Error when any bytes are found during flush
} IoReadFlushParam;

#define ioReadFlushP(this, ...)                                                                                                    \
    ioReadFlush(this, (IoReadFlushParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN uint64_t ioReadFlush(IoRead *this, IoReadFlushParam param);

// Close the IO
FN_EXTERN void ioReadClose(IoRead *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
ioReadFree(IoRead *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_READ_TYPE                                                                                                  \
    IoRead *
#define FUNCTION_LOG_IO_READ_FORMAT(value, buffer, bufferSize)                                                                     \
    objNameToLog(value, "IoRead", buffer, bufferSize)

#endif
