/***********************************************************************************************************************************
Remote Storage Read Multi
***********************************************************************************************************************************/
#ifndef STORAGE_REMOTE_READ_MULTI_H
#define STORAGE_REMOTE_READ_MULTI_H

#include "protocol/client.h"
#include "storage/readMulti.h"
#include "storage/remote/storage.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageReadMultiRemote StorageReadMultiRemote;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageReadMultiRemote *storageReadMultiRemoteNew(
    StorageRemote *storage, ProtocolClient *client, unsigned int compressLevel);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_MULTI_REMOTE_TYPE                                                                                \
    StorageReadMultiRemote *
#define FUNCTION_LOG_STORAGE_READ_MULTI_REMOTE_FORMAT(value, buffer, bufferSize)                                                   \
    objNameToLog(value, "StorageReadMultiRemote", buffer, bufferSize)

#endif
