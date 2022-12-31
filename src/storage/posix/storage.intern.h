/***********************************************************************************************************************************
Posix Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_POSIX_STORAGE_INTERN_H
#define STORAGE_POSIX_STORAGE_INTERN_H

#include "common/type/object.h"
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FV_EXTERN Storage *storagePosixNewInternal(
    StringId type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, bool pathSync);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_POSIX_TYPE                                                                                            \
    StoragePosix *
#define FUNCTION_LOG_STORAGE_POSIX_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "StoragePosix *", buffer, bufferSize)

#endif
