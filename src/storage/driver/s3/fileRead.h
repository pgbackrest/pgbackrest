/***********************************************************************************************************************************
S3 Storage File Read Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_S3_FILEREAD_H
#define STORAGE_DRIVER_S3_FILEREAD_H

#include "storage/driver/s3/storage.intern.h"
#include "storage/fileRead.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageFileRead *storageFileReadDriverS3New(StorageDriverS3 *storage, const String *name, bool ignoreMissing);

#endif
