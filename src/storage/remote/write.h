/***********************************************************************************************************************************
Remote Storage File write
***********************************************************************************************************************************/
#ifndef STORAGE_REMOTE_WRITE_H
#define STORAGE_REMOTE_WRITE_H

#include "protocol/client.h"
#include "storage/remote/storage.intern.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteRemote StorageWriteRemote;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageWriteRemote *storageWriteRemoteNew(
    StorageRemote *storage, ProtocolClient *client, const String *name, mode_t modeFile, mode_t modePath, const String *user,
    const String *group, time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic, bool compressible,
    unsigned int compressLevel);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_REMOTE_TYPE                                                                                     \
    StorageWriteRemote *
#define FUNCTION_LOG_STORAGE_WRITE_REMOTE_FORMAT(value, buffer, bufferSize)                                                        \
    objNameToLog(value, "StorageWriteRemote", buffer, bufferSize)

#endif
