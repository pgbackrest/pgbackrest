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
FN_EXTERN bool storageRemoteFeatureProtocol(PackRead *param, ProtocolServer *server, void *sessionData);
FN_EXTERN bool storageRemoteInfoProtocol(PackRead *param, ProtocolServer *server, void *sessionData);
FN_EXTERN bool storageRemoteLinkCreateProtocol(PackRead *param, ProtocolServer *server, void *sessionData);
FN_EXTERN bool storageRemoteListProtocol(PackRead *param, ProtocolServer *server, void *sessionData);
FN_EXTERN bool storageRemoteOpenWriteProtocol(PackRead *param, ProtocolServer *server, void *sessionData);
FN_EXTERN bool storageRemotePathCreateProtocol(PackRead *param, ProtocolServer *server, void *sessionData);
FN_EXTERN bool storageRemotePathRemoveProtocol(PackRead *param, ProtocolServer *server, void *sessionData);
FN_EXTERN bool storageRemotePathSyncProtocol(PackRead *param, ProtocolServer *server, void *sessionData);
FN_EXTERN void *storageRemoteReadOpenProtocol(PackRead *param, ProtocolServer *server, uint64_t sessionId);
FN_EXTERN bool storageRemoteReadProtocol(PackRead *param, ProtocolServer *server, void *sessionData);
FN_EXTERN bool storageRemoteRemoveProtocol(PackRead *param, ProtocolServer *server, void *sessionData);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_STORAGE_FEATURE                            STRID5("s-f", 0x1b730)
#define PROTOCOL_COMMAND_STORAGE_INFO                               STRID5("s-i", 0x27730)
#define PROTOCOL_COMMAND_STORAGE_LINK_CREATE                        STRID5("s-lc", 0x1b3730)
#define PROTOCOL_COMMAND_STORAGE_LIST                               STRID5("s-l", 0x33730)
#define PROTOCOL_COMMAND_STORAGE_READ                               STRID5("s-rd", 0x24b730)
#define PROTOCOL_COMMAND_STORAGE_OPEN_WRITE                         STRID5("s-wr", 0x95f730)
#define PROTOCOL_COMMAND_STORAGE_PATH_CREATE                        STRID5("s-pc", 0x1c3730)
#define PROTOCOL_COMMAND_STORAGE_REMOVE                             STRID5("s-r", 0x4b730)
#define PROTOCOL_COMMAND_STORAGE_PATH_REMOVE                        STRID5("s-pr", 0x943730)
#define PROTOCOL_COMMAND_STORAGE_PATH_SYNC                          STRID5("s-ps", 0x9c3730)

#define PROTOCOL_SERVER_HANDLER_STORAGE_REMOTE_LIST                                                                                \
    {.command = PROTOCOL_COMMAND_STORAGE_FEATURE, .process = storageRemoteFeatureProtocol},                                        \
    {.command = PROTOCOL_COMMAND_STORAGE_INFO, .process = storageRemoteInfoProtocol},                                              \
    {.command = PROTOCOL_COMMAND_STORAGE_LINK_CREATE, .process = storageRemoteLinkCreateProtocol},                                 \
    {.command = PROTOCOL_COMMAND_STORAGE_LIST, .process = storageRemoteListProtocol},                                              \
    {.command = PROTOCOL_COMMAND_STORAGE_READ, .open = storageRemoteReadOpenProtocol, .process = storageRemoteReadProtocol},       \
    {.command = PROTOCOL_COMMAND_STORAGE_OPEN_WRITE, .process = storageRemoteOpenWriteProtocol},                                   \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_CREATE, .process = storageRemotePathCreateProtocol},                                 \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_REMOVE, .process = storageRemotePathRemoveProtocol},                                 \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_SYNC, .process = storageRemotePathSyncProtocol},                                     \
    {.command = PROTOCOL_COMMAND_STORAGE_REMOVE, .process = storageRemoteRemoveProtocol},

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

FN_EXTERN void storageRemoteFilterHandlerSet(const StorageRemoteFilterHandler *filterHandler, unsigned int filterHandlerSize);

#endif
