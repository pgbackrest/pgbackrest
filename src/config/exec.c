/***********************************************************************************************************************************
Exec Configuration
***********************************************************************************************************************************/
#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "config/exec.h"

/***********************************************************************************************************************************
Load log settings
***********************************************************************************************************************************/
StringList *
cfgExecParam(ConfigCommand commandId, const KeyValue *optionReplace)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, commandId);
        FUNCTION_LOG_PARAM(KEY_VALUE, optionReplace);
    FUNCTION_LOG_END();

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Loop though options and add the ones that apply to the specified command
        result = strLstNew();
        ConfigDefineCommand commandDefId = cfgCommandDefIdFromId(commandId);

        for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
        {
            ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

            // Skip the option if it is not valid for the specified command or if is secure
            if (!cfgDefOptionValid(commandDefId, optionDefId) || cfgDefOptionSecure(optionDefId))
                continue;

            // First check for a replacement
            const Variant *key = varNewStr(strNew(cfgOptionName(optionId)));
            const Variant *value = NULL;
            bool exists = false;

            if (optionReplace != NULL)
            {
                exists = kvKeyExists(optionReplace, key);

                if (exists)
                    value = kvGet(optionReplace, key);
            }

            // If the key exists but is NULL then skip this option
            if (exists && value == NULL)
                continue;

            // If no replacement then see if this option is valid for the current command and is not default
            if (value == NULL && cfgOptionValid(optionId))
            {
                if (cfgOptionNegate(optionId))
                    value = varNewBool(false);
                else if (cfgOptionSource(optionId) != cfgSourceDefault)
                    value = cfgOption(optionId);
            }

            // Format the value if found
            if (value != NULL)
            {
                if (varType(value) == varTypeBool)
                {
                    strLstAdd(result, strNewFmt("--%s%s", varBool(value) ? "" : "no-", cfgOptionName(optionId)));
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
                                    "%s=%s", strPtr(varStr(varLstGet(keyList, keyIdx))),
                                        strPtr(varStrForce(kvGet(optionKv, varLstGet(keyList, keyIdx))))));
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
                        strLstAdd(valueList, varStrForce(value));
                    }

                    // Output options and values
                    for (unsigned int valueListIdx = 0; valueListIdx < strLstSize(valueList); valueListIdx++)
                    {
                        const String *value = strLstGet(valueList, valueListIdx);

                        if (strchr(strPtr(value), ' ') != NULL)
                            value = strNewFmt("\"%s\"", strPtr(value));

                        strLstAdd(result, strNewFmt("--%s=%s", cfgOptionName(optionId), strPtr(value)));
                    }
                }
            }
        }

        // Add the requested command
        strLstAddZ(result, cfgCommandName(commandId));

        // Move list to the calling context
        strLstMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}
