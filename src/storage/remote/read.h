/***********************************************************************************************************************************
Remote Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_REMOTE_READ_H
#define STORAGE_REMOTE_READ_H

#include "protocol/client.h"
#include "storage/read.h"
#include "storage/remote/storage.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageReadRemote StorageReadRemote;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageReadRemote *storageReadRemoteNew(
    StorageRemote *storage, ProtocolClient *client, const String *name, bool compressible, unsigned int compressLevel,
    uint64_t offset, const Variant *limit, const String *versionId);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_REMOTE_TYPE                                                                                      \
    StorageReadRemote *
#define FUNCTION_LOG_STORAGE_READ_REMOTE_FORMAT(value, buffer, bufferSize)                                                         \
    objNameToLog(value, "StorageReadRemote", buffer, bufferSize)

#endif
