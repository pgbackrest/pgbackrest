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
    StorageSftp *storage, const String *name, mode_t modeFile, mode_t modePath, const String *user, const String *group,
    time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic);

#endif

//#endif // HAVE_LIBSSH2
