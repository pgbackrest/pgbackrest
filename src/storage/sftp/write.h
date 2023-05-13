/***********************************************************************************************************************************
SFTP Storage File Write
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_WRITE_H
#define STORAGE_SFTP_WRITE_H

#include "common/io/session.h"
#include "storage/sftp/storage.h"
#include "storage/sftp/storage.intern.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageWrite *storageWriteSftpNew(
    StorageSftp *storage, const String *name, IoSession *ioSession, LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftpSession,
    LIBSSH2_SFTP_HANDLE *sftpHandle, TimeMSec timeout, mode_t modeFile, mode_t modePath, const String *user, const String *group,
    time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic, bool truncate);

#endif
