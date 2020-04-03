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
bool dbProtocol(const String *command, const VariantList *paramList, ProtocolServer *server);

#endif
