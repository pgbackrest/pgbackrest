/***********************************************************************************************************************************
Remote Storage File Read Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_REMOTE_FILEREAD_H
#define STORAGE_DRIVER_REMOTE_FILEREAD_H

#include "protocol/client.h"
#include "storage/driver/remote/storage.intern.h"
#include "storage/fileRead.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageFileRead *storageFileReadDriverRemoteNew(
    StorageDriverRemote *storage, ProtocolClient *client, const String *name, bool ignoreMissing);

#endif
