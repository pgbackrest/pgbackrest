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

        TEST_RESULT_STR(
            strLstJoin(cfgExecParam(cfgCmdArchiveGet, cfgCmdRoleAsync, NULL, false, true), "|"),
            strNewFmt(
                "--archive-async|--archive-timeout=5000ms|--no-config|--exec-id=1-test|--log-subprocess|--reset-neutral-umask"
                "|--pg1-path=\"%s/db path\"|--pg2-path=/db2|--repo1-path=%s/repo|--stanza=test1|archive-get:async",
                testPath(), testPath()),
            "exec archive-get -> archive-get:async");

        TEST_RESULT_STR(
            strLstJoin(cfgExecParam(cfgCmdBackup, cfgCmdRoleDefault, NULL, false, false), "|"),
            strNewFmt(
                "--archive-timeout=5000ms|--no-config|--exec-id=1-test|--log-subprocess|--reset-neutral-umask|--pg1-path=%s/db path"
                "|--pg2-path=/db2|--repo1-path=%s/repo|--stanza=test1|backup",
                testPath(), testPath()),
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
        kvPut(optionReplace, varNewStr(strNew("repo1-path")), varNewStr(strNew("/replace/path")));
        kvPut(optionReplace, varNewStr(strNew("stanza")), NULL);
        kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_PATH), VARSTRDEF("/log"));

        TEST_RESULT_STR(
            strLstJoin(cfgExecParam(cfgCmdRestore, cfgCmdRoleDefault, optionReplace, true, false), "|"),
            strNewFmt(
                "--db-include=1|--db-include=2|--exec-id=1-test|--log-path=/log|--pg1-path=%s/db path|--recovery-option=a=b"
                    "|--recovery-option=c=d|--repo1-path=/replace/path|restore",
                testPath()),
            "exec restore -> restore");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
