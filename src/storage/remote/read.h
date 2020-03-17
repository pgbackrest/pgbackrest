/***********************************************************************************************************************************
Remote Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_REMOTE_READ_H
#define STORAGE_REMOTE_READ_H

#include "common/type/primitive.h"
#include "protocol/client.h"
#include "storage/remote/storage.intern.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageRead *storageReadRemoteNew(
    StorageRemote *storage, ProtocolClient *client, const String *name, bool ignoreMissing, bool compressible,
    unsigned int compressLevel, const PrmUInt64 *limit);

#endif
