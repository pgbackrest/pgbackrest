/***********************************************************************************************************************************
Posix Storage File write
***********************************************************************************************************************************/
#ifndef STORAGE_POSIX_WRITE_H
#define STORAGE_POSIX_WRITE_H

#include "storage/posix/storage.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StorageWrite *storageWritePosixNew(
    StoragePosix *storage, const String *name, mode_t modeFile, mode_t modePath, const String *user, const String *group,
    time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic, bool truncate);

#endif
