/***********************************************************************************************************************************
Posix Storage
***********************************************************************************************************************************/
#ifndef STORAGE_POSIX_STORAGE_H
#define STORAGE_POSIX_STORAGE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StoragePosix StoragePosix;

#include "storage/storage.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
#define STORAGE_POSIX_TYPE                                          STRID6("posix", 0x182533d01)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct StoragePosixNewParam
{
    VAR_PARAM_HEADER;
    bool write;
    mode_t modeFile;
    mode_t modePath;
    StoragePathExpressionCallback *pathExpressionFunction;
} StoragePosixNewParam;

#define storagePosixNewP(path, ...)                                                                                                \
    storagePosixNew(path, (StoragePosixNewParam){VAR_PARAM_INIT, __VA_ARGS__})

Storage *storagePosixNew(const String *path, StoragePosixNewParam param);

#endif
