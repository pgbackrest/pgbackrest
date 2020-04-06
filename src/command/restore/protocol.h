/***********************************************************************************************************************************
Restore Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_RESTORE_PROTOCOL_H
#define COMMAND_RESTORE_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/variantList.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_RESTORE_FILE                               "restoreFile"
    STRING_DECLARE(PROTOCOL_COMMAND_RESTORE_FILE_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
bool restoreProtocol(const String *command, const VariantList *paramList, ProtocolServer *server);

#endif
