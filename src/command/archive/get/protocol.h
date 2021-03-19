/***********************************************************************************************************************************
Archive Get Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_GET_PROTOCOL_H
#define COMMAND_ARCHIVE_GET_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/stringId.h"
#include "common/type/variantList.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
void archiveGetFileProtocol(const VariantList *paramList, ProtocolServer *server);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_ARCHIVE_GET_FILE                           STR5ID4('a', 'g', '-', 'f')

#define PROTOCOL_SERVER_HANDLER_ARCHIVE_GET_LIST                                                                                   \
    {.command = PROTOCOL_COMMAND_ARCHIVE_GET_FILE, .handler = archiveGetFileProtocol},

#endif
