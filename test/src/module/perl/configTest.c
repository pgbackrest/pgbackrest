/***********************************************************************************************************************************
Test Perl Exec
***********************************************************************************************************************************/
#include "config/config.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("perlMain()"))
    {
        cfgInit();
        TEST_RESULT_STR(strPtr(perlOptionJson()), "{}", "no options");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();

        cfgOptionValidSet(cfgOptCompress, true);
        cfgOptionSet(cfgOptCompress, cfgSourceParam, varNewBool(true));

        cfgOptionValidSet(cfgOptOnline, true);
        cfgOptionNegateSet(cfgOptOnline, true);
        cfgOptionSet(cfgOptOnline, cfgSourceParam, varNewBool(false));

        cfgOptionValidSet(cfgOptPgHost, true);
        cfgOptionResetSet(cfgOptPgHost, true);

        cfgOptionValidSet(cfgOptBackupStandby, true);
        cfgOptionResetSet(cfgOptBackupStandby, true);
        cfgOptionSet(cfgOptBackupStandby, cfgSourceDefault, varNewBool(false));

        cfgOptionValidSet(cfgOptProtocolTimeout, true);
        cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, varNewDbl(1.1));

        cfgOptionValidSet(cfgOptArchiveQueueMax, true);
        cfgOptionSet(cfgOptArchiveQueueMax, cfgSourceParam, varNewInt64(999999999999));

        cfgOptionValidSet(cfgOptCompressLevel, true);
        cfgOptionSet(cfgOptCompressLevel, cfgSourceConfig, varNewInt(3));

        cfgOptionValidSet(cfgOptStanza, true);
        cfgOptionSet(cfgOptStanza, cfgSourceDefault, varNewStr(strNew("db")));

        TEST_RESULT_STR(
            strPtr(perlOptionJson()),
            "{"
            "\"archive-queue-max\":{\"source\":\"param\",\"value\":999999999999},"
            "\"backup-standby\":{\"reset\":true,\"value\":false},"
            "\"compress\":{\"source\":\"param\",\"value\":true},"
            "\"compress-level\":{\"source\":\"config\",\"value\":3},"
            "\"online\":{\"source\":\"param\",\"negate\":true},"
            "\"pg1-host\":{\"reset\":true},"
            "\"protocol-timeout\":{\"source\":\"param\",\"value\":1.1},"
            "\"stanza\":{\"value\":\"db\"}"
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
        // !!! WHY DO WE STILL NEED TO CREATE THE VAR KV EMPTY?
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
            "\"db-include\":{\"source\":\"param\",\"value\":{\"db1\":true,\"db2\":true}},"
            "\"perl-option\":{\"source\":\"param\",\"value\":{\"-I.\":true,\"-MDevel::Cover=-silent,1\":true}},"
            "\"recovery-option\":{\"source\":\"param\",\"value\":{\"standby_mode\":\"on\",\"primary_conn_info\":\"blah\"}}"
            "}",
            "complex options");
    }
}
