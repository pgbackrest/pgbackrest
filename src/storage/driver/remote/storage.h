/***********************************************************************************************************************************
Remote Storage Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_REMOTE_STORAGE_H
#define STORAGE_DRIVER_REMOTE_STORAGE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverRemote StorageDriverRemote;

#include "common/io/http/client.h"
#include "common/type/string.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Driver type constant
***********************************************************************************************************************************/
#define STORAGE_DRIVER_REMOTE_TYPE                                      "remote"
    STRING_DECLARE(STORAGE_DRIVER_REMOTE_TYPE_STR);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageDriverRemote *storageDriverRemoteNew(
    mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction, ProtocolClient *client);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool storageDriverRemoteExists(StorageDriverRemote *this, const String *path);
StorageInfo storageDriverRemoteInfo(StorageDriverRemote *this, const String *file, bool ignoreMissing, bool followLink);
StringList *storageDriverRemoteList(StorageDriverRemote *this, const String *path, bool errorOnMissing, const String *expression);
StorageFileRead *storageDriverRemoteNewRead(StorageDriverRemote *this, const String *file, bool ignoreMissing);
StorageFileWrite *storageDriverRemoteNewWrite(
    StorageDriverRemote *this, const String *file, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile, bool syncPath,
    bool atomic);
void storageDriverRemotePathCreate(
    StorageDriverRemote *this, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode);
void storageDriverRemotePathRemove(StorageDriverRemote *this, const String *path, bool errorOnMissing, bool recurse);
void storageDriverRemotePathSync(StorageDriverRemote *this, const String *path, bool ignoreMissing);
void storageDriverRemoteRemove(StorageDriverRemote *this, const String *file, bool errorOnMissing);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
Storage *storageDriverRemoteInterface(const StorageDriverRemote *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageDriverRemoteFree(StorageDriverRemote *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_DRIVER_REMOTE_TYPE                                                                                    \
    StorageDriverRemote *
#define FUNCTION_LOG_STORAGE_DRIVER_REMOTE_FORMAT(value, buffer, bufferSize)                                                       \
    objToLog(value, "StorageDriverRemote", buffer, bufferSize)

#endif
