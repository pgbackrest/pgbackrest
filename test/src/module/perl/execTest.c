/***********************************************************************************************************************************
Test Perl Exec
***********************************************************************************************************************************/
#include "config/config.h"
#include "config/load.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("perlMain()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandSet(cfgCmdInfo);
        cfgExeSet(strNew("/path/to/pgbackrest"));

        TEST_RESULT_STR(
            strPtr(perlMain()), "($iResult, $bErrorC, $strMessage) = pgBackRest::Main::main('info')", "command with no options");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionValidSet(cfgOptCompress, true);
        cfgOptionSet(cfgOptCompress, cfgSourceParam, varNewBool(true));

        StringList *commandParamList = strLstNew();
        strLstAdd(commandParamList, strNew("A"));
        strLstAdd(commandParamList, strNew("B"));
        cfgCommandParamSet(commandParamList);

        TEST_RESULT_STR(
            strPtr(perlMain()), "($iResult, $bErrorC, $strMessage) = pgBackRest::Main::main('info','A','B')",
            "command with one option and params");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("perlInit(), perlExec(), and perlFree()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--log-level-console=off"));
        strLstAdd(argList, strNew("--log-level-stderr=off"));
        strLstAdd(argList, strNew("--log-level-file=off"));
        strLstAdd(argList, strNew("archive-push"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load archive-push config");

        TEST_RESULT_VOID(perlFree(0), "free Perl before it is init'd");
        TEST_RESULT_VOID(perlInit(), "init Perl");
        TEST_ERROR(perlExec(), ParamRequiredError, PERL_EMBED_ERROR);
        TEST_RESULT_VOID(perlFree(0), "free Perl");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
