/***********************************************************************************************************************************
Storage File Write Driver For Posix
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_DRIVERWRITE_H
#define STORAGE_DRIVER_POSIX_DRIVERWRITE_H

#include <sys/types.h>

/***********************************************************************************************************************************
Write file object
***********************************************************************************************************************************/
typedef struct StorageFileWritePosix StorageFileWritePosix;

#include "common/type/buffer.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageFileWritePosix *storageFileWritePosixNew(
    const String *name, mode_t modeFile, mode_t modePath, bool noCreatePath, bool noSyncFile, bool noSyncPath, bool noAtomic);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void storageFileWritePosixOpen(StorageFileWritePosix *this);
void storageFileWritePosix(StorageFileWritePosix *this, const Buffer *buffer);
void storageFileWritePosixClose(StorageFileWritePosix *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool storageFileWritePosixAtomic(StorageFileWritePosix *this);
bool storageFileWritePosixCreatePath(StorageFileWritePosix *this);
mode_t storageFileWritePosixModeFile(StorageFileWritePosix *this);
mode_t storageFileWritePosixModePath(StorageFileWritePosix *this);
const String *storageFileWritePosixName(StorageFileWritePosix *this);
const String *storageFileWritePosixPath(StorageFileWritePosix *this);
bool storageFileWritePosixSyncFile(StorageFileWritePosix *this);
bool storageFileWritePosixSyncPath(StorageFileWritePosix *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageFileWritePosixFree(StorageFileWritePosix *this);

#endif
