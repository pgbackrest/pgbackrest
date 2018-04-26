/***********************************************************************************************************************************
Storage File Write
***********************************************************************************************************************************/
#ifndef STORAGE_FILEWRITE_H
#define STORAGE_FILEWRITE_H

#include <sys/types.h>

/***********************************************************************************************************************************
Storage file read object
***********************************************************************************************************************************/
typedef struct StorageFileWrite StorageFileWrite;

#include "common/type/buffer.h"
#include "common/type/string.h"
#include "storage/driver/posix/driverWrite.h"
#include "version.h"

/***********************************************************************************************************************************
Temporary file extension
***********************************************************************************************************************************/
#define STORAGE_FILE_TEMP_EXT                                       PGBACKREST_BIN ".tmp"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageFileWrite *storageFileWriteNew(
    const String *name, mode_t modeFile, mode_t modePath, bool noCreatePath, bool noSyncFile, bool noSyncPath, bool noAtomic);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void storageFileWriteOpen(StorageFileWrite *this);
void storageFileWrite(StorageFileWrite *this, const Buffer *buffer);
void storageFileWriteClose(StorageFileWrite *this);

StorageFileWrite *storageFileWriteMove(StorageFileWrite *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool storageFileWriteAtomic(const StorageFileWrite *this);
bool storageFileWriteCreatePath(const StorageFileWrite *this);
StorageFileWritePosix *storageFileWriteFileDriver(const StorageFileWrite *this);
mode_t storageFileWriteModeFile(const StorageFileWrite *this);
mode_t storageFileWriteModePath(const StorageFileWrite *this);
const String *storageFileWriteName(const StorageFileWrite *this);
const String *storageFileWritePath(const StorageFileWrite *this);
bool storageFileWriteSyncFile(const StorageFileWrite *this);
bool storageFileWriteSyncPath(const StorageFileWrite *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageFileWriteFree(const StorageFileWrite *this);

#endif
