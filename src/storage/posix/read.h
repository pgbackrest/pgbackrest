/***********************************************************************************************************************************
Posix Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_POSIX_READ_H
#define STORAGE_POSIX_READ_H

#include "storage/posix/storage.intern.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageRead *storageReadPosixNew(
    StoragePosix *storage, const String *name, bool ignoreMissing, const StorageRangeList *rangeList);

#endif
