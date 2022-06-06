/***********************************************************************************************************************************
Sftp Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_STORAGE_INTERN_H
#define STORAGE_SFTP_STORAGE_INTERN_H

#include "common/type/object.h"
#include "storage/sftp/storage.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Storage *storageSftpNewInternal(
    StringId type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, bool pathSync);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void storageSftpPathCreate(
    THIS_VOID, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode, StorageInterfacePathCreateParam param);
void storageSftpPathSync(THIS_VOID, const String *path, StorageInterfacePathSyncParam param);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_SFTP_TYPE                                                                                            \
    StorageSftp *
#define FUNCTION_LOG_STORAGE_SFTP_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "StorageSftp *", buffer, bufferSize)

#endif