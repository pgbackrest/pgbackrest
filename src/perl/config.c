/***********************************************************************************************************************************
Perl Configuration
***********************************************************************************************************************************/
#include <string.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.h"

/***********************************************************************************************************************************
Build JSON output from options
***********************************************************************************************************************************/
String *
perlOptionJson(void)
{
    FUNCTION_TEST_VOID();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        KeyValue *configKv = kvNew();

        for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
        {
            // Skip if not valid
            if (!cfgOptionValid(optionId))
                continue;

            Variant *optionVar = varNewKv();

            // Add valid
            kvPut(varKv(optionVar), varNewStr(STRDEF("valid")), varNewBool(true));

            // Add source
            const char *source = NULL;

            switch (cfgOptionSource(optionId))
            {
                case cfgSourceParam:
                {
                    source = "param";
                    break;
                }

                case cfgSourceConfig:
                {
                    source = "config";
                    break;
                }

                case cfgSourceDefault:
                {
                    source = "default";
                    break;
                }
            }

            kvPut(varKv(optionVar), varNewStr(STRDEF("source")), varNewStr(STR(source)));

            // Add negate and reset
            kvPut(varKv(optionVar), varNewStr(STRDEF("negate")), varNewBool(cfgOptionNegate(optionId)));
            kvPut(varKv(optionVar), varNewStr(STRDEF("reset")), varNewBool(cfgOptionReset(optionId)));

            // Add value if it is set
            if (cfgOptionTest(optionId))
            {
                const Variant *valueVar = NULL;

                switch (cfgDefOptionType(cfgOptionDefIdFromId(optionId)))
                {
                    case cfgDefOptTypeBoolean:
                    case cfgDefOptTypeFloat:
                    case cfgDefOptTypeInteger:
                    case cfgDefOptTypePath:
                    case cfgDefOptTypeSize:
                    case cfgDefOptTypeString:
                    {
                        valueVar = cfgOption(optionId);
                        break;
                    }

                    case cfgDefOptTypeHash:
                    {
                        valueVar = varNewKv();

                        const KeyValue *valueKv = cfgOptionKv(optionId);
                        const VariantList *keyList = kvKeyList(valueKv);

                        for (unsigned int listIdx = 0; listIdx < varLstSize(keyList); listIdx++)
                            kvPut(varKv(valueVar), varLstGet(keyList, listIdx), kvGet(valueKv, varLstGet(keyList, listIdx)));

                        break;
                    }

                    case cfgDefOptTypeList:
                    {
                        valueVar = varNewKv();

                        const VariantList *valueList = cfgOptionLst(optionId);

                        for (unsigned int listIdx = 0; listIdx < varLstSize(valueList); listIdx++)
                            kvPut(varKv(valueVar), varLstGet(valueList, listIdx), varNewBool(true));

                        break;
                    }
                }

                kvPut(varKv(optionVar), varNewStr(STRDEF("value")), valueVar);
            }

            kvPut(configKv, varNewStr(STR(cfgOptionName(optionId))), optionVar);
        }

        memContextSwitch(MEM_CONTEXT_OLD());
        result = kvToJson(configKv, 0);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}
