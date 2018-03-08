/***********************************************************************************************************************************
Test Perl Exec
***********************************************************************************************************************************/
#include "config/config.h"
#include "config/load.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("perlMain()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandSet(cfgCmdInfo);
        cfgExeSet(strNew("/path/to/pgbackrest"));

        TEST_RESULT_STR(
            strPtr(perlMain()), "pgBackRest::Main::main('/path/to/pgbackrest','info')", "command with no options");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionValidSet(cfgOptCompress, true);
        cfgOptionSet(cfgOptCompress, cfgSourceParam, varNewBool(true));

        StringList *commandParamList = strLstNew();
        strLstAdd(commandParamList, strNew("A"));
        strLstAdd(commandParamList, strNew("B"));
        cfgCommandParamSet(commandParamList);

        TEST_RESULT_STR(
            strPtr(perlMain()),
            "pgBackRest::Main::main('/path/to/pgbackrest','info','A','B')",
            "command with one option and params");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("perlExec()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--log-level-console=off"));
        strLstAdd(argList, strNew("--log-level-stderr=off"));
        strLstAdd(argList, strNew("archive-push"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load archive-push config");

        int processId = 0;

        if ((processId = fork()) == 0)
        {
            perlExec();
        }
        // Wait for async process to exit (this should happen quickly) and report any errors
        else
        {
            int processStatus;

            THROW_ON_SYS_ERROR(
                waitpid(processId, &processStatus, 0) != processId, AssertError, "unable to find perl child process");

            TEST_RESULT_INT(WEXITSTATUS(processStatus), errorTypeCode(&ParamRequiredError), "check error code");
        }
    }
}
