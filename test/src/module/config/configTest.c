/***********************************************************************************************************************************
Test Configuration Commands and Options
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
void testRun()
{
    // Static tests against known values -- these may break as options change so will need to be kept up to date.  The tests have
    // generally been selected to favor values that are not expected to change but adjustments are welcome as long as the type of
    // test is not drastically changed.
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("check known values"))
    {
        // Generate standard error messages
        char optionDefIdInvalidHighError[256];
        snprintf(
            optionDefIdInvalidHighError, sizeof(optionDefIdInvalidHighError), "option def id %d invalid - must be >= 0 and < %d",
            cfgDefOptionTotal(), cfgDefOptionTotal());

        char optionIdInvalidHighError[256];
        snprintf(
            optionIdInvalidHighError, sizeof(optionIdInvalidHighError), "option id %d invalid - must be >= 0 and < %d",
            CFG_OPTION_TOTAL, CFG_OPTION_TOTAL);

        char optionIdInvalidLowError[256];
        snprintf(
            optionIdInvalidLowError, sizeof(optionIdInvalidLowError), "option id -1 invalid - must be >= 0 and < %d",
            CFG_OPTION_TOTAL);

        char commandIdInvalidHighError[256];
        snprintf(
            commandIdInvalidHighError, sizeof(commandIdInvalidHighError), "command id %d invalid - must be >= 0 and < %d",
            CFG_COMMAND_TOTAL, CFG_COMMAND_TOTAL);

        char commandIdInvalidLowError[256];
        snprintf(
            commandIdInvalidLowError, sizeof(commandIdInvalidLowError), "command id -1 invalid - must be >= 0 and < %d",
            CFG_COMMAND_TOTAL);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(cfgCommandId(BOGUS_STR), AssertError, "invalid command 'BOGUS'");
        TEST_RESULT_INT(cfgCommandId("archive-push"), cfgCmdArchivePush, "command id from name");

        TEST_ERROR(cfgCommandDefIdFromId(CFG_COMMAND_TOTAL), AssertError, commandIdInvalidHighError);
        TEST_RESULT_INT(cfgCommandDefIdFromId(cfgCmdBackup), cfgDefCmdBackup, "command id to def id");

        TEST_ERROR(cfgCommandName(-1), AssertError, commandIdInvalidLowError);
        TEST_RESULT_STR(cfgCommandName(cfgCmdBackup), "backup", "command name from id");

        TEST_RESULT_INT(cfgOptionDefIdFromId(cfgOptDbHost + 6), cfgDefOptDbHost, "option id to def id");

        TEST_RESULT_INT(cfgOptionId("target"), cfgOptTarget, "option id from name");
        TEST_RESULT_INT(cfgOptionId(BOGUS_STR), -1, "option id from invalid option name");

        TEST_ERROR(cfgOptionIdFromDefId(cfgDefOptionTotal(), 6), AssertError, optionDefIdInvalidHighError);
        TEST_RESULT_INT(cfgOptionIdFromDefId(cfgDefOptDbHost, 6), cfgOptDbHost + 6, "option def id to id");

        TEST_ERROR(cfgOptionIndex(CFG_OPTION_TOTAL), AssertError, optionIdInvalidHighError);
        TEST_RESULT_INT(cfgOptionIndex(cfgOptDbCmd + 6), 6, "option index");
        TEST_RESULT_INT(cfgOptionIndex(cfgOptCompressLevel), 0, "option index");

        TEST_RESULT_INT(cfgOptionIndexTotal(cfgOptDbPath), 8, "option index total");
        TEST_RESULT_INT(cfgOptionIndexTotal(cfgOptLogLevelConsole), 1, "option index total");

        TEST_ERROR(cfgOptionName(-1), AssertError, optionIdInvalidLowError);
        TEST_RESULT_STR(cfgOptionName(cfgOptBackupStandby), "backup-standby", "option id from name");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("configuration"))
    {
        TEST_RESULT_INT(cfgCommand(), cfgCmdNone, "command begins as none");
        TEST_RESULT_VOID(cfgCommandSet(cfgCmdBackup), "command set to backup");
        TEST_RESULT_INT(cfgCommand(), cfgCmdBackup, "command is backup");
    }
}
