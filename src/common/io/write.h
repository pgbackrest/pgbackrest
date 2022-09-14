/***********************************************************************************************************************************
IO Write Interface

Objects that write to some IO destination (file, socket, etc.) are implemented using this interface.  All objects are required to
implement IoWriteProcess and can optionally implement IoWriteOpen or IoWriteClose.  IoWriteOpen and IoWriteClose can be used to
allocate/open or deallocate/free resources.  An example of an IoWrite object is IoBufferWrite.
***********************************************************************************************************************************/
#ifndef COMMON_IO_WRITE_H
#define COMMON_IO_WRITE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoWrite IoWrite;

#include "common/io/filter/group.h"
#include "common/io/write.intern.h"
#include "common/type/buffer.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct IoWritePub
{
    MemContext *memContext;                                         // Mem context
    IoFilterGroup *filterGroup;                                     // IO filters
} IoWritePub;

// Filter group. Filters must be set before open and cannot be reset
FN_INLINE_ALWAYS IoFilterGroup *
ioWriteFilterGroup(IoWrite *const this)
{
    return THIS_PUB(IoWrite)->filterGroup;
}

// File descriptor for the write object. Not all write objects have a file descriptor and -1 will be returned in that case.
int ioWriteFd(const IoWrite *this);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open the IO
void ioWriteOpen(IoWrite *this);

// Write data to IO and process filters
void ioWrite(IoWrite *this, const Buffer *buffer);

// Write linefeed-terminated buffer
void ioWriteLine(IoWrite *this, const Buffer *buffer);

// Can bytes be written immediately? There are no guarantees on how much data can be written but it must be at least one byte.
typedef struct IoWriteReadyParam
{
    VAR_PARAM_HEADER;
    bool error;                                                     // Error when write not ready
} IoWriteReadyParam;

#define ioWriteReadyP(this, ...)                                                                                                   \
    ioWriteReady(this, (IoWriteReadyParam){VAR_PARAM_INIT, __VA_ARGS__})

bool ioWriteReady(IoWrite *this, IoWriteReadyParam param);

// Write string
void ioWriteStr(IoWrite *this, const String *string);

// Write linefeed-terminated string
void ioWriteStrLine(IoWrite *this, const String *string);

// Write varint-128 encoding
void ioWriteVarIntU64(IoWrite *this, uint64_t value);

// Flush any data in the output buffer. This does not end writing and will not work if filters are present.
void ioWriteFlush(IoWrite *this);

// Close the IO and write any additional data that has not been written yet
void ioWriteClose(IoWrite *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
ioWriteFree(IoWrite *const this)
{
    objFreeContext(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_IO_WRITE_TYPE                                                                                                 \
    IoWrite *
#define FUNCTION_LOG_IO_WRITE_FORMAT(value, buffer, bufferSize)                                                                    \
    objToLog(value, "IoWrite", buffer, bufferSize)

#endif
