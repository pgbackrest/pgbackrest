/***********************************************************************************************************************************
Sftp Storage
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_STORAGE_H
#define STORAGE_SFTP_STORAGE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageSftp StorageSftp;

#include "storage/storage.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
//jrt generate strid5 #define STORAGE_SFTP_TYPE                                          STRID5("posix", 0x184cdf00)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct StorageSftpNewParam
{
    VAR_PARAM_HEADER;
    bool write;
    mode_t modeFile;
    mode_t modePath;
    StoragePathExpressionCallback *pathExpressionFunction;
} StorageSftpNewParam;

#define storageSftpNewP(path, ...)                                                                                                 \
    storageSftpNew(path, (StorageSftpNewParam){VAR_PARAM_INIT, __VA_ARGS__})

Storage *storageSftpNew(const String *path, StorageSftpNewParam param);

#endif
