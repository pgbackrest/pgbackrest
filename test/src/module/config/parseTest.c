/***********************************************************************************************************************************
Test Configuration Parse
***********************************************************************************************************************************/

#define TEST_BACKREST_EXE                                           "pgbackrest"

#define TEST_COMMAND_ARCHIVE_GET                                    "archive-get"
#define TEST_COMMAND_BACKUP                                         "backup"
#define TEST_COMMAND_HELP                                           "help"
#define TEST_COMMAND_RESTORE                                        "restore"
#define TEST_COMMAND_VERSION                                        "version"

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
void testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("configParseArg()"))
    {
        ParseData *parseData = NULL;

        StringList *argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        TEST_ASSIGN(parseData, configParseArg(strLstSize(argList), strLstPtr(argList)), "no command, no args - set help command");
        TEST_RESULT_INT(parseData->command, cfgCmdHelp, "    command is " TEST_COMMAND_HELP);

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNew("--online"));
        TEST_ERROR(configParseArg(strLstSize(argList), strLstPtr(argList)), CommandRequiredError, "no command found");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNew(TEST_COMMAND_VERSION));
        TEST_ASSIGN(parseData, configParseArg(strLstSize(argList), strLstPtr(argList)), TEST_COMMAND_VERSION " command");
        TEST_RESULT_INT(parseData->command, cfgCmdVersion, "    command is " TEST_COMMAND_VERSION);
        TEST_RESULT_PTR(parseData->perlOptionList, NULL, "    no perl options");
        TEST_RESULT_PTR(parseData->commandArgList, NULL, "    no command arguments");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNew("--perl-option=value"));
        TEST_ASSIGN(parseData, configParseArg(strLstSize(argList), strLstPtr(argList)), "perl option");
        TEST_RESULT_STR(strPtr(strLstJoin(parseData->perlOptionList, ",")), "value", "check perl option");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNewFmt("--%s", cfgOptionName(cfgOptDelta)));
        TEST_ASSIGN(parseData, configParseArg(strLstSize(argList), strLstPtr(argList)), "delta option");
        TEST_RESULT_BOOL(parseData->parseOptionList[cfgOptDelta].found, true, "    option found");
        TEST_RESULT_BOOL(parseData->parseOptionList[cfgOptDelta].negate, false, "    option not negated");
        TEST_RESULT_PTR(parseData->parseOptionList[cfgOptDelta].valueList, NULL, "    no values");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNew("--" BOGUS_STR));
        TEST_ERROR(configParseArg(strLstSize(argList), strLstPtr(argList)), OptionInvalidError, "invalid option '--BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--no-archive-check"));
        strLstAdd(argList, strNew(TEST_COMMAND_HELP));
        TEST_ASSIGN(parseData, configParseArg(strLstSize(argList), strLstPtr(argList)), TEST_COMMAND_HELP " command");
        TEST_RESULT_BOOL(parseData->parseOptionList[cfgOptArchiveCheck].negate, true, "    option negated");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(BOGUS_STR));
        TEST_ERROR(configParseArg(strLstSize(argList), strLstPtr(argList)), CommandInvalidError, "invalid command 'BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--no-online"));
        strLstAdd(argList, strNew("--no-online"));
        TEST_ERROR(
            configParseArg(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "option 'online' is negated multiple times");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-host"));
        TEST_ERROR(
            configParseArg(strLstSize(argList), strLstPtr(argList)), OptionInvalidError, "option '--db-host' requires argument");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--config=/etc/config"));
        strLstAdd(argList, strNew("--no-config"));
        TEST_ERROR(
            configParseArg(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "option 'config' cannot be set and negated");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--compress-level=3"));
        strLstAdd(argList, strNew("--compress-level=3"));
        TEST_ERROR(
            configParseArg(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "option 'compress-level' cannot have multiple arguments");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_ARCHIVE_GET));
        strLstAdd(argList, strNew("000000010000000200000003"));
        strLstAdd(argList, strNew("/path/to/wal/RECOVERYWAL"));
        TEST_ASSIGN(parseData, configParseArg(strLstSize(argList), strLstPtr(argList)), "command arguments");
        TEST_RESULT_STR(
            strPtr(strLstJoin(parseData->commandArgList, "|")), "000000010000000200000003|/path/to/wal/RECOVERYWAL",
            "    check command arguments");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-host=db1.test.org"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ASSIGN(parseData, configParseArg(strLstSize(argList), strLstPtr(argList)), "single valid option");
        TEST_RESULT_STR(
            strPtr(strLstJoin(parseData->parseOptionList[cfgOptDbHost].valueList, "|")), "db1.test.org", "    check db-host option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--link-map=ts1=/path/to/ts1"));
        strLstAdd(argList, strNew("--link-map=ts2=/path/to/ts2"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ASSIGN(parseData, configParseArg(strLstSize(argList), strLstPtr(argList)), "multiple valid options");
        TEST_RESULT_STR(
            strPtr(strLstJoin(parseData->parseOptionList[cfgOptLinkMap].valueList, "|")), "ts1=/path/to/ts1|ts2=/path/to/ts2",
            "    check link-map options");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("configParse()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_HELP));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), TEST_COMMAND_HELP " command");
        TEST_RESULT_INT(cfgCommand(), cfgCmdHelp, "    command is " TEST_COMMAND_HELP);
    }
}
