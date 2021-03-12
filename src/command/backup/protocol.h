/***********************************************************************************************************************************
Backup Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_PROTOCOL_H
#define COMMAND_BACKUP_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/variantList.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_BACKUP_FILE                                "backupFile"
    STRING_DECLARE(PROTOCOL_COMMAND_BACKUP_FILE_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
void backupFileProtocol(const VariantList *paramList, ProtocolServer *server);

#endif
