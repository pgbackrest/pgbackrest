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

            // Output option
            if (strSize(result) != 1)
                strCat(result, ",");

            strCatFmt(result, "\"%s\":{", cfgOptionName(optionId));

            // Output source unless it is default
            if (cfgOptionSource(optionId) != cfgSourceDefault)
            {
                strCat(result, "\"source\":\"");

                if (cfgOptionSource(optionId) == cfgSourceParam)
                    strCat(result, "param");
                else
                    strCat(result, "config");

                strCat(result, "\"");

                // Add a comma if another define will be added
                if (cfgOptionTest(optionId))
                    strCat(result, ",");
            }

            // If option was negated
            if (cfgOptionNegate(optionId))
                strCatFmt(result, "\"negate\":%s", strPtr(varStrForce(varNewBool(true))));
            else
            {
                // If option is reset then add flag
                if (cfgOptionReset(optionId))
                    strCatFmt(result, "\"reset\":%s", strPtr(varStrForce(varNewBool(true))));

                // If has a value
                if (cfgOptionTest(optionId))
                {
                    // If option is reset, then add a comma separator before setting the value
                    if (cfgOptionReset(optionId))
                        strCat(result, ",");

                    strCat(result, "\"value\":");

                    switch (cfgDefOptionType(cfgOptionDefIdFromId(optionId)))
                    {
                        case cfgDefOptTypeBoolean:
                        case cfgDefOptTypeFloat:
                        case cfgDefOptTypeInteger:
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
            }

            strCat(result, "}");
        }

        strCat(result, "}");
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
