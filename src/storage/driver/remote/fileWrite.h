/***********************************************************************************************************************************
Remote Storage File Write Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_REMOTE_FILEWRITE_H
#define STORAGE_DRIVER_REMOTE_FILEWRITE_H

#include <sys/types.h>

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverRemoteFileWrite StorageDriverRemoteFileWrite;

#include "common/type/buffer.h"
#include "protocol/client.h"
#include "storage/driver/remote/storage.h"
#include "storage/fileWrite.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageDriverRemoteFileWrite *storageDriverRemoteFileWriteNew(
    StorageDriverRemote *storage, ProtocolClient *client, const String *name, mode_t modeFile, mode_t modePath, const String *user,
    const String *group, time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void storageDriverRemoteFileWriteOpen(StorageDriverRemoteFileWrite *this);
void storageDriverRemoteFileWrite(StorageDriverRemoteFileWrite *this, const Buffer *buffer);
void storageDriverRemoteFileWriteClose(StorageDriverRemoteFileWrite *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool storageDriverRemoteFileWriteAtomic(const StorageDriverRemoteFileWrite *this);
bool storageDriverRemoteFileWriteCreatePath(const StorageDriverRemoteFileWrite *this);
mode_t storageDriverRemoteFileWriteModeFile(const StorageDriverRemoteFileWrite *this);
StorageFileWrite* storageDriverRemoteFileWriteInterface(const StorageDriverRemoteFileWrite *this);
IoWrite *storageDriverRemoteFileWriteIo(const StorageDriverRemoteFileWrite *this);
mode_t storageDriverRemoteFileWriteModePath(const StorageDriverRemoteFileWrite *this);
const String *storageDriverRemoteFileWriteName(const StorageDriverRemoteFileWrite *this);
const StorageDriverRemote *storageDriverRemoteFileWriteStorage(const StorageDriverRemoteFileWrite *this);
bool storageDriverRemoteFileWriteSyncFile(const StorageDriverRemoteFileWrite *this);
bool storageDriverRemoteFileWriteSyncPath(const StorageDriverRemoteFileWrite *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageDriverRemoteFileWriteFree(StorageDriverRemoteFileWrite *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_DRIVER_REMOTE_FILE_WRITE_TYPE                                                                         \
    StorageDriverRemoteFileWrite *
#define FUNCTION_LOG_STORAGE_DRIVER_REMOTE_FILE_WRITE_FORMAT(value, buffer, bufferSize)                                            \
    objToLog(value, "StorageDriverRemoteFileWrite", buffer, bufferSize)

#endif
