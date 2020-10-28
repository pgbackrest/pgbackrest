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

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_CONFIG_OPTION_STR,                   PROTOCOL_COMMAND_CONFIG_OPTION);

/**********************************************************************************************************************************/
bool
configProtocol(const String *command, const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, PROTOCOL_COMMAND_CONFIG_OPTION_STR))
        {
            VariantList *optionList = varLstNew();

            for (unsigned int optionIdx = 0; optionIdx < varLstSize(paramList); optionIdx++)
            {
                CfgParseOptionResult option = cfgParseOption(varStr(varLstGet(paramList, optionIdx)));
                CHECK(option.found);

                varLstAdd(optionList, varDup(cfgOptionIdx(option.id, cfgOptionKeyToIdx(option.id, option.keyIdx + 1))));
            }

            protocolServerResponse(server, varNewVarLst(optionList));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}

/**********************************************************************************************************************************/
VariantList *
configProtocolOption(ProtocolClient *client, const VariantList *paramList)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
    FUNCTION_LOG_END();

    VariantList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_CONFIG_OPTION_STR);

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
