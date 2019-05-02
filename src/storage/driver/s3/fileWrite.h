/***********************************************************************************************************************************
S3 Storage File Write Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_S3_FILEWRITE_H
#define STORAGE_DRIVER_S3_FILEWRITE_H

#include "storage/fileWrite.h"
#include "storage/driver/s3/storage.intern.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageFileWrite *storageFileWriteDriverS3New(StorageDriverS3 *storage, const String *name, size_t partSize);

#endif
