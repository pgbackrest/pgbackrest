/***********************************************************************************************************************************
Sftp Storage Read
***********************************************************************************************************************************/
//#ifdef HAVE_LIBSSH2

#ifndef STORAGE_SFTP_READ_H
#define STORAGE_SFTP_READ_H

#include "storage/sftp/storage.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StorageRead *storageReadSftpNew(
    StorageSftp *storage, const String *name, bool ignoreMissing, uint64_t offset, const Variant *limit);

#endif

//#endif // HAVE_LIBSSH2
