/***********************************************************************************************************************************
Storage Write Interface
***********************************************************************************************************************************/
#ifndef STORAGE_WRITE_H
#define STORAGE_WRITE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWrite StorageWrite;

#include "common/io/write.h"
#include "common/type/buffer.h"
#include "common/type/object.h"
#include "common/type/string.h"
#include "common/type/stringId.h"
#include "storage/write.intern.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
FN_INLINE_ALWAYS StorageWrite *
storageWriteMove(StorageWrite *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Will the file be written atomically? Atomic writes means the file will be complete or be missing. Filesystems have different ways
// to accomplish this.
FN_INLINE_ALWAYS bool
storageWriteAtomic(const StorageWrite *const this)
{
    return storageWriteInterface(this)->atomic;
}

// Will the path be created if required?
FN_INLINE_ALWAYS bool
storageWriteCreatePath(const StorageWrite *const this)
{
    return storageWriteInterface(this)->createPath;
}

// Write interface
FN_INLINE_ALWAYS IoWrite *
storageWriteIo(const StorageWrite *const this)
{
    return THIS_PUB(StorageWrite)->io;
}

// File mode
FN_INLINE_ALWAYS mode_t
storageWriteModeFile(const StorageWrite *const this)
{
    return storageWriteInterface(this)->modeFile;
}

// Path mode (if the destination path needs to be create)
FN_INLINE_ALWAYS mode_t
storageWriteModePath(const StorageWrite *const this)
{
    return storageWriteInterface(this)->modePath;
}

// File name
FN_INLINE_ALWAYS const String *
storageWriteName(const StorageWrite *const this)
{
    return storageWriteInterface(this)->name;
}

// Will the file be synced before it is closed?
FN_INLINE_ALWAYS bool
storageWriteSyncFile(const StorageWrite *const this)
{
    return storageWriteInterface(this)->syncFile;
}

// Will the path be synced after the file is closed?
FN_INLINE_ALWAYS bool
storageWriteSyncPath(const StorageWrite *const this)
{
    return storageWriteInterface(this)->syncPath;
}

// Will the file be truncated if it exists?
FN_INLINE_ALWAYS bool
storageWriteTruncate(const StorageWrite *const this)
{
    return storageWriteInterface(this)->truncate;
}

// File type
FN_INLINE_ALWAYS StringId
storageWriteType(const StorageWrite *const this)
{
    return storageWriteInterface(this)->type;
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
storageWriteFree(StorageWrite *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void storageWriteToLog(const StorageWrite *this, StringStatic *debugLog);

#define FUNCTION_LOG_STORAGE_WRITE_TYPE                                                                                            \
    StorageWrite *
#define FUNCTION_LOG_STORAGE_WRITE_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_OBJECT_FORMAT(value, storageWriteToLog, buffer, bufferSize)

#endif
