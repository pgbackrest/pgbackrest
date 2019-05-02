/***********************************************************************************************************************************
Posix Storage File Write Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_FILEWRITE_H
#define STORAGE_DRIVER_POSIX_FILEWRITE_H

#include "storage/fileWrite.intern.h"
#include "storage/driver/posix/storage.h"
#include "storage/fileWrite.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageFileWrite *storageFileWriteDriverPosixNew(
    StorageDriverPosix *storage, const String *name, mode_t modeFile, mode_t modePath, const String *user, const String *group,
    time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic);

#endif
