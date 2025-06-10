/***********************************************************************************************************************************
Backup Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_PROTOCOL_H
#define COMMAND_BACKUP_PROTOCOL_H

#include "common/type/pack.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
FN_EXTERN ProtocolServerResult *backupFileProtocol(PackRead *param);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_BACKUP_FILE                                STRID5("bp-f", 0x36e020)

#define PROTOCOL_SERVER_HANDLER_BACKUP_LIST                                                                                        \
    {.command = PROTOCOL_COMMAND_BACKUP_FILE, .process = backupFileProtocol},

#endif
