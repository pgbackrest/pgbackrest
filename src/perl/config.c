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
            // Skip the option if it is not valid
            if (!cfgOptionValid(optionId))
                continue;

            String *option = strNew("");

            // Output source unless it is default
            if (cfgOptionSource(optionId) != cfgSourceDefault)
            {
                strCat(option, "\"source\":\"");

                if (cfgOptionSource(optionId) == cfgSourceParam)
                    strCat(option, "param");
                else
                    strCat(option, "config");

                strCat(option, "\"");
            }

            // If option was negated
            if (cfgOptionNegate(optionId))
            {
                // Add comma if needed
                if (strSize(option) != 0)
                    strCat(option, ",");

                strCatFmt(option, "\"negate\":%s", strPtr(varStrForce(varNewBool(true))));
            }
            else
            {
                // If option is reset then add flag
                if (cfgOptionReset(optionId))
                {
                    // Add comma if needed
                    if (strSize(option) != 0)
                        strCat(option, ",");

                    strCatFmt(option, "\"reset\":%s", strPtr(varStrForce(varNewBool(true))));
                }

                // If has a value
                if (cfgOptionTest(optionId))
                {
                    // Add comma if needed
                    if (strSize(option) != 0)
                        strCat(option, ",");

                    strCat(option, "\"value\":");

                    switch (cfgDefOptionType(cfgOptionDefIdFromId(optionId)))
                    {
                        case cfgDefOptTypeBoolean:
                        case cfgDefOptTypeFloat:
                        case cfgDefOptTypeInteger:
                        {
                            strCat(option, strPtr(varStrForce(cfgOption(optionId))));
                            break;
                        }

                        case cfgDefOptTypeString:
                        {
                            strCatFmt(option, "\"%s\"", strPtr(cfgOptionStr(optionId)));
                            break;
                        }

                        case cfgDefOptTypeHash:
                        {
                            const KeyValue *valueKv = cfgOptionKv(optionId);
                            const VariantList *keyList = kvKeyList(valueKv);

                            strCat(option, "{");

                            for (unsigned int listIdx = 0; listIdx < varLstSize(keyList); listIdx++)
                            {
                                if (listIdx != 0)
                                    strCat(option, ",");

                                strCatFmt(
                                    option, "\"%s\":\"%s\"", strPtr(varStr(varLstGet(keyList, listIdx))),
                                    strPtr(varStr(kvGet(valueKv, varLstGet(keyList, listIdx)))));
                            }

                            strCat(option, "}");

                            break;
                        }

                        case cfgDefOptTypeList:
                        {
                            StringList *valueList = strLstNewVarLst(cfgOptionLst(optionId));

                            strCat(option, "{");

                            for (unsigned int listIdx = 0; listIdx < strLstSize(valueList); listIdx++)
                            {
                                if (listIdx != 0)
                                    strCat(option, ",");

                                strCatFmt(option, "\"%s\":true", strPtr(strLstGet(valueList, listIdx)));
                            }

                            strCat(option, "}");

                            break;
                        }
                    }
                }
            }

            // Add option to main JSON blob
            if (strSize(result) != 1)
                strCat(result, ",");

            strCatFmt(result, "\"%s\":{%s}", cfgOptionName(optionId), strPtr(option));
        }

        strCat(result, "}");
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
