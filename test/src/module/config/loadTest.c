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

        TEST_RESULT_INT(logLevelStdOut, logLevelWarn, "console logging is error");
        TEST_RESULT_INT(logLevelStdErr, logLevelWarn, "stderr logging is error");

        // -------------------------------------------------------------------------------------------------------------------------

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
        TEST_RESULT_INT(logLevelStdOut, logLevelWarn, "console logging is warn");
        TEST_RESULT_INT(logLevelStdErr, logLevelWarn, "stderr logging is warn");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cfgLoadParam(strLstSize(argList), strLstPtr(argList), strNew("pgbackrest2")), "load local config");

        TEST_RESULT_STR(strPtr(cfgExe()), "pgbackrest2", "check exe");

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

        // Set a distinct umask value and test that the umask is reset by configLoad since default for neutral-umask=y
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("archive-get"));
        strLstAdd(argList, strNew("--stanza=db"));

        umask(0111);
        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for neutral-umask");
        TEST_RESULT_INT(umask(0111), 0000, "    umask was reset");

        // Set a distinct umask value and test that the umask is not reset by configLoad with option --no-neutral-umask
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("archive-get"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--no-neutral-umask"));

        umask(0111);
        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for no-neutral-umask");
        TEST_RESULT_INT(umask(0), 0111, "    umask was not reset");

        // db-timeout / protocol-timeout tests
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("archive-get"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--db-timeout=1830"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for protocol-timeout reset");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptDbTimeout), 1830, "    db-timeout set");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptProtocolTimeout), 1860, "    protocol-timeout set greater than db-timeout");

        // Set the protocol-timeout so the source is the command line and not the default
        strLstAdd(argList, strNew("--protocol-timeout=1820"));
        TEST_ERROR(
            cfgLoad(strLstSize(argList), strLstPtr(argList)), OptionInvalidValueError,
            strPtr(strNewFmt("'%f' is not valid for '%s' option\n"
            "HINT '%s' option (%f) should be greater than '%s' option (%f).",
            cfgOptionDbl(cfgOptProtocolTimeout), cfgOptionName(cfgOptProtocolTimeout),
            cfgOptionName(cfgOptProtocolTimeout), cfgOptionDbl(cfgOptProtocolTimeout), cfgOptionName(cfgOptDbTimeout),
            cfgOptionDbl(cfgOptDbTimeout))));

        // pg-host and repo-host tests
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("archive-get"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--pg2-host=pg2"));

        TEST_RESULT_VOID(cfgLoad(strLstSize(argList), strLstPtr(argList)), "load config for pg and repo host test");

        strLstAdd(argList, strNew("--repo1-host=repo1"));

        TEST_ERROR(
            cfgLoad(strLstSize(argList), strLstPtr(argList)), ConfigError,
            "pg and repo hosts cannot both be configured as remote");
    }
}
