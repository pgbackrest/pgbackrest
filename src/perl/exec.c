/***********************************************************************************************************************************
Execute Perl for Legacy Functionality
***********************************************************************************************************************************/
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "version.h"
#include "common/error.h"
#include "common/memContext.h"
#include "common/type.h"

/***********************************************************************************************************************************
Constants used to build perl options
***********************************************************************************************************************************/
#define PERL_EXE                                                    "perl"
#define ENV_EXE                                                     "/usr/bin/env"

#define PARAM_PERL_OPTION                                           "perl-option"
#define PARAM_PERL_OPTION_ID                                        1000

#define PGBACKREST_MODULE                                           PGBACKREST_NAME "::Main"
#define PGBACKREST_MAIN                                             PGBACKREST_MODULE "::main"

/***********************************************************************************************************************************
Build list of perl options to use for exec
***********************************************************************************************************************************/
StringList *perlCommand(int argListSize, char *argList[])
{
    // Setup arg list for perl exec
    StringList *perlArgList = strLstNew();
    strLstAdd(perlArgList, strNew(ENV_EXE));
    strLstAdd(perlArgList, strNew(PERL_EXE));

    // Setup Perl main call
    String *mainParamBuffer = strNew("");

    // Setup pgbackrest bin
    String *binParamBuffer = strNew("");

    // Reset optind to 1 in case getopt_long has been called before
    optind = 1;

    // Struct with all valid options
    static struct option optionList[] =
    {
        {PARAM_PERL_OPTION, required_argument, NULL, PARAM_PERL_OPTION_ID},
        {0, 0, NULL, 0},
    };

    // Parse options
    int option;
    int optionIdx;
    opterr = false;

    while ((option = getopt_long(argListSize, argList, "-", optionList, &optionIdx)) != -1)
    {
        switch (option)
        {
            case 1:
            case '?':
                strCat(mainParamBuffer, ", ");
                strCatFmt(mainParamBuffer, "'%s'", argList[optind - 1]);
                break;

            case PARAM_PERL_OPTION_ID:
                strLstAdd(perlArgList, strNew(optarg));

                strCatFmt(binParamBuffer, " --" PARAM_PERL_OPTION "=\"%s\"", optarg);
                break;
        }
    }

    // Finish Perl main call
    String *mainBuffer = strNewFmt(
        PGBACKREST_MAIN "('%s%s'%s)", argList[0], strPtr(binParamBuffer), strPtr(mainParamBuffer));

    strFree(binParamBuffer);
    strFree(mainParamBuffer);

    // Finish arg list for perl exec
    strLstAdd(perlArgList, strNew("-M" PGBACKREST_MODULE));
    strLstAdd(perlArgList, strNew("-e"));
    strLstAdd(perlArgList, mainBuffer);
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
