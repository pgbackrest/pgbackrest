/***********************************************************************************************************************************
Configuration Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.intern.h"
#include "config/parse.h"
#include "config/protocol.h"

/**********************************************************************************************************************************/
FN_EXTERN ProtocolServerResult *
configOptionProtocol(PackRead *const param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);

    ProtocolServerResult *const result = protocolServerResultNewP();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        VariantList *const optionList = varLstNew();

        while (pckReadNext(param))
        {
            CfgParseOptionResult option = cfgParseOptionP(pckReadStrP(param));
            CHECK(AssertError, option.found, "option not found");

            varLstAdd(optionList, varDup(cfgOptionIdxVar(option.id, cfgOptionKeyToIdx(option.id, option.keyIdx + 1))));
        }

        pckWriteStrP(protocolServerResultData(result), jsonFromVar(varNewVarLst(optionList)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PROTOCOL_SERVER_RESULT, result);
}

/**********************************************************************************************************************************/
FN_EXTERN VariantList *
configOptionRemote(ProtocolClient *const client, const VariantList *const paramList)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
    FUNCTION_LOG_END();

    VariantList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const param = protocolPackNew();

        for (unsigned int paramIdx = 0; paramIdx < varLstSize(paramList); paramIdx++)
            pckWriteStrP(param, varStr(varLstGet(paramList, paramIdx)));

        const VariantList *const list = varVarLst(
            jsonToVar(pckReadStrP(protocolClientRequestP(client, PROTOCOL_COMMAND_CONFIG_OPTION, .param = param))));

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = varLstDup(list);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(VARIANT_LIST, result);
}
