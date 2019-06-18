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
StorageWrite *storageWriteMove(StorageWrite *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool storageWriteAtomic(const StorageWrite *this);
bool storageWriteCreatePath(const StorageWrite *this);
IoWrite *storageWriteIo(const StorageWrite *this);
mode_t storageWriteModeFile(const StorageWrite *this);
mode_t storageWriteModePath(const StorageWrite *this);
const String *storageWriteName(const StorageWrite *this);
bool storageWriteSyncFile(const StorageWrite *this);
bool storageWriteSyncPath(const StorageWrite *this);
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
