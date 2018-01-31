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

        TEST_RESULT_INT(logLevelStdOut, logLevelOff, "console logging is off");
        TEST_RESULT_INT(logLevelStdErr, logLevelWarn, "stderr logging is warn");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--repo-path=/path/to/repo"));
        strLstAdd(argList, strNew("stanza-create"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load local config");

        TEST_RESULT_INT(logLevelStdOut, logLevelWarn, "console logging is off");
        TEST_RESULT_INT(logLevelStdErr, logLevelOff, "stderr logging is off");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("archive-push"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load archive-push config");
        TEST_RESULT_PTR(cfgOptionDefault(cfgOptBackupCmd), NULL, "    command archive-push, option backup-cmd default");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--backup-host=backup"));
        strLstAdd(argList, strNew("archive-push"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load archive-push config");
        TEST_RESULT_STR(
            strPtr(varStr(cfgOptionDefault(cfgOptBackupCmd))), strPtr(cfgExe()),
            "    command archive-push, option backup-cmd default");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("backup"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load backup config");
        TEST_RESULT_PTR(cfgOptionDefault(cfgOptDbCmd), NULL, "    command backup, option db1-cmd default");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--db-host=db"));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--db3-host=db"));
        strLstAdd(argList, strNew("--db3-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("backup"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load backup config");
        TEST_RESULT_STR(
            strPtr(varStr(cfgOptionDefault(cfgOptDbCmd))), strPtr(cfgExe()), "    command backup, option db1-cmd default");
        TEST_RESULT_PTR(cfgOptionDefault(cfgOptDbCmd + 1), NULL, "    command backup, option db2-cmd default");
        TEST_RESULT_STR(
            strPtr(varStr(cfgOptionDefault(cfgOptDbCmd + 2))), strPtr(cfgExe()), "    command backup, option db3-cmd default");
    }
}
