/***********************************************************************************************************************************
S3 Storage File Read Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_S3_FILEREAD_H
#define STORAGE_DRIVER_S3_FILEREAD_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverS3FileRead StorageDriverS3FileRead;

#include "common/type/buffer.h"
#include "common/type/string.h"
#include "storage/driver/s3/storage.h"
#include "storage/fileRead.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageDriverS3FileRead *storageDriverS3FileReadNew(StorageDriverS3 *storage, const String *name, bool ignoreMissing);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool storageDriverS3FileReadOpen(StorageDriverS3FileRead *this);
size_t storageDriverS3FileRead(StorageDriverS3FileRead *this, Buffer *buffer, bool block);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool storageDriverS3FileReadEof(const StorageDriverS3FileRead *this);
bool storageDriverS3FileReadIgnoreMissing(const StorageDriverS3FileRead *this);
StorageFileRead *storageDriverS3FileReadInterface(const StorageDriverS3FileRead *this);
IoRead *storageDriverS3FileReadIo(const StorageDriverS3FileRead *this);
const String *storageDriverS3FileReadName(const StorageDriverS3FileRead *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageDriverS3FileReadFree(StorageDriverS3FileRead *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_DRIVER_S3_FILE_READ_TYPE                                                                              \
    StorageDriverS3FileRead *
#define FUNCTION_LOG_STORAGE_DRIVER_S3_FILE_READ_FORMAT(value, buffer, bufferSize)                                                 \
    objToLog(value, "StorageDriverS3FileRead", buffer, bufferSize)

#endif
