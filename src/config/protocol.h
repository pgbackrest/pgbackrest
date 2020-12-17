/***********************************************************************************************************************************
Configuration Protocol Handler
***********************************************************************************************************************************/
#ifndef CONFIG_PROTOCOL_H
#define CONFIG_PROTOCOL_H

#include "common/type/pack.h"
#include "common/type/string.h"
#include "protocol/client.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_CONFIG_OPTION                              "configOption"
    STRING_DECLARE(PROTOCOL_COMMAND_CONFIG_OPTION_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process config protocol requests
bool configProtocol(const String *command, PackRead *param, ProtocolServer *server);

// Get option values from another process
VariantList *configProtocolOption(ProtocolClient *client, const VariantList *paramList);

#endif
