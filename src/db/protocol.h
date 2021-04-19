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
Constants
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_DB_OPEN                                    "dbOpen"
    STRING_DECLARE(PROTOCOL_COMMAND_DB_OPEN_STR);
#define PROTOCOL_COMMAND_DB_QUERY                                   "dbQuery"
    STRING_DECLARE(PROTOCOL_COMMAND_DB_QUERY_STR);
#define PROTOCOL_COMMAND_DB_CLOSE                                   "dbClose"
    STRING_DECLARE(PROTOCOL_COMMAND_DB_CLOSE_STR);

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
#define PROTOCOL_SERVER_HANDLER_DB_LIST                                                                                            \
    {.command = PROTOCOL_COMMAND_DB_OPEN, .handler = dbOpenProtocol},                                                              \
    {.command = PROTOCOL_COMMAND_DB_QUERY, .handler = dbQueryProtocol},                                                            \
    {.command = PROTOCOL_COMMAND_DB_CLOSE, .handler = dbCloseProtocol},

#endif
