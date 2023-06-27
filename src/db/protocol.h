/***********************************************************************************************************************************
Db Protocol Handler
***********************************************************************************************************************************/
#ifndef DB_PROTOCOL_H
#define DB_PROTOCOL_H

#include "common/type/pack.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process db protocol requests
FN_EXTERN void *dbOpenProtocol(PackRead *param, ProtocolServer *server, uint64_t sessionId);
FN_EXTERN bool dbQueryProtocol(PackRead *param, ProtocolServer *server, void *sessionData);
FN_EXTERN void dbCloseProtocol(PackRead *param, ProtocolServer *server, void *sessionData); // !!! REMOVE THIS?

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_DB                                         STRID5("db", 0x440)

#define PROTOCOL_SERVER_HANDLER_DB_LIST                                                                                            \
    {.command = PROTOCOL_COMMAND_DB, .open = dbOpenProtocol, .processSession = dbQueryProtocol, .close = dbCloseProtocol},

#endif
