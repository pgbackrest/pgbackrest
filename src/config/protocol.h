/***********************************************************************************************************************************
Configuration Protocol Handler
***********************************************************************************************************************************/
#ifndef CONFIG_PROTOCOL_H
#define CONFIG_PROTOCOL_H

#include "common/type/string.h"
#include "common/type/variantList.h"
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
void configOptionProtocol(const VariantList *paramList, ProtocolServer *server);

// Get option values from another process
VariantList *configProtocolOption(ProtocolClient *client, const VariantList *paramList);

#endif
