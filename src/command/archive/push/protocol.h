/***********************************************************************************************************************************
Archive Push Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_PUSH_PROTOCOL_H
#define COMMAND_ARCHIVE_PUSH_PROTOCOL_H

#include "common/type/pack.h"
#include "common/type/string.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
void archivePushFileProtocol(PackRead *param, ProtocolServer *server);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_ARCHIVE_PUSH_FILE                          STRID5("ap-f", 0x36e010)

#define PROTOCOL_SERVER_HANDLER_ARCHIVE_PUSH_LIST                                                                                  \
    {.command = PROTOCOL_COMMAND_ARCHIVE_PUSH_FILE, .handler = archivePushFileProtocol},

#endif
