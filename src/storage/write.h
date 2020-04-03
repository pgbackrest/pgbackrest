/***********************************************************************************************************************************
Storage Write Interface
***********************************************************************************************************************************/
#ifndef STORAGE_WRITE_H
#define STORAGE_WRITE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define STORAGE_WRITE_TYPE                                          StorageWrite
#define STORAGE_WRITE_PREFIX                                        storageWrite

typedef struct StorageWrite StorageWrite;

#include "common/io/write.h"
#include "common/type/buffer.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to a new parent mem context
StorageWrite *storageWriteMove(StorageWrite *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
// Will the file be written atomically? Atomic writes means the file will be complete or be missing. Filesystems have different ways
// to accomplish this.
bool storageWriteAtomic(const StorageWrite *this);

// Will the path be created if required?
bool storageWriteCreatePath(const StorageWrite *this);

// Write interface
IoWrite *storageWriteIo(const StorageWrite *this);

// File mode
mode_t storageWriteModeFile(const StorageWrite *this);

// Path mode (if the destination path needs to be create)
mode_t storageWriteModePath(const StorageWrite *this);

// File name
const String *storageWriteName(const StorageWrite *this);

// Will the file be synced before it is closed?
bool storageWriteSyncFile(const StorageWrite *this);

// Will the path be synced after the file is closed?
bool storageWriteSyncPath(const StorageWrite *this);

// File type
const String *storageWriteType(const StorageWrite *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageWriteFree(StorageWrite *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *storageWriteToLog(const StorageWrite *this);

#define FUNCTION_LOG_STORAGE_WRITE_TYPE                                                                                            \
    StorageWrite *
#define FUNCTION_LOG_STORAGE_WRITE_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, storageWriteToLog, buffer, bufferSize)

#endif
