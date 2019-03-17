/***********************************************************************************************************************************
S3 Storage File Write Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_S3_FILEWRITE_H
#define STORAGE_DRIVER_S3_FILEWRITE_H

#include <sys/types.h>

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverS3FileWrite StorageDriverS3FileWrite;

#include "common/type/buffer.h"
#include "storage/driver/s3/storage.h"
#include "storage/fileWrite.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageDriverS3FileWrite *storageDriverS3FileWriteNew(StorageDriverS3 *storage, const String *name, size_t partSize);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void storageDriverS3FileWriteOpen(StorageDriverS3FileWrite *this);
void storageDriverS3FileWrite(StorageDriverS3FileWrite *this, const Buffer *buffer);
void storageDriverS3FileWriteClose(StorageDriverS3FileWrite *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool storageDriverS3FileWriteAtomic(const StorageDriverS3FileWrite *this);
bool storageDriverS3FileWriteCreatePath(const StorageDriverS3FileWrite *this);
mode_t storageDriverS3FileWriteModeFile(const StorageDriverS3FileWrite *this);
StorageFileWrite* storageDriverS3FileWriteInterface(const StorageDriverS3FileWrite *this);
IoWrite *storageDriverS3FileWriteIo(const StorageDriverS3FileWrite *this);
mode_t storageDriverS3FileWriteModePath(const StorageDriverS3FileWrite *this);
const String *storageDriverS3FileWriteName(const StorageDriverS3FileWrite *this);
const StorageDriverS3 *storageDriverS3FileWriteStorage(const StorageDriverS3FileWrite *this);
bool storageDriverS3FileWriteSyncFile(const StorageDriverS3FileWrite *this);
bool storageDriverS3FileWriteSyncPath(const StorageDriverS3FileWrite *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageDriverS3FileWriteFree(StorageDriverS3FileWrite *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_DRIVER_S3_FILE_WRITE_TYPE                                                                             \
    StorageDriverS3FileWrite *
#define FUNCTION_LOG_STORAGE_DRIVER_S3_FILE_WRITE_FORMAT(value, buffer, bufferSize)                                                \
    objToLog(value, "StorageDriverS3FileWrite", buffer, bufferSize)

#endif
