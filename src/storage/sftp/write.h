/***********************************************************************************************************************************
SFTP Storage File Write
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_WRITE_H
#define STORAGE_SFTP_WRITE_H

#include "common/io/session.h"
#include "storage/sftp/storage.h"
#include "storage/sftp/storage.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteSftp StorageWriteSftp;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageWriteSftp *storageWriteSftpNew(
    StorageSftp *storage, const String *name, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile, bool atomic);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_SFTP_TYPE                                                                                       \
    StorageWriteSftp *
#define FUNCTION_LOG_STORAGE_WRITE_SFTP_FORMAT(value, buffer, bufferSize)                                                          \
    objNameToLog(value, "StorageWriteSftp", buffer, bufferSize)

#endif
