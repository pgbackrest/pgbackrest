/***********************************************************************************************************************************
Storage File Read Interface
***********************************************************************************************************************************/
#ifndef STORAGE_FILEREAD_H
#define STORAGE_FILEREAD_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageFileRead StorageFileRead;

#include "common/io/read.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
StorageFileRead *storageFileReadMove(StorageFileRead *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
void *storageFileReadDriver(const StorageFileRead *this);
IoRead *storageFileReadIo(const StorageFileRead *this);
bool storageFileReadIgnoreMissing(const StorageFileRead *this);
const String *storageFileReadName(const StorageFileRead *this);
const String *storageFileReadType(const StorageFileRead *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageFileReadFree(const StorageFileRead *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *storageFileReadToLog(const StorageFileRead *this);

#define FUNCTION_DEBUG_STORAGE_FILE_READ_TYPE                                                                                      \
    StorageFileRead *
#define FUNCTION_DEBUG_STORAGE_FILE_READ_FORMAT(value, buffer, bufferSize)                                                         \
    FUNCTION_DEBUG_STRING_OBJECT_FORMAT(value, storageFileReadToLog, buffer, bufferSize)

#endif
