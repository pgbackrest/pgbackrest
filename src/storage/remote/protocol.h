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
#define PROTOCOL_COMMAND_STORAGE_FEATURE                            STR5ID3('s', '-', 'f')
#define PROTOCOL_COMMAND_STORAGE_INFO                               STR5ID3('s', '-', 'i')
#define PROTOCOL_COMMAND_STORAGE_INFO_LIST                          STR5ID3('s', '-', 'l')
#define PROTOCOL_COMMAND_STORAGE_OPEN_READ                          STR5ID4('s', '-', 'o', 'r')
#define PROTOCOL_COMMAND_STORAGE_OPEN_WRITE                         STR5ID4('s', '-', 'o', 'w')
#define PROTOCOL_COMMAND_STORAGE_PATH_CREATE                        STR5ID4('s', '-', 'p', 'c')
#define PROTOCOL_COMMAND_STORAGE_REMOVE                             STR5ID3('s', '-', 'r')
#define PROTOCOL_COMMAND_STORAGE_PATH_REMOVE                        STR5ID4('s', '-', 'p', 'r')
#define PROTOCOL_COMMAND_STORAGE_PATH_SYNC                          STR5ID4('s', '-', 'p', 's')

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
