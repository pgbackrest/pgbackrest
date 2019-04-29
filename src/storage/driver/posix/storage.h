/***********************************************************************************************************************************
Posix Storage Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_STORAGE_H
#define STORAGE_DRIVER_POSIX_STORAGE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverPosix StorageDriverPosix;

#include <sys/types.h>

#include "common/type/buffer.h"
#include "common/type/stringList.h"
#include "storage/driver/posix/fileRead.h"
#include "storage/driver/posix/fileWrite.h"
#include "storage/info.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Driver type constant
***********************************************************************************************************************************/
#define STORAGE_DRIVER_POSIX_TYPE                                   "posix"
    STRING_DECLARE(STORAGE_DRIVER_POSIX_TYPE_STR);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageDriverPosix *storageDriverPosixNew(
    const String *path, mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool storageDriverPosixExists(StorageDriverPosix *this, const String *path);
StorageInfo storageDriverPosixInfo(StorageDriverPosix *this, const String *file, bool ignoreMissing, bool followLink);
bool storageDriverPosixInfoList(
    StorageDriverPosix *this, const String *path, bool errorOnMissing, StorageInfoListCallback callback, void *callbackData);
StringList *storageDriverPosixList(StorageDriverPosix *this, const String *path, bool errorOnMissing, const String *expression);
bool storageDriverPosixMove(StorageDriverPosix *this, StorageDriverPosixFileRead *source, StorageDriverPosixFileWrite *destination);
StorageFileRead *storageDriverPosixNewRead(StorageDriverPosix *this, const String *file, bool ignoreMissing);
StorageFileWrite *storageDriverPosixNewWrite(
    StorageDriverPosix *this, const String *file, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile, bool syncPath,
    bool atomic);
void storageDriverPosixPathCreate(
    StorageDriverPosix *this, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode);
void storageDriverPosixPathRemove(StorageDriverPosix *this, const String *path, bool errorOnMissing, bool recurse);
void storageDriverPosixPathSync(StorageDriverPosix *this, const String *path, bool ignoreMissing);
void storageDriverPosixRemove(StorageDriverPosix *this, const String *file, bool errorOnMissing);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
Storage *storageDriverPosixInterface(const StorageDriverPosix *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageDriverPosixFree(StorageDriverPosix *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_DRIVER_POSIX_TYPE                                                                                     \
    StorageDriverPosix *
#define FUNCTION_LOG_STORAGE_DRIVER_POSIX_FORMAT(value, buffer, bufferSize)                                                        \
    objToLog(value, "StorageDriverPosix", buffer, bufferSize)

#endif
