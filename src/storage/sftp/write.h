/***********************************************************************************************************************************
Sftp Storage File write
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_WRITE_H
#define STORAGE_SFTP_WRITE_H

#include "common/io/session.h"
#include "storage/sftp/storage.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StorageWrite *storageWriteSftpNew(
    StorageSftp *storage, const String *name, IoSession * ioSession, LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftpSession,
    LIBSSH2_SFTP_HANDLE *sftpHandle, const TimeMSec timeoutConnect, const TimeMSec timeoutSession, const mode_t modeFile,
    const mode_t modePath, const String *user, const String *group, const time_t timeModified, const bool createPath,
    const bool syncFile, const bool syncPath, const bool atomic);

#endif
