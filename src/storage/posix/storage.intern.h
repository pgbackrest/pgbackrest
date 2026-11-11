/***********************************************************************************************************************************
Posix Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_POSIX_STORAGE_INTERN_H
#define STORAGE_POSIX_STORAGE_INTERN_H

#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Direct I/O alignment of address, size, and offset
***********************************************************************************************************************************/
#define STORAGE_POSIX_DIRECT_ALIGN                                  4096

// Round size up to the direct I/O alignment
#define STORAGE_POSIX_DIRECT_ALIGN_UP(size)                                                                                        \
    (((size) + STORAGE_POSIX_DIRECT_ALIGN - 1) / STORAGE_POSIX_DIRECT_ALIGN * STORAGE_POSIX_DIRECT_ALIGN)

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StoragePosix StoragePosix;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN Storage *storagePosixNewInternal(
    StringId type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, bool pathSync, bool symLink, StoragePosixIoMode ioMode);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_POSIX_TYPE                                                                                            \
    StoragePosix *
#define FUNCTION_LOG_STORAGE_POSIX_FORMAT(value, buffer, bufferSize)                                                               \
    objNameToLog(value, "StoragePosix *", buffer, bufferSize)

#endif
