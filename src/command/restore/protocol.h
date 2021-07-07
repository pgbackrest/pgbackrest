/***********************************************************************************************************************************
Restore Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_RESTORE_PROTOCOL_H
#define COMMAND_RESTORE_PROTOCOL_H

#include "common/type/pack.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
void restoreFileProtocol(PackRead *param, ProtocolServer *server);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_RESTORE_FILE                               STRID5("rs-f", 0x36e720)

#define PROTOCOL_SERVER_HANDLER_RESTORE_LIST                                                                                       \
    {.command = PROTOCOL_COMMAND_RESTORE_FILE, .handler = restoreFileProtocol},

#endif
