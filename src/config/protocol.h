/***********************************************************************************************************************************
Configuration Protocol Handler
***********************************************************************************************************************************/
#ifndef CONFIG_PROTOCOL_H
#define CONFIG_PROTOCOL_H

#include "common/type/pack.h"
#include "protocol/client.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process config protocol requests
FN_EXTERN void configOptionProtocol(PackRead *param, ProtocolServer *server);

// Get option values from a remote process
FN_EXTERN VariantList *configOptionRemote(ProtocolClient *client, const VariantList *paramList);

/***********************************************************************************************************************************
Protocol commands for ProtocolServerHandler arrays passed to protocolServerProcess()
***********************************************************************************************************************************/
#define PROTOCOL_COMMAND_CONFIG_OPTION                              STRID5("opt-g", 0x7dd20f0)

#define PROTOCOL_SERVER_HANDLER_OPTION_LIST                                                                                        \
    {.command = PROTOCOL_COMMAND_CONFIG_OPTION, .handler = configOptionProtocol},

#endif
