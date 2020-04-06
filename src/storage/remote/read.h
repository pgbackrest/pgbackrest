/***********************************************************************************************************************************
Remote Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_REMOTE_READ_H
#define STORAGE_REMOTE_READ_H

#include "protocol/client.h"
#include "storage/remote/storage.intern.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StorageRead *storageReadRemoteNew(
    StorageRemote *storage, ProtocolClient *client, const String *name, bool ignoreMissing, bool compressible,
    unsigned int compressLevel, const Variant *limit);

#endif
