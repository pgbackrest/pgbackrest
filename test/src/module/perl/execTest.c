/***********************************************************************************************************************************
Test Perl Exec
***********************************************************************************************************************************/
#include "config/config.h"

#define TEST_ENV_EXE                                                "/usr/bin/env"
#define TEST_PERL_EXE                                               "perl"
#define TEST_BACKREST_EXE                                           "/path/to/pgbackrest"
#define TEST_PERL_MAIN                                                                                                             \
    "-MpgBackRest::Main|-e|pgBackRest::Main::main('" TEST_BACKREST_EXE ""

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("perlCommand()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandSet(cfgCmdInfo);
        cfgExeSet(strNew(TEST_BACKREST_EXE));
        cfgOptionSet(cfgOptPerlBin, cfgSourceParam, varNewStrZ("/usr/bin/perl"));

        TEST_RESULT_STR(
            strPtr(strLstJoin(perlCommand(), "|")),
            "/usr/bin/perl|" TEST_PERL_MAIN "', 'info')|[NULL]", "custom command with no options");

        cfgOptionSet(cfgOptPerlBin, cfgSourceParam, NULL);

        TEST_RESULT_STR(
            strPtr(strLstJoin(perlCommand(), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|" TEST_PERL_MAIN "', 'info')|[NULL]", "command with no options");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandSet(cfgCmdBackup);
        cfgExeSet(strNew(TEST_BACKREST_EXE));

        cfgOptionValidSet(cfgOptCompress, true);
        cfgOptionSet(cfgOptCompress, cfgSourceParam, varNewBool(true));

        cfgOptionValidSet(cfgOptOnline, true);
        cfgOptionNegateSet(cfgOptOnline, true);
        cfgOptionSet(cfgOptOnline, cfgSourceParam, varNewBool(false));

        cfgOptionValidSet(cfgOptProtocolTimeout, true);
        cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, varNewDbl(1.1));

        cfgOptionValidSet(cfgOptCompressLevel, true);
        cfgOptionSet(cfgOptCompressLevel, cfgSourceParam, varNewInt(3));

        cfgOptionValidSet(cfgOptStanza, true);
        cfgOptionSet(cfgOptStanza, cfgSourceParam, varNewStr(strNew("db")));

        TEST_RESULT_STR(
            strPtr(strLstJoin(perlCommand(), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|" TEST_PERL_MAIN "'"
            ", '--compress', '--compress-level', '3', '--no-online', '--protocol-timeout', '1.1', '--stanza', 'db'"
            ", 'backup')|[NULL]", "simple options");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandHelpSet(true);
        cfgCommandSet(cfgCmdRestore);
        cfgExeSet(strNew(TEST_BACKREST_EXE));

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
        // !!! WHY DO WE STILL NEED TO CREATE THE VAR KV EMPTY?
        StringList *perlList = strLstNew();
        strLstAdd(perlList, strNew("-I."));
        strLstAdd(perlList, strNew("-MDevel::Cover=-silent,1"));
        cfgOptionSet(cfgOptPerlOption, cfgSourceParam, varNewVarLst(varLstNewStrLst(perlList)));

        TEST_RESULT_STR(
            strPtr(strLstJoin(perlCommand(), "|")),
            TEST_ENV_EXE "|" TEST_PERL_EXE "|-I.|-MDevel::Cover=-silent,1|" TEST_PERL_MAIN "'"
            ", '--db-include', 'db1', '--db-include', 'db2'"
            ", '--perl-option', '-I.', '--perl-option', '-MDevel::Cover=-silent,1'"
            ", '--recovery-option', 'standby_mode=on', '--recovery-option', 'primary_conn_info=blah'"
            ", 'help', 'restore', 'param1', 'param2')|[NULL]", "complex options");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("perlExec()"))
    {
        StringList *param = strLstAdd(strLstAdd(strLstNew(), strNew(BOGUS_STR)), NULL);

        TEST_ERROR(perlExec(param), AssertError, "unable to exec BOGUS: No such file or directory");
    }
}
