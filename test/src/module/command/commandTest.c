/***********************************************************************************************************************************
Test Common Command Routines
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "storage/storage.h"
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
        cfgCommandSet(cfgCmdArchiveGet);

        cfgOptionValidSet(cfgOptCompress, true);
        cfgOptionSet(cfgOptCompress, cfgSourceParam, varNewBool(true));

        StringList *commandParamList = strLstNew();
        strLstAddZ(commandParamList, "param1");
        cfgCommandParamSet(commandParamList);

        TEST_RESULT_VOID(cmdBegin(true), "command begin with command parameter");
        harnessLogResult(
            "P00   INFO: archive-get command begin " PROJECT_VERSION ": [param1] --compress");

        strLstAddZ(commandParamList, "param 2");
        cfgCommandParamSet(commandParamList);

        TEST_RESULT_VOID(cmdBegin(true), "command begin with command parameters");
        harnessLogResult(
            "P00   INFO: archive-get command begin " PROJECT_VERSION ": [param1, \"param 2\"] --compress");

        cfgInit();
        cfgCommandSet(cfgCmdArchiveGet);

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

        cfgOptionValidSet(cfgOptCompress, true);

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

        TEST_RESULT_VOID(cmdBegin(true), "command begin with option logging");
        harnessLogResult(
            "P00   INFO: archive-get command begin " PROJECT_VERSION ": --no-config --db-include=db1 --db-include=db2"
                " --recovery-option=standby_mode=on --recovery-option=primary_conn_info=blah --reset-repo1-host"
                " --repo1-path=\"/path/to the/repo\" --repo1-s3-key=<redacted>");

        TEST_RESULT_VOID(cmdBegin(false), "command begin no option logging");
        harnessLogResult(
            "P00   INFO: archive-get command begin");

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

        TEST_RESULT_VOID(cmdEnd(0, NULL), "command end with success");
        harnessLogResultRegExp("P00   INFO\\: archive-get command end: completed successfully \\([0-9]+ms\\)");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
