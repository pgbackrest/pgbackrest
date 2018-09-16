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

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool ioReadOpen(IoRead *this);
size_t ioRead(IoRead *this, Buffer *buffer);
void ioReadClose(IoRead *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
bool ioReadEof(const IoRead *this);
const IoFilterGroup *ioReadFilterGroup(const IoRead *this);
void ioReadFilterGroupSet(IoRead *this, IoFilterGroup *filterGroup);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_IO_READ_TYPE                                                                                                \
    IoRead *
#define FUNCTION_DEBUG_IO_READ_FORMAT(value, buffer, bufferSize)                                                                   \
    objToLog(value, "IoRead", buffer, bufferSize)

#endif
