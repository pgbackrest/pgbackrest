/***********************************************************************************************************************************
Posix Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_POSIX_READ_H
#define STORAGE_POSIX_READ_H

#include "common/type/primitive.h"
#include "storage/posix/storage.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageRead *storageReadPosixNew(StoragePosix *storage, const String *name, bool ignoreMissing, const PrmUInt64 *limit);

#endif
