/***********************************************************************************************************************************
Storage File Write Interface
***********************************************************************************************************************************/
#ifndef STORAGE_FILEWRITE_H
#define STORAGE_FILEWRITE_H

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct StorageFileWrite StorageFileWrite;

#include "common/io/write.h"
#include "common/type/buffer.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
StorageFileWrite *storageFileWriteMove(StorageFileWrite *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool storageFileWriteAtomic(const StorageFileWrite *this);
bool storageFileWriteCreatePath(const StorageFileWrite *this);
IoWrite *storageFileWriteIo(const StorageFileWrite *this);
mode_t storageFileWriteModeFile(const StorageFileWrite *this);
mode_t storageFileWriteModePath(const StorageFileWrite *this);
const String *storageFileWriteName(const StorageFileWrite *this);
bool storageFileWriteSyncFile(const StorageFileWrite *this);
bool storageFileWriteSyncPath(const StorageFileWrite *this);
const String *storageFileWriteType(const StorageFileWrite *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageFileWriteFree(const StorageFileWrite *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *storageFileWriteToLog(const StorageFileWrite *this);

#define FUNCTION_LOG_STORAGE_FILE_WRITE_TYPE                                                                                       \
    StorageFileWrite *
#define FUNCTION_LOG_STORAGE_FILE_WRITE_FORMAT(value, buffer, bufferSize)                                                          \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, storageFileWriteToLog, buffer, bufferSize)

#endif
