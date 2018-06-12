/***********************************************************************************************************************************
Storage File Read Driver For Posix
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_DRIVERREAD_H
#define STORAGE_DRIVER_POSIX_DRIVERREAD_H

/***********************************************************************************************************************************
Read file object
***********************************************************************************************************************************/
typedef struct StorageFileReadPosix StorageFileReadPosix;

#include "common/type/buffer.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageFileReadPosix *storageFileReadPosixNew(const String *name, bool ignoreMissing, size_t bufferSize);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool storageFileReadPosixOpen(StorageFileReadPosix *this);
Buffer *storageFileReadPosix(StorageFileReadPosix *this);
void storageFileReadPosixClose(StorageFileReadPosix *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool storageFileReadPosixIgnoreMissing(StorageFileReadPosix *this);
const String *storageFileReadPosixName(StorageFileReadPosix *this);
size_t storageFileReadPosixSize(StorageFileReadPosix *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageFileReadPosixFree(StorageFileReadPosix *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_STORAGE_FILE_READ_POSIX_TYPE                                                                                \
    StorageFileReadPosix *
#define FUNCTION_DEBUG_STORAGE_FILE_READ_POSIX_FORMAT(value, buffer, bufferSize)                                                   \
    objToLog(value, "StorageFileReadPosix", buffer, bufferSize)

#endif
