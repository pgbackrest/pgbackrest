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
Build list of perl options to use for exec
***********************************************************************************************************************************/
StringList *perlCommand()
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

    // Construct option list to pass to main
    String *configJson = strNew("{");

    for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
    {
        // Skip the option if it is not valid
        if (!cfgOptionValid(optionId))
            continue;

        // Output option
        if (strSize(configJson) != 1)
            strCat(configJson, ",");

        strCatFmt(configJson, "\"%s\":{", cfgOptionName(optionId));

        // Output source unless it is default
        if (cfgOptionSource(optionId) != cfgSourceDefault)
        {
            strCat(configJson, "\"source\":\"");

            if (cfgOptionSource(optionId) == cfgSourceParam)
                strCat(configJson, "param");
            else
                strCat(configJson, "config");

            strCat(configJson, "\"");
        }

        // If option was negated
        if (cfgOptionNegate(optionId))
            strCatFmt(configJson, ",\"negate\":%s", strPtr(varStrForce(varNewBool(true))));
        // Else not negated and has a value
        else if (cfgOption(optionId) != NULL)
        {
            if (cfgOptionSource(optionId) != cfgSourceDefault)
                strCat(configJson, ",");

            strCat(configJson, "\"value\":");

            switch (cfgDefOptionType(cfgOptionDefIdFromId(optionId)))
            {
                case cfgDefOptTypeBoolean:
                case cfgDefOptTypeFloat:
                case cfgDefOptTypeInteger:
                {
                    strCat(configJson, strPtr(varStrForce(cfgOption(optionId))));
                    break;
                }

                case cfgDefOptTypeString:
                {
                    strCatFmt(configJson, "\"%s\"", strPtr(cfgOptionStr(optionId)));
                    break;
                }

                case cfgDefOptTypeHash:
                {
                    const KeyValue *valueKv = cfgOptionKv(optionId);
                    const VariantList *keyList = kvKeyList(valueKv);

                    strCat(configJson, "{");

                    for (unsigned int listIdx = 0; listIdx < varLstSize(keyList); listIdx++)
                    {
                        if (listIdx != 0)
                            strCat(configJson, ",");

                        strCatFmt(
                            configJson, "\"%s\":\"%s\"", strPtr(varStr(varLstGet(keyList, listIdx))),
                            strPtr(varStr(kvGet(valueKv, varLstGet(keyList, listIdx)))));
                    }

                    strCat(configJson, "}");

                    break;
                }

                case cfgDefOptTypeList:
                {
                    StringList *valueList = strLstNewVarLst(cfgOptionLst(optionId));

                    strCat(configJson, "{");

                    for (unsigned int listIdx = 0; listIdx < strLstSize(valueList); listIdx++)
                    {
                        if (listIdx != 0)
                            strCat(configJson, ",");

                        strCatFmt(configJson, "\"%s\":true", strPtr(strLstGet(valueList, listIdx)));
                    }

                    strCat(configJson, "}");

                    break;
                }
            }
        }

        strCat(configJson, "}");
    }

    strCat(configJson, "}");

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
        PGBACKREST_MAIN "('%s','%s','%s'%s)", strPtr(cfgExe()), cfgCommandName(cfgCommand()), strPtr(configJson),
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
void perlExec(StringList *perlArgList)
{
    // Exec perl with supplied arguments
    execvp(strPtr(strLstGet(perlArgList, 0)), (char **)strLstPtr(perlArgList));

    // The previous command only returns on error so throw it
    int errNo = errno;
    THROW(AssertError, "unable to exec %s: %s", strPtr(strLstGet(perlArgList, 0)), strerror(errNo));
}                                                                   // {uncoverable - perlExec() does not return}
