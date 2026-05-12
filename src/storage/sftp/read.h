/***********************************************************************************************************************************
SFTP Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_READ_H
#define STORAGE_SFTP_READ_H

#include "storage/read.h"
#include "storage/sftp/storage.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageReadSftp StorageReadSftp;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageReadSftp *storageReadSftpNew(
    StorageSftp *storage, const String *name, LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftpSession, LIBSSH2_SFTP_HANDLE *sftpHandle,
    uint64_t offset, const Variant *limit);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_SFTP_TYPE                                                                                        \
    StorageReadSftp *
#define FUNCTION_LOG_STORAGE_READ_SFTP_FORMAT(value, buffer, bufferSize)                                                           \
    objNameToLog(value, "StorageReadSftp", buffer, bufferSize)

#endif
