/***********************************************************************************************************************************
Archive Get Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_GET_PROTOCOL_H
#define COMMAND_ARCHIVE_GET_PROTOCOL_H

#include "common/type/pack.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
FN_EXTERN ProtocolServerResult *archiveGetFileProtocol(PackRead *param);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_ARCHIVE_GET_FILE                           STRID5("ag-f", 0x36ce10)

#define PROTOCOL_SERVER_HANDLER_ARCHIVE_GET_LIST                                                                                   \
    {.command = PROTOCOL_COMMAND_ARCHIVE_GET_FILE, .process = archiveGetFileProtocol},

#endif
