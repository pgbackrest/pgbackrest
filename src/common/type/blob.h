/***********************************************************************************************************************************
Blob Handler

Efficiently store large numbers of small allocations by packing them onto larger blocks. Note that the allocations will not be
aligned unless all of them are aligned.

A usage example is to store zero-terminated strings, which do not need to be aligned, and where alignment might waste significant
space.

Note that this code is fairly primitive, and this should probably be a capability of memory contexts. However, for now it reduces
memory requirements for large numbers of zero-terminated strings.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_BLOB_H
#define COMMON_TYPE_BLOB_H

/***********************************************************************************************************************************
Size of blocks allocated for blob data
***********************************************************************************************************************************/
#ifndef BLOB_BLOCK_SIZE
    #define BLOB_BLOCK_SIZE                                         (64 * 1024)
#endif

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct Blob Blob;

#include "common/type/object.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
Blob *blbNew(void);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add data to the blob
const void *blbAdd(Blob *this, const void *data, size_t size);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
strBlbFree(Blob *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_BLOB_TYPE                                                                                                     \
    Blob *
#define FUNCTION_LOG_BLOB_FORMAT(value, buffer, bufferSize)                                                                        \
    objToLog(value, "Blob", buffer, bufferSize)

#endif
