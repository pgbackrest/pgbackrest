/***********************************************************************************************************************************
Storage File Read Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_FILEREAD_INTERN_H
#define STORAGE_FILEREAD_INTERN_H

#include "storage/fileRead.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef bool (*StorageFileReadInterfaceIgnoreMissing)(const void *driver);
typedef IoRead *(*StorageFileReadInterfaceIo)(const void *driver);
typedef const String *(*StorageFileReadInterfaceName)(const void *driver);

typedef struct StorageFileReadInterface
{
    StorageFileReadInterfaceIgnoreMissing ignoreMissing;
    StorageFileReadInterfaceIo io;
    StorageFileReadInterfaceName name;
} StorageFileReadInterface;

#define storageFileReadNewP(type, driver, ...)                                                                                     \
    storageFileReadNew(type, driver, (StorageFileReadInterface){__VA_ARGS__})

StorageFileRead *storageFileReadNew(const String *type, void *driver, StorageFileReadInterface interface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_FILE_READ_INTERFACE_TYPE                                                                              \
    StorageFileReadInterface
#define FUNCTION_LOG_STORAGE_FILE_READ_INTERFACE_FORMAT(value, buffer, bufferSize)                                                 \
    objToLog(&value, "StorageFileReadInterface", buffer, bufferSize)

#endif
