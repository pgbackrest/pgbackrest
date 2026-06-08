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
typedef struct StorageWritePub
{
    StringId type;                                                  // Storage type
    const String *name;                                             // File name
    bool createPath;                                                // Will the path be created if required?
    bool atomic;                                                    // Are writes atomic?
    bool truncate;                                                  // Truncate file if it exists
    bool syncPath;                                                  // Will path be synced?
    bool syncFile;                                                  // Will file be synced?
    const String *user;                                             // Set user ownership
    const String *group;                                            // Set group ownership
    mode_t modePath;                                                // Path mode
    mode_t modeFile;                                                // File mode
    time_t timeModified;                                            // Set modification time
    IoWrite *io;                                                    // Write interface
} StorageWritePub;

// Will the file be written atomically? Atomic writes means the file will be complete or be missing. Filesystems have different ways
// to accomplish this.
FN_INLINE_ALWAYS bool
storageWriteAtomic(const StorageWrite *const this)
{
    return THIS_PUB(StorageWrite)->atomic;
}

// Will the path be created if required?
FN_INLINE_ALWAYS bool
storageWriteCreatePath(const StorageWrite *const this)
{
    return THIS_PUB(StorageWrite)->createPath;
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
    return THIS_PUB(StorageWrite)->modeFile;
}

// Path mode (if the destination path needs to be create)
FN_INLINE_ALWAYS mode_t
storageWriteModePath(const StorageWrite *const this)
{
    return THIS_PUB(StorageWrite)->modePath;
}

// File name
FN_INLINE_ALWAYS const String *
storageWriteName(const StorageWrite *const this)
{
    return THIS_PUB(StorageWrite)->name;
}

// Will the file be synced before it is closed?
FN_INLINE_ALWAYS bool
storageWriteSyncFile(const StorageWrite *const this)
{
    return THIS_PUB(StorageWrite)->syncFile;
}

// Will the path be synced after the file is closed?
FN_INLINE_ALWAYS bool
storageWriteSyncPath(const StorageWrite *const this)
{
    return THIS_PUB(StorageWrite)->syncPath;
}

// Will the file be truncated if it exists?
FN_INLINE_ALWAYS bool
storageWriteTruncate(const StorageWrite *const this)
{
    return THIS_PUB(StorageWrite)->truncate;
}

// File type
FN_INLINE_ALWAYS StringId
storageWriteType(const StorageWrite *const this)
{
    return THIS_PUB(StorageWrite)->type;
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
Helper functions
***********************************************************************************************************************************/
// Calculate chunk size for multipart upload to an object store
FN_EXTERN size_t storageWriteChunkSize(
    size_t chunkSizeDefault, unsigned int defaultSplit, unsigned int maxSplit, unsigned int chunkIdx);

// Resize chunk buffer for multipart upload to an object store
FN_EXTERN void storageWriteChunkBufferResize(const Buffer *input, Buffer *const chunk, size_t chunkSizeMax);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void storageWriteToLog(const StorageWrite *this, StringStatic *debugLog);

#define FUNCTION_LOG_STORAGE_WRITE_TYPE                                                                                            \
    StorageWrite *
#define FUNCTION_LOG_STORAGE_WRITE_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_OBJECT_FORMAT(value, storageWriteToLog, buffer, bufferSize)

#endif
