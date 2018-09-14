/***********************************************************************************************************************************
Storage File Read Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_FILEREAD_INTERN_H
#define STORAGE_FILEREAD_INTERN_H

#include "storage/fileRead.h"

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef bool (*StorageFileReadIgnoreMissing)(const void *driver);
typedef IoRead *(*StorageFileReadIo)(const void *driver);
typedef const String *(*StorageFileReadName)(const void *driver);

typedef struct StorageFileReadInterface
{
    StorageFileReadIgnoreMissing ignoreMissing;
    StorageFileReadIo io;
    StorageFileReadName name;
} StorageFileReadInterface;

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageFileRead *storageFileReadNew(const String *type, void *driver, const StorageFileReadInterface *interface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_STORAGE_FILE_READ_INTERFACE_TYPE                                                                            \
    StorageFileReadInterface *
#define FUNCTION_DEBUG_STORAGE_FILE_READ_INTERFACE_FORMAT(value, buffer, bufferSize)                                               \
    objToLog(value, "StorageFileReadInterface", buffer, bufferSize)

#endif
