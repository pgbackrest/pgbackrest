/***********************************************************************************************************************************
Test Configuration Command and Option Definition
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Static tests against known values -- these may break as options change so will need to be kept up to date.  The tests have
    // generally been selected to favor values that are not expected to change but adjustments are welcome as long as the type of
    // test is not drastically changed.
    // *****************************************************************************************************************************
    if (testBegin("check known values"))
    {
        TEST_RESULT_BOOL(cfgDefOptionAllowList(cfgCmdBackup, cfgOptLogLevelConsole), true, "allow list valid");
        TEST_RESULT_BOOL(cfgDefOptionAllowList(cfgCmdBackup, cfgOptPgHost), false, "allow list not valid");
        TEST_RESULT_BOOL(cfgDefOptionAllowList(cfgCmdBackup, cfgOptType), true, "command allow list valid");

        TEST_RESULT_INT(cfgDefOptionAllowListValueTotal(cfgCmdBackup, cfgOptChecksumPage), 0, "allow list total = 0");

        TEST_RESULT_INT(cfgDefOptionAllowListValueTotal(cfgCmdBackup, cfgOptType), 3, "allow list total");

        TEST_RESULT_Z(cfgDefOptionAllowListValue(cfgCmdBackup, cfgOptType, 0), "full", "allow list value 0");
        TEST_RESULT_Z(cfgDefOptionAllowListValue(cfgCmdBackup, cfgOptType, 1), "diff", "allow list value 1");
        TEST_RESULT_Z(cfgDefOptionAllowListValue(cfgCmdBackup, cfgOptType, 2), "incr", "allow list value 2");
        TEST_ERROR(
            cfgDefOptionAllowListValue(cfgCmdBackup, cfgOptType, 3), AssertError,
            "assertion 'valueId < cfgDefOptionAllowListValueTotal(commandId, optionId)' failed");

        TEST_RESULT_BOOL(
            cfgDefOptionAllowListValueValid(cfgCmdBackup, cfgOptType, "diff"), true, "allow list value valid");
        TEST_RESULT_BOOL(
            cfgDefOptionAllowListValueValid(cfgCmdBackup, cfgOptType, BOGUS_STR), false, "allow list value not valid");

        TEST_RESULT_BOOL(cfgDefOptionAllowRange(cfgCmdBackup, cfgOptCompressLevel), true, "range allowed");
        TEST_RESULT_BOOL(cfgDefOptionAllowRange(cfgCmdBackup, cfgOptRepoHost), false, "range not allowed");

        TEST_RESULT_INT(cfgDefOptionAllowRangeMin(cfgCmdBackup, cfgOptDbTimeout), 100, "range min");
        TEST_RESULT_INT(cfgDefOptionAllowRangeMax(cfgCmdBackup, cfgOptCompressLevel), 9, "range max");
        TEST_RESULT_INT(cfgDefOptionAllowRangeMin(cfgCmdArchivePush, cfgOptArchivePushQueueMax), 0, "range min");
        TEST_RESULT_INT(cfgDefOptionAllowRangeMax(cfgCmdArchivePush, cfgOptArchivePushQueueMax), 4503599627370496, "range max");

        TEST_ERROR(
            cfgDefOptionDefault(cfgDefCommandTotal(), cfgOptCompressLevel), AssertError,
            "assertion 'commandId < cfgDefCommandTotal()' failed");
        TEST_ERROR(cfgDefOptionDefault(
            cfgCmdBackup, cfgDefOptionTotal()), AssertError,
            "assertion 'optionId < cfgDefOptionTotal()' failed");
        TEST_RESULT_Z(cfgDefOptionDefault(cfgCmdRestore, cfgOptType), "default", "command default exists");
        TEST_RESULT_Z(cfgDefOptionDefault(cfgCmdBackup, cfgOptRepoHost), NULL, "default does not exist");

        TEST_RESULT_BOOL(cfgDefOptionDepend(cfgCmdRestore, cfgOptRepoS3Key), true, "has depend option");
        TEST_RESULT_BOOL(cfgDefOptionDepend(cfgCmdRestore, cfgOptType), false, "does not have depend option");

        TEST_RESULT_INT(cfgDefOptionDependOption(cfgCmdBackup, cfgOptPgHostUser), cfgOptPgHost, "depend option id");
        TEST_RESULT_INT(cfgDefOptionDependOption(cfgCmdBackup, cfgOptRepoHostCmd), cfgOptRepoHost, "depend option id");

        TEST_RESULT_INT(cfgDefOptionDependValueTotal(cfgCmdRestore, cfgOptTarget), 3, "depend option value total");
        TEST_RESULT_Z(cfgDefOptionDependValue(cfgCmdRestore, cfgOptTarget, 0), "name", "depend option value 0");
        TEST_RESULT_Z(cfgDefOptionDependValue(cfgCmdRestore, cfgOptTarget, 1), "time", "depend option value 1");
        TEST_RESULT_Z(cfgDefOptionDependValue(cfgCmdRestore, cfgOptTarget, 2), "xid", "depend option value 2");
        TEST_ERROR(
            cfgDefOptionDependValue(cfgCmdRestore, cfgOptTarget, 3), AssertError,
            "assertion 'valueId < cfgDefOptionDependValueTotal(commandId, optionId)' failed");

        TEST_RESULT_BOOL(
                cfgDefOptionDependValueValid(cfgCmdRestore, cfgOptTarget, "time"), true, "depend option value valid");
        TEST_RESULT_BOOL(
            cfgDefOptionDependValueValid(cfgCmdRestore, cfgOptTarget, BOGUS_STR), false, "depend option value not valid");

        TEST_RESULT_BOOL(cfgDefOptionRequired(cfgCmdBackup, cfgOptConfig), true, "option required");
        TEST_RESULT_BOOL(cfgDefOptionRequired(cfgCmdBackup, cfgOptForce), true, "option required");
        TEST_RESULT_BOOL(cfgDefOptionRequired(cfgCmdRestore, cfgOptRepoHost), false, "option not required");
        TEST_RESULT_BOOL(cfgDefOptionRequired(cfgCmdInfo, cfgOptStanza), false, "command option not required");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
