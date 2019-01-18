/***********************************************************************************************************************************
Remote Storage File Read Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_REMOTE_FILEREAD_H
#define STORAGE_DRIVER_REMOTE_FILEREAD_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverRemoteFileRead StorageDriverRemoteFileRead;

#include "common/type/buffer.h"
#include "common/type/string.h"
#include "protocol/client.h"
#include "storage/driver/remote/storage.h"
#include "storage/fileRead.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageDriverRemoteFileRead *storageDriverRemoteFileReadNew(
    StorageDriverRemote *storage, ProtocolClient *client, const String *name, bool ignoreMissing);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool storageDriverRemoteFileReadOpen(StorageDriverRemoteFileRead *this);
size_t storageDriverRemoteFileRead(StorageDriverRemoteFileRead *this, Buffer *buffer, bool block);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool storageDriverRemoteFileReadEof(const StorageDriverRemoteFileRead *this);
bool storageDriverRemoteFileReadIgnoreMissing(const StorageDriverRemoteFileRead *this);
StorageFileRead *storageDriverRemoteFileReadInterface(const StorageDriverRemoteFileRead *this);
IoRead *storageDriverRemoteFileReadIo(const StorageDriverRemoteFileRead *this);
const String *storageDriverRemoteFileReadName(const StorageDriverRemoteFileRead *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageDriverRemoteFileReadFree(StorageDriverRemoteFileRead *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_STORAGE_DRIVER_REMOTE_FILE_READ_TYPE                                                                        \
    StorageDriverRemoteFileRead *
#define FUNCTION_DEBUG_STORAGE_DRIVER_REMOTE_FILE_READ_FORMAT(value, buffer, bufferSize)                                           \
    objToLog(value, "StorageDriverRemoteFileRead", buffer, bufferSize)

#endif
