/***********************************************************************************************************************************
Test Perl Exec
***********************************************************************************************************************************/
#include "config/config.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("perlOptionJson()"))
    {
        cfgInit();
        TEST_RESULT_STR(strPtr(perlOptionJson()), "{}", "no options");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();

        cfgOptionValidSet(cfgOptCompress, true);
        cfgOptionSet(cfgOptCompress, cfgSourceParam, varNewBool(true));

        cfgOptionValidSet(cfgOptConfig, true);
        cfgOptionNegateSet(cfgOptConfig, true);
        cfgOptionSet(cfgOptConfig, cfgSourceParam, NULL);

        cfgOptionValidSet(cfgOptOnline, true);
        cfgOptionNegateSet(cfgOptOnline, true);
        cfgOptionSet(cfgOptOnline, cfgSourceParam, varNewBool(false));

        cfgOptionValidSet(cfgOptPgHost, true);
        cfgOptionResetSet(cfgOptPgHost, true);

        cfgOptionValidSet(cfgOptPgPath, true);
        cfgOptionSet(cfgOptPgPath, cfgSourceConfig, varNewStr(strNew("/path/db/pg")));

        cfgOptionValidSet(cfgOptBackupStandby, true);
        cfgOptionResetSet(cfgOptBackupStandby, true);
        cfgOptionSet(cfgOptBackupStandby, cfgSourceParam, varNewBool(false));

        cfgOptionValidSet(cfgOptProtocolTimeout, true);
        cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, varNewDbl(1.1));

        cfgOptionValidSet(cfgOptArchivePushQueueMax, true);
        cfgOptionSet(cfgOptArchivePushQueueMax, cfgSourceParam, varNewInt64(999999999999));

        cfgOptionValidSet(cfgOptRepoCipherPass, true);
        cfgOptionSet(cfgOptRepoCipherPass, cfgSourceConfig, varNewStr(strNew("part1\npart2")));

        cfgOptionValidSet(cfgOptCompressLevel, true);
        cfgOptionSet(cfgOptCompressLevel, cfgSourceConfig, varNewInt(3));

        cfgOptionValidSet(cfgOptStanza, true);
        cfgOptionSet(cfgOptStanza, cfgSourceDefault, varNewStr(strNew("db")));

        TEST_RESULT_STR(
            strPtr(perlOptionJson()),
            "{"
            "\"archive-push-queue-max\":"
                "{\"negate\":false,\"reset\":false,\"source\":\"param\",\"valid\":true,\"value\":999999999999},"
            "\"backup-standby\":{\"negate\":false,\"reset\":true,\"source\":\"param\",\"valid\":true,\"value\":false},"
            "\"compress\":{\"negate\":false,\"reset\":false,\"source\":\"param\",\"valid\":true,\"value\":true},"
            "\"compress-level\":{\"negate\":false,\"reset\":false,\"source\":\"config\",\"valid\":true,\"value\":3},"
            "\"config\":{\"negate\":true,\"reset\":false,\"source\":\"param\",\"valid\":true},"
            "\"online\":{\"negate\":true,\"reset\":false,\"source\":\"param\",\"valid\":true,\"value\":false},"
            "\"pg1-host\":{\"negate\":false,\"reset\":true,\"source\":\"default\",\"valid\":true},"
            "\"pg1-path\":{\"negate\":false,\"reset\":false,\"source\":\"config\",\"valid\":true,\"value\":\"\\/path\\/db\\/pg\"},"
            "\"protocol-timeout\":{\"negate\":false,\"reset\":false,\"source\":\"param\",\"valid\":true,\"value\":1.1},"
            "\"repo1-cipher-pass\":{\"negate\":false,\"reset\":false,\"source\":\"config\",\"valid\":true,"
                "\"value\":\"part1\\npart2\"},"
            "\"stanza\":{\"negate\":false,\"reset\":false,\"source\":\"default\",\"valid\":true,\"value\":\"db\"}"
            "}",
            "simple options");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();

        cfgOptionValidSet(cfgOptDbInclude, true);
        StringList *includeList = strLstNew();
        strLstAdd(includeList, strNew("db1"));
        strLstAdd(includeList, strNew("db2"));
        cfgOptionSet(cfgOptDbInclude, cfgSourceParam, varNewVarLst(varLstNewStrLst(includeList)));

        cfgOptionValidSet(cfgOptRecoveryOption, true);

        Variant *recoveryVar = varNewKv();
        KeyValue *recoveryKv = varKv(recoveryVar);
        kvPut(recoveryKv, varNewStr(strNew("standby_mode")), varNewStr(strNew("on")));
        kvPut(recoveryKv, varNewStr(strNew("primary_conn_info")), varNewStr(strNew("blah")));
        cfgOptionSet(cfgOptRecoveryOption, cfgSourceParam, recoveryVar);

        StringList *commandParamList = strLstNew();
        strLstAdd(commandParamList, strNew("param1"));
        strLstAdd(commandParamList, strNew("param2"));
        cfgCommandParamSet(commandParamList);

        cfgOptionValidSet(cfgOptPerlOption, true);
        StringList *perlList = strLstNew();
        strLstAdd(perlList, strNew("-I."));
        strLstAdd(perlList, strNew("-MDevel::Cover=-silent,1"));
        cfgOptionSet(cfgOptPerlOption, cfgSourceParam, varNewVarLst(varLstNewStrLst(perlList)));

        TEST_RESULT_STR(
            strPtr(perlOptionJson()),
            "{"
            "\"db-include\":{\"negate\":false,\"reset\":false,\"source\":\"param\",\"valid\":true,"
                "\"value\":{\"db1\":true,\"db2\":true}},"
            "\"perl-option\":{\"negate\":false,\"reset\":false,\"source\":\"param\",\"valid\":true,"
                "\"value\":{\"-I.\":true,\"-MDevel::Cover=-silent,1\":true}},"
            "\"recovery-option\":{\"negate\":false,\"reset\":false,\"source\":\"param\",\"valid\":true,"
                "\"value\":{\"primary_conn_info\":\"blah\",\"standby_mode\":\"on\"}}"
            "}",
            "complex options");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
