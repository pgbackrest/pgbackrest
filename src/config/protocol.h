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
Functions
***********************************************************************************************************************************/
// Process config protocol requests
void configOptionProtocol(PackRead *const param, ProtocolServer *const server);

// Get option values from a remote process
VariantList *configOptionRemote(ProtocolClient *client, const VariantList *paramList);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_CONFIG_OPTION                              STRID5("opt-g", 0x7dd20f0)

#define PROTOCOL_SERVER_HANDLER_OPTION_LIST                                                                                        \
    {.command = PROTOCOL_COMMAND_CONFIG_OPTION, .handler = configOptionProtocol},

#endif
