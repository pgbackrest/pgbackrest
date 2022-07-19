/***********************************************************************************************************************************
Sftp Storage File write
***********************************************************************************************************************************/
//#ifdef HAVE_LIBSSH2

#ifndef STORAGE_SFTP_WRITE_H
#define STORAGE_SFTP_WRITE_H

#include "storage/sftp/storage.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StorageWrite *storageWriteSftpNew(
    StorageSftp *storage, const String *name, LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftpSession, LIBSSH2_SFTP_HANDLE *sftpHandle,
    LIBSSH2_SFTP_ATTRIBUTES *attr, const TimeMSec timeoutConnect, const TimeMSec timeoutSession, const mode_t modeFile,
    const mode_t modePath, const String *user, const String *group, const time_t timeModified, const bool createPath,
    const bool syncFile, const bool syncPath, const bool atomic);

#endif

//#endif // HAVE_LIBSSH2
