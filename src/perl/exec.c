/***********************************************************************************************************************************
Execute Perl for Legacy Functionality
***********************************************************************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "version.h"
#include "common/error.h"
#include "common/memContext.h"
#include "common/type.h"
#include "config/config.h"

/***********************************************************************************************************************************
Constants used to build perl options
***********************************************************************************************************************************/
#define PERL_EXE                                                    "perl"
#define ENV_EXE                                                     "/usr/bin/env"

#define PARAM_PERL_OPTION                                           "perl-option"

#define PGBACKREST_MODULE                                           PGBACKREST_NAME "::Main"
#define PGBACKREST_MAIN                                             PGBACKREST_MODULE "::main"

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
                if (cfgOption(optionId) != NULL)
                    strCat(result, ",");
            }

            // If option was negated
            if (cfgOptionNegate(optionId))
                strCatFmt(result, "\"negate\":%s", strPtr(varStrForce(varNewBool(true))));
            else
            {
                // If option is reset then add indicator
                if (cfgOptionReset(optionId))
                    strCatFmt(result, "\"reset\":%s", strPtr(varStrForce(varNewBool(true))));

                // Else not negated and has a value
                if (cfgOption(optionId) != NULL)
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

/***********************************************************************************************************************************
Build list of perl options to use for exec
***********************************************************************************************************************************/
StringList *
perlCommand()
{
    // Begin arg list for perl exec
    StringList *perlArgList = strLstNew();

    // Use specific perl bin if passed
    if (cfgOption(cfgOptPerlBin) != NULL)
        strLstAdd(perlArgList, strDup(cfgOptionStr(cfgOptPerlBin)));
    // Otherwise use env to find it
    else
    {
        strLstAdd(perlArgList, strNew(ENV_EXE));
        strLstAdd(perlArgList, strNew(PERL_EXE));
    }

    // Add command arguments to pass to main
    String *commandParam = strNew("");

    for (unsigned int paramIdx = 0; paramIdx < strLstSize(cfgCommandParam()); paramIdx++)
        strCatFmt(commandParam, ",'%s'", strPtr(strLstGet(cfgCommandParam(), paramIdx)));

    // Add Perl options
    StringList *perlOptionList = strLstNewVarLst(cfgOptionLst(cfgOptPerlOption));

    if (perlOptionList != NULL)
        for (unsigned int argIdx = 0; argIdx < strLstSize(perlOptionList); argIdx++)
            strLstAdd(perlArgList, strLstGet(perlOptionList, argIdx));

    // Construct Perl main call
    String *mainCall = strNewFmt(
        PGBACKREST_MAIN "('%s','%s','%s'%s)", strPtr(cfgExe()), cfgCommandName(cfgCommand()), strPtr(perlOptionJson()),
        strPtr(commandParam));

    // End arg list for perl exec
    strLstAdd(perlArgList, strNew("-M" PGBACKREST_MODULE));
    strLstAdd(perlArgList, strNew("-e"));
    strLstAdd(perlArgList, mainCall);
    strLstAdd(perlArgList, NULL);

    return perlArgList;
}

/***********************************************************************************************************************************
Exec supplied Perl options
***********************************************************************************************************************************/
void
perlExec(StringList *perlArgList)
{
    // Exec perl with supplied arguments
    execvp(strPtr(strLstGet(perlArgList, 0)), (char **)strLstPtr(perlArgList));

    // The previous command only returns on error so throw it
    int errNo = errno;
    THROW(AssertError, "unable to exec %s: %s", strPtr(strLstGet(perlArgList, 0)), strerror(errNo));
}                                                                   // {uncoverable - perlExec() does not return}
