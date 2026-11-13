/***********************************************************************************************************************************
Posix Storage
***********************************************************************************************************************************/
#ifndef STORAGE_POSIX_STORAGE_H
#define STORAGE_POSIX_STORAGE_H

#include "storage/storage.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
#define STORAGE_POSIX_TYPE                                          STRID5("posix", 0x184cdf00)

/***********************************************************************************************************************************
I/O mode
***********************************************************************************************************************************/
typedef enum
{
    storagePosixIoModeDefault,
    storagePosixIoModeDirect,
} StoragePosixIoMode;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct StoragePosixNewParam
{
    VAR_PARAM_HEADER;
    bool write;
    bool noSymLink;                                                 // Do not create symlinks on this storage
    StoragePosixIoMode ioMode;                                      // I/O mode
    mode_t modeFile;
    mode_t modePath;
    StoragePathExpressionCallback *pathExpressionFunction;
} StoragePosixNewParam;

#define storagePosixNewP(path, ...)                                                                                                \
    storagePosixNew(path, (StoragePosixNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN Storage *storagePosixNew(const String *path, StoragePosixNewParam param);

#endif
