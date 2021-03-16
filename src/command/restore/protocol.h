/***********************************************************************************************************************************
Restore Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_RESTORE_PROTOCOL_H
#define COMMAND_RESTORE_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/variantList.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
void restoreFileProtocol(const VariantList *paramList, ProtocolServer *server);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_RESTORE_FILE                               STRID4('r', 's', '-', 'f')

#define PROTOCOL_SERVER_HANDLER_RESTORE_LIST                                                                                       \
    {.command = PROTOCOL_COMMAND_RESTORE_FILE, .handler = restoreFileProtocol},

#endif
