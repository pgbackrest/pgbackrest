/***********************************************************************************************************************************
Test Exec Configuration
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/crypto/common.h"

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
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "5");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/db path");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 2, "/db2");
        hrnCfgArgRawBool(argList, cfgOptLogSubprocess, true);
        hrnCfgArgRawBool(argList, cfgOptConfig, false);
        hrnCfgArgRawReset(argList, cfgOptNeutralUmask);
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);

        // Set repo1-cipher-pass to make sure it is not passed on the command line
        hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .noStd = true);
        hrnCfgEnvRemoveRaw(cfgOptRepoCipherPass);

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
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/db path");
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "1");
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "2");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "a=b");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "c=d");
        hrnCfgArgRawReset(argList, cfgOptLogPath);

        hrnCfgEnvRawZ(cfgOptRepoHost, "bogus");
        HRN_CFG_LOAD(cfgCmdRestore, argList, .noStd = true);
        hrnCfgEnvRemoveRaw(cfgOptRepoHost);

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
