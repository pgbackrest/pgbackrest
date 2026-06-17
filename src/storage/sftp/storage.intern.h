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

// Current libssh2 session/sftp session, used by the read/write objects so a reopened session is picked up automatically
FN_EXTERN LIBSSH2_SESSION *storageSftpSession(StorageSftp *this);
FN_EXTERN LIBSSH2_SFTP *storageSftpSessionSftp(StorageSftp *this);

// Is the libssh2 error a sign that the connection to the server has been lost?
FN_EXTERN bool storageSftpConnLost(int rc);

// If the error indicates the connection was lost then reopen the session so the operation can be retried. Returns true when the
// session was reopened (the caller should retry the operation).
FN_EXTERN bool storageSftpReconnect(StorageSftp *this, int rc);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_SFTP_TYPE                                                                                             \
    StorageSftp *
#define FUNCTION_LOG_STORAGE_SFTP_FORMAT(value, buffer, bufferSize)                                                                \
    objNameToLog(value, "StorageSftp *", buffer, bufferSize)

#endif
