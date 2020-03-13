/***********************************************************************************************************************************
Remote Storage File write
***********************************************************************************************************************************/
#ifndef STORAGE_REMOTE_WRITE_H
#define STORAGE_REMOTE_WRITE_H

#include "protocol/client.h"
#include "storage/remote/storage.intern.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageWrite *storageWriteRemoteNew(
    StorageRemote *storage, ProtocolClient *client, const String *name, mode_t modeFile, mode_t modePath, const String *user,
    const String *group, time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic, bool compressible,
    int compressLevel);

#endif
