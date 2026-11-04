/***********************************************************************************************************************************
Posix Storage Direct I/O

Direct I/O requires the memory address, transfer size, and file offset of every read/write to be aligned, so data is staged through
an aligned bounce buffer.
***********************************************************************************************************************************/
#ifndef STORAGE_POSIX_DIRECT_H
#define STORAGE_POSIX_DIRECT_H

#include "common/type/buffer.h"

#ifndef STORAGE_POSIX_DIRECT_FLAG
#define STORAGE_POSIX_DIRECT_FLAG 0
#endif

/***********************************************************************************************************************************
Alignment used when the device does not report one
***********************************************************************************************************************************/
#define STORAGE_POSIX_DIRECT_ALIGN_DEFAULT                          4096

/***********************************************************************************************************************************
Aligned bounce buffer
***********************************************************************************************************************************/
typedef struct StoragePosixDirect
{
    size_t align;                                                   // Required alignment of address, size, and offset
    size_t size;                                                    // Usable size of the buffer (multiple of align)
    Buffer *buffer;                                                 // Buffer that owns the memory
    uint8_t *ptr;                                                   // Aligned pointer into buffer
} StoragePosixDirect;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open flag for direct I/O (0 when not supported by the platform)
FN_EXTERN int storagePosixDirectFlag(void);

// New aligned bounce buffer for an open file using device alignment when available. The buffer is allocated in the current
// mem context.
FN_EXTERN Buffer *storagePosixDirectNew(int fd, StoragePosixDirect *result);

#endif
