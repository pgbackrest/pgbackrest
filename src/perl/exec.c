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

#if __GNUC__ > 4 || (__GNUC__ == 4 &&  __GNUC_MINOR__ >= 8)
    #define WARNING_PEDANTIC 1
#endif

#pragma GCC diagnostic ignored "-Wsign-conversion"

#if WARNING_PEDANTIC
    #pragma GCC diagnostic ignored "-Wpedantic"
#endif

#ifndef HAS_BOOL
#  define HAS_BOOL 1
#endif

#include <EXTERN.h>
#include <perl.h>

#if WARNING_PEDANTIC
    #pragma GCC diagnostic warning "-Wpedantic"
#endif

#pragma GCC diagnostic warning "-Wsign-conversion"

/***********************************************************************************************************************************
Constants used to build perl options
***********************************************************************************************************************************/
#define PGBACKREST_MODULE                                           PGBACKREST_NAME "::Main"
#define PGBACKREST_MAIN                                             PGBACKREST_MODULE "::main"

/***********************************************************************************************************************************
Build list of parameters to use for perl main
***********************************************************************************************************************************/
String *
perlMain()
{
    // Add command arguments to pass to main
    String *commandParam = strNew("");

    for (unsigned int paramIdx = 0; paramIdx < strLstSize(cfgCommandParam()); paramIdx++)
        strCatFmt(commandParam, ",'%s'", strPtr(strLstGet(cfgCommandParam(), paramIdx)));

    // Construct Perl main call
    String *mainCall = strNewFmt(
        PGBACKREST_MAIN "('%s','%s','%s'%s)", strPtr(cfgExe()), cfgCommandName(cfgCommand()), strPtr(perlOptionJson()),
        strPtr(commandParam));

    return mainCall;
}

/***********************************************************************************************************************************
Init the dynaloader so other C modules can be loaded
***********************************************************************************************************************************/
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

static void xs_init(pTHX)
{
    const char *file = __FILE__;
    dXSUB_SYS;
    PERL_UNUSED_CONTEXT;

    /* DynaLoader is a special case */
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

/***********************************************************************************************************************************
Execute main function in Perl
***********************************************************************************************************************************/
void
perlExec()
{
    // Initialize Perl with dummy args and environment
    int argc = 1;
    const char *argv[1] = {strPtr(cfgExe())};
    const char *env[1] = {NULL};
    PERL_SYS_INIT3(&argc, (char ***)&argv, (char ***)&env);

    // Create the interpreter
    const char *embedding[] = {"", "-M"PGBACKREST_MODULE, "-e", "0"};
    PerlInterpreter *my_perl = perl_alloc();
    perl_construct(my_perl);

    // Don't let $0 assignment update the proctitle or embedding[0]
    PL_origalen = 1;

    // Start the interpreter
    perl_parse(my_perl, xs_init, 3, (char **)embedding, NULL);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    perl_run(my_perl);

    // Run perl main function
    eval_pv(strPtr(perlMain()), TRUE);
}                                                                   // {uncoverable - perlExec() does not return}
