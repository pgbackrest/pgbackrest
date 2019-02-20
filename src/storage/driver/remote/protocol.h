/***********************************************************************************************************************************
Remote Storage Protocol Handler
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_REMOTE_PROTOCOL_H
#define STORAGE_DRIVER_REMOTE_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/variantList.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_BLOCK_HEADER                                       "BRBLOCK"

#define PROTOCOL_COMMAND_STORAGE_EXISTS                             "storageExists"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_EXISTS_STR);
#define PROTOCOL_COMMAND_STORAGE_LIST                               "storageList"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_LIST_STR);
#define PROTOCOL_COMMAND_STORAGE_OPEN_READ                          "storageOpenRead"
    STRING_DECLARE(PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool storageDriverRemoteProtocol(const String *command, const VariantList *paramList, ProtocolServer *server);

#endif
