/***********************************************************************************************************************************
Perl Configuration
***********************************************************************************************************************************/
#include "common/memContext.h"
#include "config/config.h"

/***********************************************************************************************************************************
Build JSON output from options
***********************************************************************************************************************************/
String *
perlOptionJson()
{
    String *result = strNew("{");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
        {
            // Skip if not valid
            if (!cfgOptionValid(optionId))
                continue;

            // Add comma if not first valid option
            if (strSize(result) != 1)
                strCat(result, ",");

            // Add valid and source
            strCatFmt(result, "\"%s\":{\"valid\":true,\"source\":\"", cfgOptionName(optionId));

            switch (cfgOptionSource(optionId))
            {
                case cfgSourceParam:
                {
                    strCat(result, "param");
                    break;
                }

                case cfgSourceConfig:
                {
                    strCat(result, "config");
                    break;
                }

                case cfgSourceDefault:
                {
                    strCat(result, "default");
                    break;
                }
            }

            strCat(result, "\"");

            // Add negate
            strCatFmt(result, ",\"negate\":%s", strPtr(varStrForce(varNewBool(cfgOptionNegate(optionId)))));

            // Add reset
            strCatFmt(result, ",\"reset\":%s", strPtr(varStrForce(varNewBool(cfgOptionReset(optionId)))));

            // Add value if it is set
            if (cfgOptionTest(optionId))
            {
                strCat(result, ",\"value\":");

                switch (cfgDefOptionType(cfgOptionDefIdFromId(optionId)))
                {
                    case cfgDefOptTypeBoolean:
                    case cfgDefOptTypeFloat:
                    case cfgDefOptTypeInteger:
                    case cfgDefOptTypeSize:
                    {
                        strCat(result, strPtr(varStrForce(cfgOption(optionId))));
                        break;
                    }

                    case cfgDefOptTypeString:
                    {
                        strCatFmt(result, "\"%s\"", strPtr(cfgOptionStr(optionId)));
                        break;
                    }

                    case cfgDefOptTypeHash:
                    {
                        const KeyValue *valueKv = cfgOptionKv(optionId);
                        const VariantList *keyList = kvKeyList(valueKv);

                        strCat(result, "{");

                        for (unsigned int listIdx = 0; listIdx < varLstSize(keyList); listIdx++)
                        {
                            if (listIdx != 0)
                                strCat(result, ",");

                            strCatFmt(
                                result, "\"%s\":\"%s\"", strPtr(varStr(varLstGet(keyList, listIdx))),
                                strPtr(varStr(kvGet(valueKv, varLstGet(keyList, listIdx)))));
                        }

                        strCat(result, "}");

                        break;
                    }

                    case cfgDefOptTypeList:
                    {
                        StringList *valueList = strLstNewVarLst(cfgOptionLst(optionId));

                        strCat(result, "{");

                        for (unsigned int listIdx = 0; listIdx < strLstSize(valueList); listIdx++)
                        {
                            if (listIdx != 0)
                                strCat(result, ",");

                            strCatFmt(result, "\"%s\":true", strPtr(strLstGet(valueList, listIdx)));
                        }

                        strCat(result, "}");

                        break;
                    }
                }
            }

            strCat(result, "}");
        }

        strCat(result, "}");
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
