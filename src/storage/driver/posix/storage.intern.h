/***********************************************************************************************************************************
Posix Storage Driver Internal
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_POSIX_STORAGE_INTERN_H
#define STORAGE_DRIVER_POSIX_STORAGE_INTERN_H

#include "common/object.h"
#include "storage/driver/posix/storage.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
Storage *storageDriverPosixNewInternal(
    const String *type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, bool pathSync);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void storageDriverPosixPathCreate(THIS_VOID, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode);
void storageDriverPosixPathSync(THIS_VOID, const String *path, bool ignoreMissing);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_DRIVER_POSIX_TYPE                                                                                     \
    StorageDriverPosix *
#define FUNCTION_LOG_STORAGE_DRIVER_POSIX_FORMAT(value, buffer, bufferSize)                                                        \
    objToLog(value, "StorageDriverPosix *", buffer, bufferSize)

#endif
