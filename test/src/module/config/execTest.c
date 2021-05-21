/***********************************************************************************************************************************
Test Exec Configuration
***********************************************************************************************************************************/
#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cfgExecParam()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "5");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db path", testPath()));
        strLstAddZ(argList, "--pg2-path=/db2");
        strLstAddZ(argList, "--log-subprocess");
        strLstAddZ(argList, "--no-config");
        strLstAddZ(argList, "--reset-neutral-umask");
        strLstAddZ(argList, "--repo-cipher-type=aes-256-cbc");
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        strLstAddZ(argList, "archive-get");

        // Set repo1-cipher-pass to make sure it is not passed on the command line
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "1234", true);
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));
        unsetenv("PGBACKREST_REPO1_CIPHER_PASS");

        TEST_RESULT_STRLST_Z(
            cfgExecParam(cfgCmdArchiveGet, cfgCmdRoleAsync, NULL, false, true),
            "--archive-async\n--no-config\n--exec-id=1-test\n--log-subprocess\n--reset-neutral-umask\n"
                "--pg1-path=\"" TEST_PATH "/db path\"\n--pg2-path=/db2\n--repo1-path=" TEST_PATH "/repo\n--stanza=test1\n"
                "archive-get:async\n",
            "exec archive-get -> archive-get:async");

        TEST_RESULT_STRLST_Z(
            cfgExecParam(cfgCmdBackup, cfgCmdRoleMain, NULL, false, false),
            "--archive-timeout=5\n--no-config\n--exec-id=1-test\n--log-subprocess\n--reset-neutral-umask\n"
                "--pg1-path=" TEST_PATH "/db path\n--pg2-path=/db2\n--repo1-path=" TEST_PATH "/repo\n--stanza=test1\nbackup\n",
            "exec archive-get -> backup");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db path", testPath()));
        strLstAddZ(argList, "--db-include=1");
        strLstAddZ(argList, "--db-include=2");
        strLstAddZ(argList, "--recovery-option=a=b");
        strLstAddZ(argList, "--recovery-option=c=d");
        hrnCfgArgRawReset(argList, cfgOptLogPath);
        strLstAddZ(argList, "restore");

        setenv("PGBACKREST_REPO1_HOST", "bogus", true);
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));
        unsetenv("PGBACKREST_REPO1_HOST");

        KeyValue *optionReplace = kvNew();
        kvPut(optionReplace, VARSTRDEF("repo1-path"), VARSTRDEF("/replace/path"));
        kvPut(optionReplace, VARSTRDEF("stanza"), NULL);
        kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_PATH), VARSTRDEF("/log"));

        TEST_RESULT_STRLST_Z(
            cfgExecParam(cfgCmdRestore, cfgCmdRoleMain, optionReplace, true, false),
            "--db-include=1\n--db-include=2\n--exec-id=1-test\n--log-path=/log\n--pg1-path=" TEST_PATH "/db path\n"
                "--recovery-option=a=b\n--recovery-option=c=d\n--repo1-path=/replace/path\nrestore\n",
            "exec restore -> restore");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
