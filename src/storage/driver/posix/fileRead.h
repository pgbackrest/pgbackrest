/***********************************************************************************************************************************
Posix Storage File Read Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_FILEREAD_H
#define STORAGE_DRIVER_POSIX_FILEREAD_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverPosixFileRead StorageDriverPosixFileRead;

#include "common/type/buffer.h"
#include "common/type/string.h"
#include "storage/driver/posix/storage.h"
#include "storage/fileRead.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageDriverPosixFileRead *storageDriverPosixFileReadNew(const StorageDriverPosix *storage, const String *name, bool ignoreMissing);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool storageDriverPosixFileReadOpen(StorageDriverPosixFileRead *this);
size_t storageDriverPosixFileRead(StorageDriverPosixFileRead *this, Buffer *buffer);
void storageDriverPosixFileReadClose(StorageDriverPosixFileRead *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool storageDriverPosixFileReadEof(const StorageDriverPosixFileRead *this);
bool storageDriverPosixFileReadIgnoreMissing(const StorageDriverPosixFileRead *this);
StorageFileRead *storageDriverPosixFileReadInterface(const StorageDriverPosixFileRead *this);
IoRead *storageDriverPosixFileReadIo(const StorageDriverPosixFileRead *this);
const String *storageDriverPosixFileReadName(const StorageDriverPosixFileRead *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageDriverPosixFileReadFree(StorageDriverPosixFileRead *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_STORAGE_DRIVER_POSIX_FILE_READ_TYPE                                                                         \
    StorageDriverPosixFileRead *
#define FUNCTION_DEBUG_STORAGE_DRIVER_POSIX_FILE_READ_FORMAT(value, buffer, bufferSize)                                            \
    objToLog(value, "StorageDriverPosixFileRead", buffer, bufferSize)

#endif
