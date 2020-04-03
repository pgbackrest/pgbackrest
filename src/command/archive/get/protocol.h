/***********************************************************************************************************************************
Archive Get Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_GET_PROTOCOL_H
#define COMMAND_ARCHIVE_GET_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/variantList.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_ARCHIVE_GET                                "archiveGet"
    STRING_DECLARE(PROTOCOL_COMMAND_ARCHIVE_GET_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
bool archiveGetProtocol(const String *command, const VariantList *paramList, ProtocolServer *server);

#endif
