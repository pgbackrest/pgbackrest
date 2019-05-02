/***********************************************************************************************************************************
Remote Storage File Write Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_REMOTE_FILEWRITE_H
#define STORAGE_DRIVER_REMOTE_FILEWRITE_H

#include "protocol/client.h"
#include "storage/driver/remote/storage.intern.h"
#include "storage/fileWrite.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageFileWrite *storageFileWriteDriverRemoteNew(
    StorageDriverRemote *storage, ProtocolClient *client, const String *name, mode_t modeFile, mode_t modePath, const String *user,
    const String *group, time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic);

#endif
