/***********************************************************************************************************************************
Remote Storage Protocol Handler
***********************************************************************************************************************************/
#ifndef STORAGE_REMOTE_PROTOCOL_H
#define STORAGE_REMOTE_PROTOCOL_H

#include "common/type/pack.h"
#include "common/type/string.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_BLOCK_HEADER                                       "BRBLOCK"

#define PROTOCOL_COMMAND_STORAGE_FEATURE                            "storageFeature"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_FEATURE_STR);
#define PROTOCOL_COMMAND_STORAGE_INFO                               "storageInfo"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_INFO_STR);
#define PROTOCOL_COMMAND_STORAGE_INFO_LIST                          "storageInfoList"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_INFO_LIST_STR);
#define PROTOCOL_COMMAND_STORAGE_OPEN_READ                          "storageOpenRead"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR);
#define PROTOCOL_COMMAND_STORAGE_OPEN_WRITE                         "storageOpenWrite"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_OPEN_WRITE_STR);
#define PROTOCOL_COMMAND_STORAGE_PATH_CREATE                        "storagePathCreate"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_PATH_CREATE_STR);
#define PROTOCOL_COMMAND_STORAGE_REMOVE                             "storageRemove"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_REMOVE_STR);
#define PROTOCOL_COMMAND_STORAGE_PATH_REMOVE                        "storagePathRemove"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_PATH_REMOVE_STR);
#define PROTOCOL_COMMAND_STORAGE_PATH_SYNC                          "storagePathSync"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_PATH_SYNC_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get size of the next transfer block
ssize_t storageRemoteProtocolBlockSize(const String *message);

// Process storage protocol requests
bool storageRemoteProtocol(const String *command, PackRead *param, ProtocolServer *server);

#endif
