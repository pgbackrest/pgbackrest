/***********************************************************************************************************************************
Test Configuration Parse
***********************************************************************************************************************************/

#define TEST_BACKREST_EXE                                           "pgbackrest"

#define TEST_COMMAND_HELP                                           "help"
#define TEST_COMMAND_VERSION                                        "version"

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
void testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("configParse()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), "exe only");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNew(TEST_COMMAND_VERSION));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), TEST_COMMAND_VERSION " command");
        TEST_RESULT_INT(cfgCommand(), cfgCmdVersion, "command is " TEST_COMMAND_VERSION);

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNew("--perl-option=value"));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), "perl option");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNewFmt("--%s", cfgOptionName(cfgOptDelta)));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), "delta option");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNew("--" BOGUS_STR));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError, "invalid option '--BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--no-archive-check"));
        strLstAdd(argList, strNew(TEST_COMMAND_HELP));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), TEST_COMMAND_HELP " command");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(BOGUS_STR));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList)), CommandInvalidError, "invalid command 'BOGUS'");
    }
}
