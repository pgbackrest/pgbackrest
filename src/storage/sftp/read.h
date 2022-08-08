/***********************************************************************************************************************************
Sftp Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_READ_H
#define STORAGE_SFTP_READ_H

#include "common/io/session.h"
#include "storage/sftp/storage.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StorageRead *storageReadSftpNew(
    StorageSftp *const storage, const String *const name, const bool ignoreMissing, IoSession *ioSession, LIBSSH2_SESSION *session,
    LIBSSH2_SFTP *sftpSession, LIBSSH2_SFTP_HANDLE *sftpHandle, TimeMSec timeoutSession, TimeMSec timeoutConnect,
    const uint64_t offset, const Variant *const limit);

#endif
