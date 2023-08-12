/***********************************************************************************************************************************
SFTP Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_SFTP_READ_H
#define STORAGE_SFTP_READ_H

#include "storage/read.h"
#include "storage/sftp/storage.intern.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageRead *storageReadSftpNew(
    StorageSftp *storage, const String *name, bool ignoreMissing, LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftpSession,
    LIBSSH2_SFTP_HANDLE *sftpHandle, uint64_t offset, const Variant *limit);

#endif
