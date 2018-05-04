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
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("cmdBegin() and cmdEnd()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandSet(cfgCmdArchiveGet);

        cfgOptionValidSet(cfgOptCompress, true);
        cfgOptionSet(cfgOptCompress, cfgSourceParam, varNewBool(true));

        StringList *commandParamList = strLstNew();
        strLstAddZ(commandParamList, "param1");
        cfgCommandParamSet(commandParamList);

        TEST_RESULT_VOID(cmdBegin(true), "command begin with command parameter");
        testLogResult(
            "P00   INFO: archive-get command begin " PGBACKREST_VERSION ": [param1] --compress");

        strLstAddZ(commandParamList, "param 2");
        cfgCommandParamSet(commandParamList);

        TEST_RESULT_VOID(cmdBegin(true), "command begin with command parameters");
        testLogResult(
            "P00   INFO: archive-get command begin " PGBACKREST_VERSION ": [param1, \"param 2\"] --compress");

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

        // !!! WHY DO WE STILL NEED TO CREATE THE VAR KV EMPTY?
        cfgOptionValidSet(cfgOptRecoveryOption, true);
        Variant *recoveryVar = varNewKv();
        KeyValue *recoveryKv = varKv(recoveryVar);
        kvPut(recoveryKv, varNewStr(strNew("standby_mode")), varNewStr(strNew("on")));
        kvPut(recoveryKv, varNewStr(strNew("primary_conn_info")), varNewStr(strNew("blah")));
        cfgOptionSet(cfgOptRecoveryOption, cfgSourceParam, recoveryVar);

        TEST_RESULT_VOID(cmdBegin(true), "command begin with option logging");
        testLogResult(
            "P00   INFO: archive-get command begin " PGBACKREST_VERSION ": --no-config --db-include=db1 --db-include=db2"
                " --recovery-option=standby_mode=on --recovery-option=primary_conn_info=blah --reset-repo1-host"
                " --repo1-path=\"/path/to the/repo\" --repo1-s3-key=<redacted>");

        TEST_RESULT_VOID(cmdBegin(false), "command begin no option logging");
        testLogResult(
            "P00   INFO: archive-get command begin");

        // Nothing should be logged for command begin when the log level is too low
        // -------------------------------------------------------------------------------------------------------------------------
        logInit(logLevelWarn, logLevelOff, logLevelOff, false);
        TEST_RESULT_VOID(cmdBegin(true), "command begin no logging");

        // Nothing should be logged for command end when the log level is too low
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdEnd(0, NULL), "command end no logging");
        logInit(logLevelInfo, logLevelOff, logLevelOff, false);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdEnd(0, NULL), "command end with success");
        testLogResult("P00   INFO: archive-get command end: completed successfully");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdEnd(25, strNew("aborted with exception [025]")), "command end with error");
        testLogResult("P00   INFO: archive-get command end: aborted with exception [025]");
    }
}
