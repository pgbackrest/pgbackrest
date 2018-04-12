/***********************************************************************************************************************************
Common Command Routines
***********************************************************************************************************************************/
#include <string.h>

#include "common/assert.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "version.h"

/***********************************************************************************************************************************
Debug Asserts
***********************************************************************************************************************************/
// The command must be set
#define ASSERT_DEBUG_COMMAND_SET()                                                                                                 \
    ASSERT_DEBUG(cfgCommand() != cfgCmdNone)

/***********************************************************************************************************************************
Begin the command
***********************************************************************************************************************************/
void
cmdBegin()
{
    ASSERT_DEBUG_COMMAND_SET();

    // This is fairly expensive log message to generate so skip it if it won't be output
    if (logWill(cfgLogLevelDefault()))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Basic info on command start
            String *info = strNewFmt("%s command begin %s:", cfgCommandName(cfgCommand()), PGBACKREST_VERSION);

            // Loop though options and add the ones that are interesting
            for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
            {
                // Skip the option if it is not valid
                if (!cfgOptionValid(optionId))
                    continue;

                // If option was negated
                if (cfgOptionNegate(optionId))
                    strCatFmt(info, " --no-%s", cfgOptionName(optionId));
                // If option was reset
                else if (cfgOptionReset(optionId))
                    strCatFmt(info, " --reset-%s", cfgOptionName(optionId));
                // Else set and not default
                else if (cfgOptionSource(optionId) != cfgSourceDefault && cfgOptionTest(optionId))
                {
                    ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

                    // Don't show redacted options
                    if (cfgDefOptionSecure(optionDefId))
                        strCatFmt(info, " --%s=<redacted>", cfgOptionName(optionId));
                    // Output boolean option
                    else if (cfgDefOptionType(optionDefId) == cfgDefOptTypeBoolean)
                        strCatFmt(info, " --%s", cfgOptionName(optionId));
                    // Output other options
                    else
                    {
                        StringList *valueList = NULL;

                        // Generate the values of hash options
                        if (cfgDefOptionType(optionDefId) == cfgDefOptTypeHash)
                        {
                            valueList = strLstNew();

                            const KeyValue *optionKv = cfgOptionKv(optionId);
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
                        // Generate values for list options
                        else if (cfgDefOptionType(optionDefId) == cfgDefOptTypeList)
                        {
                            valueList = strLstNewVarLst(cfgOptionLst(optionId));
                        }
                        // Else only one value
                        else
                        {
                            valueList = strLstNew();
                            strLstAdd(valueList, varStrForce(cfgOption(optionId)));
                        }

                        // Output options and values
                        for (unsigned int valueListIdx = 0; valueListIdx < strLstSize(valueList); valueListIdx++)
                        {
                            const String *value = strLstGet(valueList, valueListIdx);

                            strCatFmt(info, " --%s", cfgOptionName(optionId));

                            if (strchr(strPtr(value), ' ') != NULL)
                                value = strNewFmt("\"%s\"", strPtr(value));

                            strCatFmt(info, "=%s", strPtr(value));
                        }
                    }
                }
            }

            LOG_ANY(cfgLogLevelDefault(), 0, strPtr(info));
        }
        MEM_CONTEXT_TEMP_END();
    }
}

/***********************************************************************************************************************************
End the command
***********************************************************************************************************************************/
void
cmdEnd(int code)
{
    ASSERT_DEBUG_COMMAND_SET();

    // Skip this log message if it won't be output.  It's not too expensive but since we skipped cmdBegin(), may as well.
    if (logWill(cfgLogLevelDefault()))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Basic info on command end
            String *info = strNewFmt("%s command end: ", cfgCommandName(cfgCommand()));

            if (code == 0)
                strCat(info, "completed successfully");
            else
                strCatFmt(info, "aborted with exception [%03d]", code);

            LOG_ANY(cfgLogLevelDefault(), 0, strPtr(info));
        }
        MEM_CONTEXT_TEMP_END();
    }
}
