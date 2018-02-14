/***********************************************************************************************************************************
Test Perl Exec
***********************************************************************************************************************************/
#include "config/config.h"
#include "config/load.h"

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
        cfgCommandSet(cfgCmdInfo);
        cfgExeSet(strNew("/path/to/pgbackrest"));
        cfgOptionSet(cfgOptPerlBin, cfgSourceParam, varNewStrZ("/usr/bin/perl"));

        TEST_RESULT_STR(
            strPtr(strLstJoin(perlCommand(), "|")),
            "/usr/bin/perl|-MpgBackRest::Main|-e|pgBackRest::Main::main('/path/to/pgbackrest','info','{}')|[NULL]",
            "custom command with no options");

        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(cfgOptPerlBin, cfgSourceParam, NULL);

        cfgOptionValidSet(cfgOptCompress, true);
        cfgOptionSet(cfgOptCompress, cfgSourceParam, varNewBool(true));

        StringList *commandParamList = strLstNew();
        strLstAdd(commandParamList, strNew("A"));
        strLstAdd(commandParamList, strNew("B"));
        cfgCommandParamSet(commandParamList);

        cfgOptionValidSet(cfgOptPerlOption, true);
        StringList *perlList = strLstNew();
        strLstAdd(perlList, strNew("-I."));
        strLstAdd(perlList, strNew("-MDevel::Cover=-silent,1"));
        cfgOptionSet(cfgOptPerlOption, cfgSourceParam, varNewVarLst(varLstNewStrLst(perlList)));

        TEST_RESULT_STR(
            strPtr(strLstJoin(perlCommand(), "|")),
            "/usr/bin/env|perl|-I.|-MDevel::Cover=-silent,1|-MpgBackRest::Main|-e|pgBackRest::Main::main("
            "'/path/to/pgbackrest','info','{"
            "\"compress\":{\"source\":\"param\",\"value\":true},"
            "\"perl-option\":{\"source\":\"param\",\"value\":{\"-I.\":true,\"-MDevel::Cover=-silent,1\":true}}"
            "}','A','B')|[NULL]",
            "command with one option and params");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("perlExec()"))
    {
        StringList *param = strLstAdd(strLstAdd(strLstNew(), strNew(BOGUS_STR)), NULL);

        TEST_ERROR(perlExec(param), AssertError, "unable to exec BOGUS: No such file or directory");
    }
}
