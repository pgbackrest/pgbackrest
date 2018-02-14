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
#include "perl/config.h"

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
