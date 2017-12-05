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
#include "config/parse.h"

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
StringList *perlCommand(int argListSize, const char *argList[])
{
    ParseData *parseData = configParseArg(argListSize, argList);

    // Begin arg list for perl exec
    StringList *perlArgList = strLstNew();
    strLstAdd(perlArgList, strNew(ENV_EXE));
    strLstAdd(perlArgList, strNew(PERL_EXE));

    // Construct option list to pass to main
    String *mainCallParam = strNew("");

    for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
    {
        ParseOption *option = &parseData->parseOptionList[optionId];

        // If option was found
        if (option->found)
        {
            // If option was negated
            if (option->negate)
                strCatFmt(mainCallParam, ", '--no-%s\'", cfgOptionName(optionId));
            // Else option with no arguments
            else if (option->valueList == NULL)
                strCatFmt(mainCallParam, ", '--%s'", cfgOptionName(optionId));
            // Else options with arguments
            else
            {
                for (unsigned int argIdx = 0; argIdx < strLstSize(option->valueList); argIdx++)
                {
                    strCatFmt(mainCallParam, ", '--%s'", cfgOptionName(optionId));
                    strCatFmt(mainCallParam, ", '%s'", strPtr(strLstGet(option->valueList, argIdx)));
                }
            }
        }
    }

    // Add command to pass to main
    strCatFmt(mainCallParam, ", '%s'", cfgCommandName(parseData->command));

    // Add command arguments to pass to main
    if (parseData->commandArgList != NULL)
        for (unsigned int argIdx = 0; argIdx < strLstSize(parseData->commandArgList); argIdx++)
            strCatFmt(mainCallParam, ", '%s'", strPtr(strLstGet(parseData->commandArgList, argIdx)));

    // Construct perl option list to add to bin
    String *binPerlOption = strNew("");

    if (parseData->perlOptionList != NULL)
        for (unsigned int argIdx = 0; argIdx < strLstSize(parseData->perlOptionList); argIdx++)
        {
            // Add to bin option list
            strCatFmt(binPerlOption, " --" PARAM_PERL_OPTION "=\"%s\"", strPtr(strLstGet(parseData->perlOptionList, argIdx)));

            // Add to list that will be passed to exec
            strLstAdd(perlArgList, strLstGet(parseData->perlOptionList, argIdx));
        }

    // Construct Perl main call
    String *mainCall = strNewFmt(PGBACKREST_MAIN "('%s%s'%s)", argList[0], strPtr(binPerlOption), strPtr(mainCallParam));

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
