/***********************************************************************************************************************************
Remote Storage Protocol Handler
***********************************************************************************************************************************/
#ifndef STORAGE_REMOTE_PROTOCOL_H
#define STORAGE_REMOTE_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/variantList.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_BLOCK_HEADER                                       "BRBLOCK"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get size of the next transfer block
ssize_t storageRemoteProtocolBlockSize(const String *message);

// Process storage protocol requests
void storageRemoteFeatureProtocol(const VariantList *paramList, ProtocolServer *server);
void storageRemoteInfoProtocol(const VariantList *paramList, ProtocolServer *server);
void storageRemoteInfoListProtocol(const VariantList *paramList, ProtocolServer *server);
void storageRemoteOpenReadProtocol(const VariantList *paramList, ProtocolServer *server);
void storageRemoteOpenWriteProtocol(const VariantList *paramList, ProtocolServer *server);
void storageRemotePathCreateProtocol(const VariantList *paramList, ProtocolServer *server);
void storageRemotePathRemoveProtocol(const VariantList *paramList, ProtocolServer *server);
void storageRemotePathSyncProtocol(const VariantList *paramList, ProtocolServer *server);
void storageRemoteRemoveProtocol(const VariantList *paramList, ProtocolServer *server);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_STORAGE_FEATURE                            STRID5("s-f", 0x1b730)
#define PROTOCOL_COMMAND_STORAGE_INFO                               STRID5("s-i", 0x27730)
#define PROTOCOL_COMMAND_STORAGE_INFO_LIST                          STRID5("s-l", 0x33730)
#define PROTOCOL_COMMAND_STORAGE_OPEN_READ                          STRID5("s-or", 0x93f730)
#define PROTOCOL_COMMAND_STORAGE_OPEN_WRITE                         STRID5("s-ow", 0xbbf730)
#define PROTOCOL_COMMAND_STORAGE_PATH_CREATE                        STRID5("s-pc", 0x1c3730)
#define PROTOCOL_COMMAND_STORAGE_REMOVE                             STRID5("s-r", 0x4b730)
#define PROTOCOL_COMMAND_STORAGE_PATH_REMOVE                        STRID5("s-pr", 0x943730)
#define PROTOCOL_COMMAND_STORAGE_PATH_SYNC                          STRID5("s-ps", 0x9c3730)

#define PROTOCOL_SERVER_HANDLER_STORAGE_REMOTE_LIST                                                                                \
    {.command = PROTOCOL_COMMAND_STORAGE_FEATURE, .handler = storageRemoteFeatureProtocol},                                        \
    {.command = PROTOCOL_COMMAND_STORAGE_INFO, .handler = storageRemoteInfoProtocol},                                              \
    {.command = PROTOCOL_COMMAND_STORAGE_INFO_LIST, .handler = storageRemoteInfoListProtocol},                                     \
    {.command = PROTOCOL_COMMAND_STORAGE_OPEN_READ, .handler = storageRemoteOpenReadProtocol},                                     \
    {.command = PROTOCOL_COMMAND_STORAGE_OPEN_WRITE, .handler = storageRemoteOpenWriteProtocol},                                   \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_CREATE, .handler = storageRemotePathCreateProtocol},                                 \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_REMOVE, .handler = storageRemotePathRemoveProtocol},                                 \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_SYNC, .handler = storageRemotePathSyncProtocol},                                     \
    {.command = PROTOCOL_COMMAND_STORAGE_REMOVE, .handler = storageRemoteRemoveProtocol},

#endif
