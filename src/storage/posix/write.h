/***********************************************************************************************************************************
Posix Storage File write
***********************************************************************************************************************************/
#ifndef STORAGE_POSIX_WRITE_H
#define STORAGE_POSIX_WRITE_H

#include "storage/posix/storage.intern.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWritePosix StorageWritePosix;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageWritePosix *storageWritePosixNew(
    StoragePosix *storage, const String *name, mode_t modeFile, mode_t modePath, const String *user, const String *group,
    time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic, bool truncate);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_POSIX_TYPE                                                                                      \
    StorageWritePosix *
#define FUNCTION_LOG_STORAGE_WRITE_POSIX_FORMAT(value, buffer, bufferSize)                                                         \
    objNameToLog(value, "StorageWritePosix", buffer, bufferSize)

#endif
