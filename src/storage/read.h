/***********************************************************************************************************************************
Storage Read Interface
***********************************************************************************************************************************/
#ifndef STORAGE_READ_H
#define STORAGE_READ_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageRead StorageRead;

#include "common/io/read.h"
#include "common/type/object.h"
#include "common/type/stringId.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
FN_INLINE_ALWAYS StorageRead *
storageReadMove(StorageRead *const this, MemContext *const parentNew)
{
    return objMoveContext(this, parentNew);
}

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct StorageReadPub
{
    MemContext *memContext;                                         // Mem context
    const StorageReadInterface *interface;                          // File data (name, driver type, etc.)
    IoRead *io;                                                     // Read interface
} StorageReadPub;

// Should a missing file be ignored?
FN_INLINE_ALWAYS bool
storageReadIgnoreMissing(const StorageRead *const this)
{
    return THIS_PUB(StorageRead)->interface->ignoreMissing;
}

// Read interface
FN_INLINE_ALWAYS IoRead *
storageReadIo(const StorageRead *const this)
{
    return THIS_PUB(StorageRead)->io;
}

// Is there a read limit? NULL for no limit.
FN_INLINE_ALWAYS const Variant *
storageReadLimit(const StorageRead *const this)
{
    return THIS_PUB(StorageRead)->interface->limit;
}

// File name
FN_INLINE_ALWAYS const String *
storageReadName(const StorageRead *const this)
{
    return THIS_PUB(StorageRead)->interface->name;
}

// Is there a read limit? NULL for no limit.
FN_INLINE_ALWAYS uint64_t
storageReadOffset(const StorageRead *const this)
{
    return THIS_PUB(StorageRead)->interface->offset;
}

// Get file type
FN_INLINE_ALWAYS StringId
storageReadType(const StorageRead *const this)
{
    return THIS_PUB(StorageRead)->interface->type;
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
storageReadFree(StorageRead *const this)
{
    objFreeContext(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN String *storageReadToLog(const StorageRead *this);

#define FUNCTION_LOG_STORAGE_READ_TYPE                                                                                             \
    StorageRead *
#define FUNCTION_LOG_STORAGE_READ_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, storageReadToLog, buffer, bufferSize)

#endif
