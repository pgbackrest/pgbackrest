/***********************************************************************************************************************************
Buffer IO Write

Write to a Buffer object using the IoWrite interface.
***********************************************************************************************************************************/
#ifndef COMMON_IO_BUFFERWRITE_H
#define COMMON_IO_BUFFERWRITE_H

#include "common/io/write.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoWrite *ioBufferWriteNew(Buffer *buffer);

// Construct and open buffer write
__attribute__((always_inline)) static inline IoWrite *
ioBufferWriteNewOpen(Buffer *const buffer)
{
    IoWrite *const result = ioBufferWriteNew(buffer);
    ioWriteOpen(result);
    return result;
}

#endif
