/***********************************************************************************************************************************
Archive Push Protocol Handler
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_PUSH_PROTOCOL_H
#define COMMAND_ARCHIVE_PUSH_PROTOCOL_H

#include "common/type/pack.h"
#include "common/type/string.h"
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
bool archivePushProtocol(const String *command, PackRead *param, ProtocolServer *server);

#endif
