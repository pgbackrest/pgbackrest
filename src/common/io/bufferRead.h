/***********************************************************************************************************************************
Buffer IO Read

Read from a Buffer object using the IoRead interface.
***********************************************************************************************************************************/
#ifndef COMMON_IO_BUFFERREAD_H
#define COMMON_IO_BUFFERREAD_H

#include "common/io/read.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN IoRead *ioBufferReadNew(const Buffer *buffer);

// Construct and open buffer read
FN_INLINE_ALWAYS IoRead *
ioBufferReadNewOpen(const Buffer *const buffer)
{
    IoRead *const result = ioBufferReadNew(buffer);
    ioReadOpen(result);
    return result;
}

#endif
