/***********************************************************************************************************************************
Test Common Command Routines
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/stat.h"
#include "version.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cmdBegin() and cmdEnd()"))
    {
        cmdInit();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("single parameter");

        StringList *argList = strLstNew();
        strLstAddZ(argList, PROJECT_BIN);
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "10");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg1");
        strLstAddZ(argList, CFGCMD_ARCHIVE_GET);
        strLstAddZ(argList, "param1");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdBegin(), "command begin with command parameter");
        harnessLogResult(
            "P00   INFO: archive-get command begin " PROJECT_VERSION ": [param1] --archive-async --archive-timeout=10"
                " --exec-id=1-test --pg1-path=/pg1 --stanza=test");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multiple parameters");

        argList = strLstNew();
        strLstAddZ(argList, PROJECT_BIN);
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg1");
        strLstAddZ(argList, CFGCMD_ARCHIVE_GET);
        strLstAddZ(argList, "param1");
        strLstAddZ(argList, "param 2");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdBegin(), "command begin with command parameters");
        harnessLogResult(
            "P00   INFO: archive-get command begin " PROJECT_VERSION ": [param1, \"param 2\"] --archive-async --exec-id=1-test"
                " --pg1-path=/pg1 --stanza=test");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("reset, negate, list, hash options");

        argList = strLstNew();
        strLstAddZ(argList, PROJECT_BIN);
        hrnCfgArgRawNegate(argList, cfgOptConfig);
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 2, "/pg2");
        hrnCfgArgRawZ(argList, cfgOptRepoCipherType, "aes-256-cbc");
        hrnCfgArgRawReset(argList, cfgOptRepoHost);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/path/to the/repo");
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "db1");
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "db2");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "standby_mode=on");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "primary_conninfo=blah");
        strLstAddZ(argList, CFGCMD_RESTORE);
        hrnCfgEnvRawZ(cfgOptRepoCipherPass, "SECRET-STUFF");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdBegin(), "command begin");

        #define RESULT_OPTION                                                                                                      \
             " --no-config --db-include=db1 --db-include=db2 --exec-id=1-test --pg1-path=/pg1 --pg2-path=/pg2"                     \
             " --recovery-option=standby_mode=on --recovery-option=primary_conninfo=blah --repo1-cipher-pass=<redacted>"           \
             " --repo1-cipher-type=aes-256-cbc --reset-repo1-host --repo1-path=\"/path/to the/repo\" --stanza=test"

        harnessLogResult("P00   INFO: restore command begin " PROJECT_VERSION ":" RESULT_OPTION);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check options in cache");

        TEST_RESULT_STR_Z(cmdOption(), RESULT_OPTION, "option cache");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command begin does not log when level is too low");

        harnessLogLevelSet(logLevelWarn);
        TEST_RESULT_VOID(cmdBegin(), "command begin");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command end does not log when level is too low");

        TEST_RESULT_VOID(cmdEnd(0, NULL), "command end");
        harnessLogLevelReset();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command end with error");

        TEST_RESULT_VOID(cmdEnd(25, strNew("aborted with exception [025]")), "command end");
        harnessLogResult("P00   INFO: restore command end: aborted with exception [025]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command end with time");

        TEST_RESULT_VOID(cmdEnd(0, NULL), "command end");
        hrnLogReplaceAdd("\\([0-9]+ms\\)", "[0-9]+", "TIME", false);
        TEST_RESULT_LOG(
            "P00   INFO: restore command end: completed successfully ([TIME]ms)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command end with stat and without time");

        statInc(STRDEF("test"));
        cfgOptionSet(cfgOptLogTimestamp, cfgSourceParam, BOOL_FALSE_VAR);

        harnessLogLevelSet(logLevelDetail);

        TEST_RESULT_VOID(cmdEnd(0, NULL), "command end");
        TEST_RESULT_LOG(
            "P00 DETAIL: statistics: {\"test\":{\"total\":1}}\n"
            "P00   INFO: restore command end: completed successfully");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("switch to a new command so some options are not valid");

        cfgCommandSet(cfgCmdArchiveGet, cfgCmdRoleDefault);

        TEST_RESULT_VOID(cmdBegin(), "command begin");
        harnessLogResult(
            "P00   INFO: archive-get command begin " PROJECT_VERSION ": --no-config --exec-id=1-test --log-timestamp"
            " --pg1-path=/pg1 --pg2-path=/pg2 --repo1-cipher-pass=<redacted> --repo1-cipher-type=aes-256-cbc --reset-repo1-host"
            " --repo1-path=\"/path/to the/repo\" --stanza=test");

        harnessLogLevelReset();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
