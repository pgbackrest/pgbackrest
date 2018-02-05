/***********************************************************************************************************************************
Test Configuration Load
***********************************************************************************************************************************/
#include "common/log.h"

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("cfgLoad()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("archive-get"));

        TEST_ERROR(
            cfgLoad(strLstSize(argList), strLstPtr(argList)), OptionRequiredError,
            "archive-get command requires option: stanza");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(logLevelStdOut, logLevelOff, "console logging is off");
        TEST_RESULT_INT(logLevelStdErr, logLevelWarn, "stderr logging is warn");

        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--host-id=1"));
        strLstAdd(argList, strNew("--process=1"));
        strLstAdd(argList, strNew("--command=backup"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--type=backup"));
        strLstAdd(argList, strNew("local"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load local config");

        TEST_RESULT_STR(strPtr(cfgExe()), "pgbackrest", "check exe");
        TEST_RESULT_INT(logLevelStdOut, logLevelOff, "console logging is off");
        TEST_RESULT_INT(logLevelStdErr, logLevelWarn, "stderr logging is warn");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--repo1-path=/path/to/repo"));
        strLstAdd(argList, strNew("stanza-create"));

        TEST_RESULT_VOID(cfgLoadParam(strLstSize(argList), strLstPtr(argList), strNew("pgbackrest2")), "load local config");

        TEST_RESULT_STR(strPtr(cfgExe()), "pgbackrest2", "check exe");
        TEST_RESULT_INT(logLevelStdOut, logLevelWarn, "console logging is warn");
        TEST_RESULT_INT(logLevelStdErr, logLevelWarn, "stderr logging is warn");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("archive-push"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load archive-push config");
        TEST_RESULT_PTR(cfgOptionDefault(cfgOptRepoHostCmd), NULL, "    command archive-push, option repo1-host-cmd default");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--repo1-host=backup"));
        strLstAdd(argList, strNew("archive-push"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load archive-push config");
        TEST_RESULT_STR(
            strPtr(varStr(cfgOptionDefault(cfgOptRepoHostCmd))), strPtr(cfgExe()),
            "    command archive-push, option repo1-host-cmd default");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("backup"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load backup config");
        TEST_RESULT_PTR(cfgOptionDefault(cfgOptPgHostCmd), NULL, "    command backup, option pg1-host-cmd default");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--pg1-host=db"));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--pg3-host=db"));
        strLstAdd(argList, strNew("--pg3-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("backup"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load backup config");
        TEST_RESULT_STR(
            strPtr(varStr(cfgOptionDefault(cfgOptPgHostCmd))), strPtr(cfgExe()), "    command backup, option pg1-host-cmd default");
        TEST_RESULT_PTR(cfgOptionDefault(cfgOptPgHostCmd + 1), NULL, "    command backup, option pg2-host-cmd default");
        TEST_RESULT_STR(
            strPtr(varStr(cfgOptionDefault(cfgOptPgHostCmd + 2))), strPtr(cfgExe()),
            "    command backup, option pg3-host-cmd default");
    }
}
