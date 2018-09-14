/***********************************************************************************************************************************
Posix Storage File Write Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_FILEWRITE_H
#define STORAGE_DRIVER_POSIX_FILEWRITE_H

#include <sys/types.h>

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverPosixFileWrite StorageDriverPosixFileWrite;

#include "common/type/buffer.h"
#include "storage/driver/posix/storage.h"
#include "storage/fileWrite.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageDriverPosixFileWrite *storageDriverPosixFileWriteNew(
    const StorageDriverPosix *storage, const String *name, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile,
    bool syncPath, bool atomic);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void storageDriverPosixFileWriteOpen(StorageDriverPosixFileWrite *this);
void storageDriverPosixFileWrite(StorageDriverPosixFileWrite *this, const Buffer *buffer);
void storageDriverPosixFileWriteClose(StorageDriverPosixFileWrite *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool storageDriverPosixFileWriteAtomic(const StorageDriverPosixFileWrite *this);
bool storageDriverPosixFileWriteCreatePath(const StorageDriverPosixFileWrite *this);
mode_t storageDriverPosixFileWriteModeFile(const StorageDriverPosixFileWrite *this);
StorageFileWrite* storageDriverPosixFileWriteInterface(const StorageDriverPosixFileWrite *this);
IoWrite *storageDriverPosixFileWriteIo(const StorageDriverPosixFileWrite *this);
mode_t storageDriverPosixFileWriteModePath(const StorageDriverPosixFileWrite *this);
const String *storageDriverPosixFileWriteName(const StorageDriverPosixFileWrite *this);
const StorageDriverPosix *storageDriverPosixFileWriteStorage(const StorageDriverPosixFileWrite *this);
bool storageDriverPosixFileWriteSyncFile(const StorageDriverPosixFileWrite *this);
bool storageDriverPosixFileWriteSyncPath(const StorageDriverPosixFileWrite *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageDriverPosixFileWriteFree(StorageDriverPosixFileWrite *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_STORAGE_DRIVER_POSIX_FILE_WRITE_TYPE                                                                        \
    StorageDriverPosixFileWrite *
#define FUNCTION_DEBUG_STORAGE_DRIVER_POSIX_FILE_WRITE_FORMAT(value, buffer, bufferSize)                                           \
    objToLog(value, "StorageDriverPosixFileWrite", buffer, bufferSize)

#endif
