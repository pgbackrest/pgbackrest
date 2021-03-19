/***********************************************************************************************************************************
Archive Push Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_PUSH_PROTOCOL_H
#define COMMAND_ARCHIVE_PUSH_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/variantList.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process protocol requests
void archivePushFileProtocol(const VariantList *paramList, ProtocolServer *server);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_ARCHIVE_PUSH_FILE                          STR5ID4('a', 'p', '-', 'f')

#define PROTOCOL_SERVER_HANDLER_ARCHIVE_PUSH_LIST                                                                                  \
    {.command = PROTOCOL_COMMAND_ARCHIVE_PUSH_FILE, .handler = archivePushFileProtocol},

#endif
