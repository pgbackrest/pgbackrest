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
#define PROTOCOL_COMMAND_ARCHIVE_GET_FILE                           "archiveGetFile"
    STRING_DECLARE(PROTOCOL_COMMAND_ARCHIVE_GET_FILE_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
void archiveGetFileProtocol(const VariantList *paramList, ProtocolServer *server);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_SERVER_HANDLER_ARCHIVE_GET_LIST                                                                                   \
    {.command = PROTOCOL_COMMAND_ARCHIVE_GET_FILE, .handler = archiveGetFileProtocol},

#endif
