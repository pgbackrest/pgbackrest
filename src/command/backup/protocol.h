/***********************************************************************************************************************************
Backup Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_PROTOCOL_H
#define COMMAND_BACKUP_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/variantList.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
void backupFileProtocol(const VariantList *paramList, ProtocolServer *server);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_BACKUP_FILE                                0x19b4021 /* StringId/5 "bp-f" */

#define PROTOCOL_SERVER_HANDLER_BACKUP_LIST                                                                                        \
    {.command = PROTOCOL_COMMAND_BACKUP_FILE, .handler = backupFileProtocol},

#endif
