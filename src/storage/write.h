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
#include "storage/write.intern.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
__attribute__((always_inline)) static inline StorageWrite *
storageWriteMove(StorageWrite *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct StorageWritePub
{
    MemContext *memContext;                                         // Mem context
    const StorageWriteInterface *interface;                         // File data (name, driver type, etc.)
    IoWrite *io;                                                    // Write interface
} StorageWritePub;

// Will the file be written atomically? Atomic writes means the file will be complete or be missing. Filesystems have different ways
// to accomplish this.
__attribute__((always_inline)) static inline bool
storageWriteAtomic(const StorageWrite *this)
{
    return THIS_PUB(StorageWrite)->interface->atomic;
}

// Will the path be created if required?
__attribute__((always_inline)) static inline bool
storageWriteCreatePath(const StorageWrite *this)
{
    return THIS_PUB(StorageWrite)->interface->createPath;
}

// Write interface
__attribute__((always_inline)) static inline IoWrite *
storageWriteIo(const StorageWrite *this)
{
    return THIS_PUB(StorageWrite)->io;
}

// File mode
__attribute__((always_inline)) static inline mode_t
storageWriteModeFile(const StorageWrite *this)
{
    return THIS_PUB(StorageWrite)->interface->modeFile;
}

// Path mode (if the destination path needs to be create)
__attribute__((always_inline)) static inline mode_t
storageWriteModePath(const StorageWrite *this)
{
    return THIS_PUB(StorageWrite)->interface->modePath;
}

// File name
__attribute__((always_inline)) static inline const String *
storageWriteName(const StorageWrite *this)
{
    return THIS_PUB(StorageWrite)->interface->name;
}

// Will the file be synced before it is closed?
__attribute__((always_inline)) static inline bool
storageWriteSyncFile(const StorageWrite *this)
{
    return THIS_PUB(StorageWrite)->interface->syncFile;
}

// Will the path be synced after the file is closed?
__attribute__((always_inline)) static inline bool
storageWriteSyncPath(const StorageWrite *this)
{
    return THIS_PUB(StorageWrite)->interface->syncPath;
}

// File type
__attribute__((always_inline)) static inline const String *
storageWriteType(const StorageWrite *this)
{
    return THIS_PUB(StorageWrite)->interface->type;
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
storageWriteFree(StorageWrite *this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *storageWriteToLog(const StorageWrite *this);

#define FUNCTION_LOG_STORAGE_WRITE_TYPE                                                                                            \
    StorageWrite *
#define FUNCTION_LOG_STORAGE_WRITE_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, storageWriteToLog, buffer, bufferSize)

#endif
