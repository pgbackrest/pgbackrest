/***********************************************************************************************************************************
Exec Configuration
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "config/config.intern.h"
#include "config/exec.h"
#include "config/parse.h"

/**********************************************************************************************************************************/
StringList *
cfgExecParam(ConfigCommand commandId, ConfigCommandRole commandRoleId, const KeyValue *optionReplace, bool local, bool quote)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, commandId);
        FUNCTION_LOG_PARAM(ENUM, commandRoleId);
        FUNCTION_LOG_PARAM(KEY_VALUE, optionReplace);
        FUNCTION_LOG_PARAM(BOOL, local);                            // Will the new process be running on the same host?
        FUNCTION_LOG_PARAM(BOOL, quote);                            // Do parameters with spaces need to be quoted?
    FUNCTION_LOG_END();

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Loop though options and add the ones that apply to the specified command
        result = strLstNew();

        for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
        {
            // Skip the option if it is not valid for the original/specified command or if is secure. Also skip repo1-cipher-type
            // because there's no point of passing it if the other process doesn't have access to repo1-cipher-pass. There is
            // probably a better way to do this last part...
            if (!cfgParseOptionValid(commandId, commandRoleId, optionId) || cfgParseOptionSecure(optionId) ||
                optionId == cfgOptRepoCipherType)
            {
                continue;
            }

            // Loop through option indexes
            unsigned int optionIdxTotal = cfgOptionGroup(optionId) ? cfgOptionGroupIdxTotal(cfgOptionGroupId(optionId)) : 1;

            for (unsigned int optionIdx = 0; optionIdx < optionIdxTotal; optionIdx++)
            {
                // First check for a replacement
                const Variant *key = VARSTRZ(cfgOptionIdxName(optionId, optionIdx));
                const Variant *value = NULL;
                bool exists = false;

                // If an option is requested to be replaced (usually because remote processes do not have access to the config)
                // then if the option exists, get the new value for replacement
                if (optionReplace != NULL)
                {
                    exists = kvKeyExists(optionReplace, key);

                    if (exists)
                        value = kvGet(optionReplace, key);
                }

                // If the key exists but its value is NULL then skip this option
                if (exists && value == NULL)
                    continue;

                // If no replacement then see if this option is not default
                if (value == NULL && cfgOptionValid(optionId))
                {
                    if (cfgOptionIdxNegate(optionId, optionIdx))
                        value = BOOL_FALSE_VAR;
                    else if (cfgOptionIdxSource(optionId, optionIdx) != cfgSourceDefault)
                        value = cfgOptionIdx(optionId, optionIdx);
                }

                // If the option was reset
                if (value == NULL && cfgOptionValid(optionId) && cfgOptionIdxReset(optionId, optionIdx))
                {
                    strLstAdd(result, strNewFmt("--reset-%s", cfgOptionIdxName(optionId, optionIdx)));
                }
                // Else format the value if found, even if the option is not valid for the command
                else if (value != NULL && (!local || exists || cfgOptionIdxSource(optionId, optionIdx) == cfgSourceParam))
                {
                    if (varType(value) == varTypeBool)
                    {
                        strLstAdd(result, strNewFmt("--%s%s", varBool(value) ? "" : "no-", cfgOptionIdxName(optionId, optionIdx)));
                    }
                    else
                    {
                        StringList *valueList = NULL;

                        if (varType(value) == varTypeKeyValue)
                        {
                            valueList = strLstNew();

                            const KeyValue *optionKv = varKv(value);
                            const VariantList *keyList = kvKeyList(optionKv);

                            for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
                            {
                                strLstAdd(
                                    valueList,
                                    strNewFmt(
                                        "%s=%s", strZ(varStr(varLstGet(keyList, keyIdx))),
                                        strZ(varStrForce(kvGet(optionKv, varLstGet(keyList, keyIdx))))));
                            }
                        }
                        else if (varType(value) == varTypeVariantList)
                        {
                            valueList = strLstNewVarLst(varVarLst(value));
                        }
                        // Else only one value
                        else
                        {
                            valueList = strLstNew();
                            strLstAdd(valueList, cfgOptionIdxDisplay(optionId, optionIdx));
                        }

                        // Output options and values
                        for (unsigned int valueListIdx = 0; valueListIdx < strLstSize(valueList); valueListIdx++)
                        {
                            const String *value = strLstGet(valueList, valueListIdx);

                            if (quote && strchr(strZ(value), ' ') != NULL)
                                value = strNewFmt("\"%s\"", strZ(value));

                            strLstAdd(result, strNewFmt("--%s=%s", cfgOptionIdxName(optionId, optionIdx), strZ(value)));
                        }
                    }
                }
            }
        }

        // Add the command
        strLstAdd(result, cfgCommandRoleNameParam(commandId, commandRoleId, COLON_STR));

        // Move list to the prior context
        strLstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}
