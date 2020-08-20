/***********************************************************************************************************************************
Test Common Command Routines
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/stat.h"
#include "version.h"

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
        cfgInit();
        cfgCommandSet(cfgCmdArchiveGet, cfgCmdRoleDefault);

        cfgOptionValidSet(cfgOptArchiveAsync, true);
        cfgOptionSet(cfgOptArchiveAsync, cfgSourceParam, varNewBool(true));

        StringList *commandParamList = strLstNew();
        strLstAddZ(commandParamList, "param1");
        cfgCommandParamSet(commandParamList);

        harnessLogLevelSet(logLevelInfo);

        TEST_RESULT_VOID(cmdBegin(true), "command begin with command parameter");
        harnessLogResult(
            "P00   INFO: archive-get command begin " PROJECT_VERSION ": [param1] --archive-async");

        strLstAddZ(commandParamList, "param 2");
        cfgCommandParamSet(commandParamList);

        TEST_RESULT_VOID(cmdBegin(true), "command begin with command parameters");
        harnessLogResult(
            "P00   INFO: archive-get command begin " PROJECT_VERSION ": [param1, \"param 2\"] --archive-async");

        cfgInit();
        cfgCommandSet(cfgCmdArchiveGet, cfgCmdRoleDefault);

        cfgOptionValidSet(cfgOptConfig, true);
        cfgOptionNegateSet(cfgOptConfig, true);
        cfgOptionSet(cfgOptConfig, cfgSourceParam, NULL);

        cfgOptionValidSet(cfgOptPgHost, true);
        cfgOptionSet(cfgOptPgHost, cfgSourceConfig, NULL);

        cfgOptionValidSet(cfgOptRepoHost, true);
        cfgOptionResetSet(cfgOptRepoHost, true);

        cfgOptionValidSet(cfgOptRepoPath, true);
        cfgOptionSet(cfgOptRepoPath, cfgSourceConfig, varNewStr(strNew("/path/to the/repo")));

        cfgOptionValidSet(cfgOptRepoS3Key, true);
        cfgOptionSet(cfgOptRepoS3Key, cfgSourceConfig, varNewStr(strNew("SECRET-STUFF")));

        cfgOptionValidSet(cfgOptDbInclude, true);
        StringList *list = strLstNew();
        strLstAddZ(list, "db1");
        strLstAddZ(list, "db2");
        cfgOptionSet(cfgOptDbInclude, cfgSourceParam, varNewVarLst(varLstNewStrLst(list)));

        cfgOptionValidSet(cfgOptRecoveryOption, true);
        Variant *recoveryVar = varNewKv(kvNew());
        KeyValue *recoveryKv = varKv(recoveryVar);
        kvPut(recoveryKv, varNewStr(strNew("standby_mode")), varNewStr(strNew("on")));
        kvPut(recoveryKv, varNewStr(strNew("primary_conn_info")), varNewStr(strNew("blah")));
        cfgOptionSet(cfgOptRecoveryOption, cfgSourceParam, recoveryVar);

        cfgCommandSet(cfgCmdRestore, cfgCmdRoleDefault);
        TEST_RESULT_VOID(cmdBegin(true), "command begin with option logging");
        harnessLogResult(
            "P00   INFO: restore command begin " PROJECT_VERSION ": --no-config --db-include=db1 --db-include=db2"
                " --recovery-option=standby_mode=on --recovery-option=primary_conn_info=blah --reset-repo1-host"
                " --repo1-path=\"/path/to the/repo\" --repo1-s3-key=<redacted>");

        cfgCommandSet(cfgCmdArchiveGet, cfgCmdRoleDefault);
        TEST_RESULT_VOID(cmdBegin(true), "command begin with limited option logging");
        harnessLogResult(
            "P00   INFO: archive-get command begin " PROJECT_VERSION ": --no-config --reset-repo1-host"
                " --repo1-path=\"/path/to the/repo\" --repo1-s3-key=<redacted>");

        TEST_RESULT_VOID(cmdBegin(false), "command begin no option logging");
        harnessLogResult(
            "P00   INFO: archive-get command begin");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check options in cache");

        TEST_RESULT_STR_Z(
            cmdOption(), " --no-config --reset-repo1-host --repo1-path=\"/path/to the/repo\" --repo1-s3-key=<redacted>",
            "command options");

        // Nothing should be logged for command begin when the log level is too low
        // -------------------------------------------------------------------------------------------------------------------------
        harnessLogLevelSet(logLevelWarn);
        TEST_RESULT_VOID(cmdBegin(true), "command begin no logging");

        // Nothing should be logged for command end when the log level is too low
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdEnd(0, NULL), "command end no logging");
        harnessLogLevelReset();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdEnd(25, strNew("aborted with exception [025]")), "command end with error");
        harnessLogResult("P00   INFO: archive-get command end: aborted with exception [025]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdEnd(0, NULL), "command end with success");
        harnessLogResult("P00   INFO: archive-get command end: completed successfully");

        // Make sure time output is covered but don't do expect testing since the output is variable
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionValidSet(cfgOptLogTimestamp, true);
        cfgOptionSet(cfgOptLogTimestamp, cfgSourceParam, varNewBool(false));

        TEST_RESULT_VOID(cmdEnd(0, NULL), "command end with success");
        harnessLogResult("P00   INFO: archive-get command end: completed successfully");

        cfgOptionSet(cfgOptLogTimestamp, cfgSourceParam, varNewBool(true));

        // Add one stat to make sure it gets output
        statInc(STRDEF("test"));

        harnessLogLevelSet(logLevelDetail);

        TEST_RESULT_VOID(cmdEnd(0, NULL), "command end with success");
        hrnLogReplaceAdd("\\([0-9]+ms\\)", "[0-9]+", "TIME", false);
        TEST_RESULT_LOG(
            "P00 DETAIL: statistics: {\"test\":{\"total\":1}}\n"
            "P00   INFO: archive-get command end: completed successfully ([TIME]ms)");

        harnessLogLevelReset();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
