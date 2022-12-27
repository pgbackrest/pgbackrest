/***********************************************************************************************************************************
SFTP Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_READ_H
#define STORAGE_SFTP_READ_H

#include "common/io/session.h"
#include "storage/read.h"
#include "storage/sftp/storage.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StorageRead *storageReadSftpNew(
    StorageSftp *storage, const String *name, bool ignoreMissing, IoSession *ioSession, LIBSSH2_SESSION *session,
    LIBSSH2_SFTP *sftpSession, LIBSSH2_SFTP_HANDLE *sftpHandle, TimeMSec timeoutSession, TimeMSec timeoutConnect,
    uint64_t offset, const Variant *limit);

#endif
