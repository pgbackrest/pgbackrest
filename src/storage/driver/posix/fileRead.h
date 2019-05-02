/***********************************************************************************************************************************
Posix Storage File Read Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_FILEREAD_H
#define STORAGE_DRIVER_POSIX_FILEREAD_H

#include "storage/driver/posix/storage.h"
#include "storage/fileRead.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageFileRead *storageFileReadDriverPosixNew(StorageDriverPosix *storage, const String *name, bool ignoreMissing);

#endif
