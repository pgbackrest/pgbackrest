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
FN_EXTERN ProtocolServerResult *storageRemoteFeatureProtocol(PackRead *param);
FN_EXTERN ProtocolServerResult *storageRemoteInfoProtocol(PackRead *param);
FN_EXTERN ProtocolServerResult *storageRemoteLinkCreateProtocol(PackRead *param);
FN_EXTERN ProtocolServerResult *storageRemoteListProtocol(PackRead *param);
FN_EXTERN ProtocolServerResult *storageRemotePathCreateProtocol(PackRead *param);
FN_EXTERN ProtocolServerResult *storageRemotePathRemoveProtocol(PackRead *param);
FN_EXTERN ProtocolServerResult *storageRemotePathSyncProtocol(PackRead *param);
FN_EXTERN ProtocolServerResult *storageRemoteReadOpenProtocol(PackRead *param);
FN_EXTERN ProtocolServerResult *storageRemoteReadProtocol(PackRead *param, void *fileRead);
FN_EXTERN ProtocolServerResult *storageRemoteRemoveProtocol(PackRead *param);
FN_EXTERN ProtocolServerResult *storageRemoteWriteOpenProtocol(PackRead *param);
FN_EXTERN ProtocolServerResult *storageRemoteWriteProtocol(PackRead *param, void *fileWrite);
FN_EXTERN ProtocolServerResult *storageRemoteWriteCloseProtocol(PackRead *param, void *fileWrite);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_STORAGE_FEATURE                            STRID5("s-f", 0x1b730)
#define PROTOCOL_COMMAND_STORAGE_INFO                               STRID5("s-i", 0x27730)
#define PROTOCOL_COMMAND_STORAGE_LINK_CREATE                        STRID5("s-lc", 0x1b3730)
#define PROTOCOL_COMMAND_STORAGE_LIST                               STRID5("s-l", 0x33730)
#define PROTOCOL_COMMAND_STORAGE_PATH_CREATE                        STRID5("s-pc", 0x1c3730)
#define PROTOCOL_COMMAND_STORAGE_PATH_REMOVE                        STRID5("s-pr", 0x943730)
#define PROTOCOL_COMMAND_STORAGE_PATH_SYNC                          STRID5("s-ps", 0x9c3730)
#define PROTOCOL_COMMAND_STORAGE_READ                               STRID5("s-rd", 0x24b730)
#define PROTOCOL_COMMAND_STORAGE_REMOVE                             STRID5("s-r", 0x4b730)
#define PROTOCOL_COMMAND_STORAGE_WRITE                              STRID5("s-wr", 0x95f730)

#define PROTOCOL_SERVER_HANDLER_STORAGE_REMOTE_LIST                                                                                \
    {.command = PROTOCOL_COMMAND_STORAGE_FEATURE, .process = storageRemoteFeatureProtocol},                                        \
    {.command = PROTOCOL_COMMAND_STORAGE_INFO, .process = storageRemoteInfoProtocol},                                              \
    {.command = PROTOCOL_COMMAND_STORAGE_LINK_CREATE, .process = storageRemoteLinkCreateProtocol},                                 \
    {.command = PROTOCOL_COMMAND_STORAGE_LIST, .process = storageRemoteListProtocol},                                              \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_CREATE, .process = storageRemotePathCreateProtocol},                                 \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_REMOVE, .process = storageRemotePathRemoveProtocol},                                 \
    {.command = PROTOCOL_COMMAND_STORAGE_PATH_SYNC, .process = storageRemotePathSyncProtocol},                                     \
    {.command = PROTOCOL_COMMAND_STORAGE_READ, .open = storageRemoteReadOpenProtocol, .processSession = storageRemoteReadProtocol},\
    {.command = PROTOCOL_COMMAND_STORAGE_REMOVE, .process = storageRemoteRemoveProtocol},                                          \
    {.command = PROTOCOL_COMMAND_STORAGE_WRITE, .open = storageRemoteWriteOpenProtocol,                                            \
        .processSession = storageRemoteWriteProtocol, .close = storageRemoteWriteCloseProtocol},

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
