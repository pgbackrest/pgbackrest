/***********************************************************************************************************************************
Test Configuration Command and Option Definition
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
        char optionIdInvalidHighError[256];
        snprintf(
            optionIdInvalidHighError, sizeof(optionIdInvalidHighError), "option def id %d invalid - must be >= 0 and < %d",
            cfgDefOptionTotal(), cfgDefOptionTotal());

        char optionIdInvalidLowError[256];
        snprintf(
            optionIdInvalidLowError, sizeof(optionIdInvalidLowError), "option def id -1 invalid - must be >= 0 and < %d",
            cfgDefOptionTotal());

        char commandIdInvalidHighError[256];
        snprintf(
            commandIdInvalidHighError, sizeof(commandIdInvalidHighError), "command def id %d invalid - must be >= 0 and < %d",
            cfgDefCommandTotal(), cfgDefCommandTotal());

        char commandIdInvalidLowError[256];
        snprintf(
            commandIdInvalidLowError, sizeof(commandIdInvalidLowError), "command def id -1 invalid - must be >= 0 and < %d",
            cfgDefCommandTotal());

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(cfgDefOptionName(cfgDefOptConfig), "config", "option name");
        TEST_ERROR(cfgDefOptionName(-1), AssertError, optionIdInvalidLowError);

        TEST_RESULT_BOOL(cfgDefOptionAllowList(cfgDefCmdBackup, cfgDefOptLogLevelConsole), true, "allow list valid");
        TEST_RESULT_BOOL(cfgDefOptionAllowList(cfgDefCmdBackup, cfgDefOptDbHost), false, "allow list not valid");
        TEST_RESULT_BOOL(cfgDefOptionAllowList(cfgDefCmdBackup, cfgDefOptType), true, "command allow list valid");

        TEST_RESULT_INT(cfgDefOptionAllowListValueTotal(cfgDefCmdBackup, cfgDefOptType), 3, "allow list total");

        TEST_RESULT_STR(cfgDefOptionAllowListValue(cfgDefCmdBackup, cfgDefOptType, 0), "full", "allow list value 0");
        TEST_RESULT_STR(cfgDefOptionAllowListValue(cfgDefCmdBackup, cfgDefOptType, 1), "diff", "allow list value 1");
        TEST_RESULT_STR(cfgDefOptionAllowListValue(cfgDefCmdBackup, cfgDefOptType, 2), "incr", "allow list value 2");
        TEST_ERROR(
            cfgDefOptionAllowListValue(cfgDefCmdBackup, cfgDefOptType, 3), AssertError,
            "value id 3 invalid - must be >= 0 and < 3");

        TEST_RESULT_BOOL(
            cfgDefOptionAllowListValueValid(cfgDefCmdBackup, cfgDefOptType, "diff"), true, "allow list value valid");
        TEST_RESULT_BOOL(
            cfgDefOptionAllowListValueValid(cfgDefCmdBackup, cfgDefOptType, BOGUS_STR), false, "allow list value not valid");

        TEST_RESULT_BOOL(cfgDefOptionAllowRange(cfgDefCmdBackup, cfgDefOptCompressLevel), true, "range allowed");
        TEST_RESULT_BOOL(cfgDefOptionAllowRange(cfgDefCmdBackup, cfgDefOptBackupHost), false, "range not allowed");

        TEST_RESULT_DOUBLE(cfgDefOptionAllowRangeMin(cfgDefCmdBackup, cfgDefOptDbTimeout), 0.1, "range min");
        TEST_RESULT_DOUBLE(cfgDefOptionAllowRangeMax(cfgDefCmdBackup, cfgDefOptCompressLevel), 9, "range max");

        TEST_ERROR(cfgDefOptionDefault(-1, cfgDefOptCompressLevel), AssertError, commandIdInvalidLowError);
        TEST_ERROR(cfgDefOptionDefault(cfgDefCmdBackup, cfgDefOptionTotal()), AssertError, optionIdInvalidHighError);
        TEST_RESULT_STR(cfgDefOptionDefault(cfgDefCmdBackup, cfgDefOptCompressLevel), "6", "option default exists");
        TEST_RESULT_STR(cfgDefOptionDefault(cfgDefCmdRestore, cfgDefOptType), "default", "command default exists");
        TEST_RESULT_STR(cfgDefOptionDefault(cfgDefCmdLocal, cfgDefOptType), NULL, "command default does not exist");
        TEST_RESULT_STR(cfgDefOptionDefault(cfgDefCmdBackup, cfgDefOptBackupHost), NULL, "default does not exist");

        TEST_RESULT_BOOL(cfgDefOptionDepend(cfgDefCmdRestore, cfgDefOptRepoS3Key), true, "has depend option");
        TEST_RESULT_BOOL(cfgDefOptionDepend(cfgDefCmdRestore, cfgDefOptType), false, "does not have depend option");

        TEST_RESULT_INT(cfgDefOptionDependOption(cfgDefCmdBackup, cfgDefOptDbUser), cfgDefOptDbHost, "depend option id");
        TEST_RESULT_INT(cfgDefOptionDependOption(cfgDefCmdBackup, cfgDefOptBackupCmd), cfgDefOptBackupHost, "depend option id");

        TEST_RESULT_INT(cfgDefOptionDependValueTotal(cfgDefCmdRestore, cfgDefOptTarget), 3, "depend option value total");
        TEST_RESULT_STR(cfgDefOptionDependValue(cfgDefCmdRestore, cfgDefOptTarget, 0), "name", "depend option value 0");
        TEST_RESULT_STR(cfgDefOptionDependValue(cfgDefCmdRestore, cfgDefOptTarget, 1), "time", "depend option value 1");
        TEST_RESULT_STR(cfgDefOptionDependValue(cfgDefCmdRestore, cfgDefOptTarget, 2), "xid", "depend option value 2");
        TEST_ERROR(
            cfgDefOptionDependValue(cfgDefCmdRestore, cfgDefOptTarget, 3), AssertError,
            "value id 3 invalid - must be >= 0 and < 3");

        TEST_RESULT_BOOL(
                cfgDefOptionDependValueValid(cfgDefCmdRestore, cfgDefOptTarget, "time"), true, "depend option value valid");
        TEST_RESULT_BOOL(
            cfgDefOptionDependValueValid(cfgDefCmdRestore, cfgDefOptTarget, BOGUS_STR), false, "depend option value not valid");

        TEST_ERROR(cfgDefOptionIndexTotal(cfgDefOptionTotal()), AssertError, optionIdInvalidHighError);
        TEST_RESULT_INT(cfgDefOptionIndexTotal(cfgDefOptDbPath), 8, "index total > 1");
        TEST_RESULT_INT(cfgDefOptionIndexTotal(cfgDefOptRepoPath), 1, "index total == 1");

        TEST_RESULT_STR(cfgDefOptionNameAlt(cfgDefOptProcessMax), "thread-max", "alt name");
        TEST_RESULT_STR(cfgDefOptionNameAlt(cfgDefOptType), NULL, "no alt name");

        TEST_ERROR(cfgDefOptionNegate(cfgDefOptionTotal()), AssertError, optionIdInvalidHighError);
        TEST_RESULT_BOOL(cfgDefOptionNegate(cfgDefOptOnline), true, "option can be negated");
        TEST_RESULT_BOOL(cfgDefOptionNegate(cfgDefOptType), false, "option cannot be negated");

        TEST_RESULT_STR(cfgDefOptionPrefix(cfgDefOptDbHost), "db", "option prefix");
        TEST_RESULT_STR(cfgDefOptionPrefix(cfgDefOptType), NULL, "option has no prefix");

        TEST_RESULT_BOOL(cfgDefOptionRequired(cfgDefCmdBackup, cfgDefOptConfig), true, "option required");
        TEST_RESULT_BOOL(cfgDefOptionRequired(cfgDefCmdRestore, cfgDefOptBackupHost), false, "option not required");
        TEST_RESULT_BOOL(cfgDefOptionRequired(cfgDefCmdInfo, cfgDefOptStanza), false, "command option not required");

        TEST_RESULT_INT(cfgDefOptionSection(cfgDefOptRepoS3Key), cfgDefSectionGlobal, "global section");
        TEST_RESULT_INT(cfgDefOptionSection(cfgDefOptDbPath), cfgDefSectionStanza, "stanza section");
        TEST_RESULT_INT(cfgDefOptionSection(cfgDefOptType), cfgDefSectionCommandLine, "command line only");

        TEST_ERROR(cfgDefOptionSecure(-1), AssertError, optionIdInvalidLowError);
        TEST_RESULT_BOOL(cfgDefOptionSecure(cfgDefOptRepoS3Key), true, "option secure");
        TEST_RESULT_BOOL(cfgDefOptionSecure(cfgDefOptBackupHost), false, "option not secure");

        TEST_ERROR(cfgDefOptionType(-1), AssertError, optionIdInvalidLowError);
        TEST_RESULT_INT(cfgDefOptionType(cfgDefOptType), cfgDefOptTypeString, "string type");
        TEST_RESULT_INT(cfgDefOptionType(cfgDefOptCompress), cfgDefOptTypeBoolean, "boolean type");

        TEST_ERROR(cfgDefOptionValid(cfgDefCmdInfo, cfgDefOptionTotal()), AssertError, optionIdInvalidHighError);
        TEST_RESULT_BOOL(cfgDefOptionValid(cfgDefCmdBackup, cfgDefOptType), true, "option valid");
        TEST_RESULT_BOOL(cfgDefOptionValid(cfgDefCmdInfo, cfgDefOptType), false, "option not valid");
    }
}
