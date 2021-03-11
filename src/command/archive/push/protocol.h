/***********************************************************************************************************************************
Archive Push Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_PUSH_PROTOCOL_H
#define COMMAND_ARCHIVE_PUSH_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/variantList.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_ARCHIVE_PUSH                               "archivePush"
    STRING_DECLARE(PROTOCOL_COMMAND_ARCHIVE_PUSH_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
void archivePushProtocol(const VariantList *paramList, ProtocolServer *server);

#endif
