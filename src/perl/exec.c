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
#include "config/config.h"
#include "perl/config.h"
#include "perl/exec.h"

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
Perl interpreter

This is a silly name but Perl prefers it.
***********************************************************************************************************************************/
static PerlInterpreter *my_perl = NULL;

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
        "($result, $message) = " PGBACKREST_MAIN "('%s'%s)", cfgCommandName(cfgCommand()), strPtr(commandParam));

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
Evaluate a perl statement
***********************************************************************************************************************************/
static void
perlEval(const String *statement)
{
    eval_pv(strPtr(statement), TRUE);
}

/***********************************************************************************************************************************
Initialize Perl
***********************************************************************************************************************************/
static void
perlInit()
{
    if (!my_perl)
    {
        // Initialize Perl with dummy args and environment
        int argc = 1;
        const char *argv[1] = {strPtr(cfgExe())};
        const char *env[1] = {NULL};
        PERL_SYS_INIT3(&argc, (char ***)&argv, (char ***)&env);

        // Create the interpreter
        const char *embedding[] = {"", "-M"PGBACKREST_MODULE, "-e", "0"};
        my_perl = perl_alloc();
        perl_construct(my_perl);

        // Don't let $0 assignment update the proctitle or embedding[0]
        PL_origalen = 1;

        // Start the interpreter
        perl_parse(my_perl, xs_init, 3, (char **)embedding, NULL);
        PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
        perl_run(my_perl);

        // Set config data -- this is done separately to avoid it being included in stack traces
        perlEval(strNewFmt(PGBACKREST_MAIN "ConfigSet('%s', '%s')", strPtr(cfgExe()), strPtr(perlOptionJson())));
    }
}

/***********************************************************************************************************************************
Execute main function in Perl
***********************************************************************************************************************************/
int
perlExec()
{
    // Initialize Perl
    perlInit();

    // Run perl main function
    perlEval(perlMain());

    // Return result code
    int code = (int)SvIV(get_sv("result", 0));
    char *message = SvPV_nolen(get_sv("message", 0));                               // {uncovered - internal Perl macro branch}

    if (code >= errorTypeCode(&AssertError))                                        // {uncovered - success tested in integration}
        THROW_CODE(code, strlen(message) == 0 ? PERL_EMBED_ERROR : message);        // {+uncovered}

    return code;                                                                    // {+uncovered}
}

/***********************************************************************************************************************************
Free Perl objects

Don't bother freeing Perl itself since we are about to exit.
***********************************************************************************************************************************/
void
perlFree(int result)
{
    if (my_perl != NULL)
        perlEval(strNewFmt(PGBACKREST_MAIN "Cleanup(%d)", result));
}
