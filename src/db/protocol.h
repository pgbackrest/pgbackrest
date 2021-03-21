/***********************************************************************************************************************************
Db Protocol Handler
***********************************************************************************************************************************/
#ifndef DB_PROTOCOL_H
#define DB_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/variantList.h"
#include "protocol/client.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process db protocol requests
void dbOpenProtocol(const VariantList *paramList, ProtocolServer *server);
void dbQueryProtocol(const VariantList *paramList, ProtocolServer *server);
void dbCloseProtocol(const VariantList *paramList, ProtocolServer *server);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_DB_OPEN                                    STRID5("db-o", 0x7ec440)
#define PROTOCOL_COMMAND_DB_QUERY                                   STRID5("db-q", 0x8ec440)
#define PROTOCOL_COMMAND_DB_CLOSE                                   STRID5("db-c", 0x1ec440)

#define PROTOCOL_SERVER_HANDLER_DB_LIST                                                                                            \
    {.command = PROTOCOL_COMMAND_DB_OPEN, .handler = dbOpenProtocol},                                                              \
    {.command = PROTOCOL_COMMAND_DB_QUERY, .handler = dbQueryProtocol},                                                            \
    {.command = PROTOCOL_COMMAND_DB_CLOSE, .handler = dbCloseProtocol},

#endif
