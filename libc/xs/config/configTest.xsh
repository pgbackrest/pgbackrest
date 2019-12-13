/***********************************************************************************************************************************
Config XS Header
***********************************************************************************************************************************/
#include <string.h>

#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.h"

/***********************************************************************************************************************************
Build JSON output from options
***********************************************************************************************************************************/
String *
perlOptionJson(void)
{
    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        KeyValue *configKv = kvNew();

        for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
        {
            // Skip if not valid
            if (!cfgOptionValid(optionId))
                continue;

            Variant *optionVar = varNewKv(kvNew());

            // Add valid
            kvPut(varKv(optionVar), VARSTRDEF("valid"), BOOL_TRUE_VAR);

            // Add source
            const Variant *source = NULL;

            switch (cfgOptionSource(optionId))
            {
                case cfgSourceParam:
                {
                    source = varNewStrZ("param");
                    break;
                }

                case cfgSourceConfig:
                {
                    source = varNewStrZ("config");
                    break;
                }

                case cfgSourceDefault:
                {
                    source = varNewStrZ("default");
                    break;
                }
            }

            kvPut(varKv(optionVar), VARSTRDEF("source"), source);

            // Add negate and reset
            kvPut(varKv(optionVar), VARSTRDEF("negate"), VARBOOL(cfgOptionNegate(optionId)));
            kvPut(varKv(optionVar), VARSTRDEF("reset"), VARBOOL(cfgOptionReset(optionId)));

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
                        valueVar = varNewKv(kvNew());

                        const KeyValue *valueKv = cfgOptionKv(optionId);
                        const VariantList *keyList = kvKeyList(valueKv);

                        for (unsigned int listIdx = 0; listIdx < varLstSize(keyList); listIdx++)
                            kvPut(varKv(valueVar), varLstGet(keyList, listIdx), kvGet(valueKv, varLstGet(keyList, listIdx)));

                        break;
                    }

                    case cfgDefOptTypeList:
                    {
                        valueVar = varNewKv(kvNew());

                        const VariantList *valueList = cfgOptionLst(optionId);

                        for (unsigned int listIdx = 0; listIdx < varLstSize(valueList); listIdx++)
                            kvPut(varKv(valueVar), varLstGet(valueList, listIdx), BOOL_TRUE_VAR);

                        break;
                    }
                }

                kvPut(varKv(optionVar), VARSTRDEF("value"), valueVar);
            }

            kvPut(configKv, VARSTRZ(cfgOptionName(optionId)), optionVar);
        }

        memContextSwitch(MEM_CONTEXT_OLD());
        result = jsonFromKv(configKv);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
