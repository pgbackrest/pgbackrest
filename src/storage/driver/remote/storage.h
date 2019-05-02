/***********************************************************************************************************************************
Remote Storage Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_REMOTE_STORAGE_H
#define STORAGE_DRIVER_REMOTE_STORAGE_H

#include "protocol/client.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Driver type constant
***********************************************************************************************************************************/
#define STORAGE_DRIVER_REMOTE_TYPE                                      "remote"
    STRING_DECLARE(STORAGE_DRIVER_REMOTE_TYPE_STR);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
Storage *storageDriverRemoteNew(
    mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction, ProtocolClient *client);

#endif
