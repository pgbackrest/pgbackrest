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
#define PROTOCOL_COMMAND_VERIFY_FILE                               "verifyFile"
    STRING_DECLARE(PROTOCOL_COMMAND_VERIFY_FILE_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
bool verifyProtocol(const String *command, const VariantList *paramList, ProtocolServer *server);

#endif
