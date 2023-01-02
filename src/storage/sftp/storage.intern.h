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
Constructors
***********************************************************************************************************************************/
FN_EXTERN Storage *storageSftpNewInternal(
    StringId type, const String *path, const String *host, unsigned int port, TimeMSec timeoutConnect, TimeMSec timeoutSession,
    const String *user, const String *keyPub, const String *keyPriv, const String *keyPassphrase, const StringId hostkeyHash,
    mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction, bool pathSync);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
FN_EXTERN void storageSftpEvalLibSsh2Error(
    int ssh2Errno, uint64_t sftpErrno, const ErrorType *errorType, const String *msg, const String *hint);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_SFTP_TYPE                                                                                             \
    StorageSftp *
#define FUNCTION_LOG_STORAGE_SFTP_FORMAT(value, buffer, bufferSize)                                                                \
    objToLog(value, "StorageSftp *", buffer, bufferSize)

#endif
