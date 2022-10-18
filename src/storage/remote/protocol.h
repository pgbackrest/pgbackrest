/***********************************************************************************************************************************
Remote Storage Protocol Handler
***********************************************************************************************************************************/
#ifndef STORAGE_REMOTE_PROTOCOL_H
#define STORAGE_REMOTE_PROTOCOL_H

#include "common/type/pack.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process storage protocol requests
void storageRemoteFeatureProtocol(PackRead *param, ProtocolServer *server);
void storageRemoteInfoProtocol(PackRead *param, ProtocolServer *server);
void storageRemoteLinkCreateProtocol(PackRead *param, ProtocolServer *server);
void storageRemoteListProtocol(PackRead *param, ProtocolServer *server);
void storageRemoteOpenReadProtocol(PackRead *param, ProtocolServer *server);
void storageRemoteOpenWriteProtocol(PackRead *param, ProtocolServer *server);
void storageRemotePathCreateProtocol(PackRead *param, ProtocolServer *server);
void storageRemotePathRemoveProtocol(PackRead *param, ProtocolServer *server);
void storageRemotePathSyncProtocol(PackRead *param, ProtocolServer *server);
void storageRemoteRemoveProtocol(PackRead *param, ProtocolServer *server);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_STORAGE_FEATURE                            STRID5("s-f", 0x1b730)
#define PROTOCOL_COMMAND_STORAGE_INFO                               STRID5("s-i", 0x27730)
#define PROTOCOL_COMMAND_STORAGE_LINK_CREATE                        STRID5("s-lc", 0x1b3730)
#define PROTOCOL_COMMAND_STORAGE_LIST                               STRID5("s-l", 0x33730)
#define PROTOCOL_COMMAND_STORAGE_OPEN_READ                          STRID5("s-or", 0x93f730)
#define PROTOCOL_COMMAND_STORAGE_OPEN_WRITE                         STRID5("s-ow", 0xbbf730)
#define PROTOCOL_COMMAND_STORAGE_PATH_CREATE                        STRID5("s-pc", 0x1c3730)
#define PROTOCOL_COMMAND_STORAGE_REMOVE                             STRID5("s-r", 0x4b730)
#define PROTOCOL_COMMAND_STORAGE_PATH_REMOVE                        STRID5("s-pr", 0x943730)
#define PROTOCOL_COMMAND_STORAGE_PATH_SYNC                          STRID5("s-ps", 0x9c3730)

#define PROTOCOL_SERVER_HANDLER_STORAGE_REMOTE_LIST                                                                                \
    {.command = PROTOCOL_COMMAND_STORAGE_FEATURE, .handler = storageRemoteFeatureProtocol},                                        \
    {.command = PROTOCOL_COMMAND_STORAGE_INFO, .handler = storageRemoteInfoProtocol},                                              \
    {.command = PROTOCOL_COMMAND_STORAGE_LINK_CREATE, .handler = storageRemoteLinkCreateProtocol},                                 \
    {.command = PROTOCOL_COMMAND_STORAGE_LIST, .handler = storageRemoteListProtocol},                                              \
    {.command = PROTOCOL_COMMAND_STORAGE_OPEN_READ, .handler = storageRemoteOpenReadProtocol},                                     \
    {.command = PROTOCOL_COMMAND_STORAGE_OPEN_WRITE, .handler = storageRemoteOpenWriteProtocol},                                   \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_CREATE, .handler = storageRemotePathCreateProtocol},                                 \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_REMOVE, .handler = storageRemotePathRemoveProtocol},                                 \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_SYNC, .handler = storageRemotePathSyncProtocol},                                     \
    {.command = PROTOCOL_COMMAND_STORAGE_REMOVE, .handler = storageRemoteRemoveProtocol},

/***********************************************************************************************************************************
Filters that may be passed to a remote
***********************************************************************************************************************************/
typedef IoFilter *(*StorageRemoteFilterHandlerParam)(const Pack *paramList);
typedef IoFilter *(*StorageRemoteFilterHandlerNoParam)(void);

typedef struct StorageRemoteFilterHandler
{
    StringId type;                                                  // Filter type
    StorageRemoteFilterHandlerParam handlerParam;                   // Handler for filter with parameters
    StorageRemoteFilterHandlerNoParam handlerNoParam;               // Handler with no parameters
} StorageRemoteFilterHandler;

void storageRemoteFilterHandlerSet(const StorageRemoteFilterHandler *filterHandler, unsigned int filterHandlerSize);

#endif
