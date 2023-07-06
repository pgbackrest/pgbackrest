/***********************************************************************************************************************************
SFTP Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_STORAGE_INTERN_H
#define STORAGE_SFTP_STORAGE_INTERN_H

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "storage/sftp/storage.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageSftp StorageSftp;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
FN_EXTERN void storageSftpEvalLibSsh2Error(
    int ssh2Errno, uint64_t sftpErrno, const ErrorType *errorType, const String *msg, const String *hint);

// Check which direction we are currently blocking on (reading, writing, or both) and wait for the fd to be ready accordingly
FN_EXTERN bool storageSftpWaitFd(StorageSftp *this, int64_t rc);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_SFTP_TYPE                                                                                             \
    StorageSftp *
#define FUNCTION_LOG_STORAGE_SFTP_FORMAT(value, buffer, bufferSize)                                                                \
    objNameToLog(value, "StorageSftp *", buffer, bufferSize)

#endif
