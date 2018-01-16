/***********************************************************************************************************************************
Test Configuration Load
***********************************************************************************************************************************/
#include "common/log.h"

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
void testRun()
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
    }
}
