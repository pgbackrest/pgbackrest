/***********************************************************************************************************************************
Storage Driver Posix
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_DRIVER_H
#define STORAGE_DRIVER_POSIX_DRIVER_H

#include <sys/types.h>

#include "common/type/buffer.h"
#include "common/type/stringList.h"
#include "storage/driver/posix/driverRead.h"
#include "storage/driver/posix/driverWrite.h"
#include "storage/info.h"

/***********************************************************************************************************************************
Function
***********************************************************************************************************************************/
bool storageDriverPosixExists(const String *path);
StorageInfo storageDriverPosixInfo(const String *file, bool ignoreMissing);
StringList *storageDriverPosixList(const String *path, bool errorOnMissing, const String *expression);
bool storageDriverPosixMove(StorageFileReadPosix *source, StorageFileWritePosix *destination);
void storageDriverPosixPathCreate(const String *path, bool errorOnExists, bool noParentCreate, mode_t mode);
void storageDriverPosixPathRemove(const String *path, bool errorOnMissing, bool recurse);
void storageDriverPosixPathSync(const String *path, bool ignoreMissing);
void storageDriverPosixRemove(const String *file, bool errorOnMissing);

#endif
