/***********************************************************************************************************************************
Configuration Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.intern.h"
#include "config/parse.h"
#include "config/protocol.h"

/**********************************************************************************************************************************/
void
configOptionProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        VariantList *optionList = varLstNew();

        for (unsigned int optionIdx = 0; optionIdx < varLstSize(paramList); optionIdx++)
        {
            CfgParseOptionResult option = cfgParseOptionP(varStr(varLstGet(paramList, optionIdx)));
            CHECK(option.found);

            varLstAdd(optionList, varDup(cfgOptionIdx(option.id, cfgOptionKeyToIdx(option.id, option.keyIdx + 1))));
        }

        protocolServerResponse(server, varNewVarLst(optionList));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
VariantList *
configOptionRemote(ProtocolClient *client, const VariantList *paramList)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
    FUNCTION_LOG_END();

    VariantList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_CONFIG_OPTION);

        for (unsigned int paramIdx = 0; paramIdx < varLstSize(paramList); paramIdx++)
            protocolCommandParamAdd(command, varLstGet(paramList, paramIdx));

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = varVarLst(protocolClientExecute(client, command, true));
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(VARIANT_LIST, result);
}
