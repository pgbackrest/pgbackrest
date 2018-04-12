/***********************************************************************************************************************************
Storage Posix Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_H
#define STORAGE_DRIVER_POSIX_H

#include <sys/types.h>

#include "common/type/buffer.h"
#include "common/type/string.h"
#include "storage/file.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Function
***********************************************************************************************************************************/
bool storageDriverPosixExists(const String *path);
Buffer *storageDriverPosixGet(const StorageFile *file);
StringList *storageDriverPosixList(const String *path, bool errorOnMissing, const String *expression);
void *storageDriverPosixOpenRead(const String *file, bool ignoreMissing);
void *storageDriverPosixOpenWrite(const String *file, mode_t mode);
void storageDriverPosixPathCreate(const String *path, bool errorOnExists, bool noParentCreate, mode_t mode);
void storageDriverPosixPathRemove(const String *path, bool errorOnMissing, bool recurse);
void storageDriverPosixPut(const StorageFile *file, const Buffer *buffer);
void storageDriverPosixRemove(const String *file, bool errorOnMissing);

#endif
