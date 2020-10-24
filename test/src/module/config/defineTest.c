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
        TEST_RESULT_Z(cfgDefOptionName(cfgDefOptConfig), "config", "option name");

        TEST_RESULT_INT(cfgDefOptionId("repo-host"), cfgDefOptRepoHost, "define id");
        TEST_RESULT_INT(cfgDefOptionId(BOGUS_STR), -1, "invalid define id");

        TEST_RESULT_BOOL(cfgDefOptionAllowList(cfgCmdBackup, cfgDefOptLogLevelConsole), true, "allow list valid");
        TEST_RESULT_BOOL(cfgDefOptionAllowList(cfgCmdBackup, cfgDefOptPgHost), false, "allow list not valid");
        TEST_RESULT_BOOL(cfgDefOptionAllowList(cfgCmdBackup, cfgDefOptType), true, "command allow list valid");

        TEST_RESULT_INT(cfgDefOptionAllowListValueTotal(cfgCmdBackup, cfgDefOptChecksumPage), 0, "allow list total = 0");

        TEST_RESULT_INT(cfgDefOptionAllowListValueTotal(cfgCmdBackup, cfgDefOptType), 3, "allow list total");

        TEST_RESULT_Z(cfgDefOptionAllowListValue(cfgCmdBackup, cfgDefOptType, 0), "full", "allow list value 0");
        TEST_RESULT_Z(cfgDefOptionAllowListValue(cfgCmdBackup, cfgDefOptType, 1), "diff", "allow list value 1");
        TEST_RESULT_Z(cfgDefOptionAllowListValue(cfgCmdBackup, cfgDefOptType, 2), "incr", "allow list value 2");
        TEST_ERROR(
            cfgDefOptionAllowListValue(cfgCmdBackup, cfgDefOptType, 3), AssertError,
            "assertion 'valueId < cfgDefOptionAllowListValueTotal(commandId, optionDefId)' failed");

        TEST_RESULT_BOOL(
            cfgDefOptionAllowListValueValid(cfgCmdBackup, cfgDefOptType, "diff"), true, "allow list value valid");
        TEST_RESULT_BOOL(
            cfgDefOptionAllowListValueValid(cfgCmdBackup, cfgDefOptType, BOGUS_STR), false, "allow list value not valid");

        TEST_RESULT_BOOL(cfgDefOptionAllowRange(cfgCmdBackup, cfgDefOptCompressLevel), true, "range allowed");
        TEST_RESULT_BOOL(cfgDefOptionAllowRange(cfgCmdBackup, cfgDefOptRepoHost), false, "range not allowed");

        TEST_RESULT_DOUBLE(cfgDefOptionAllowRangeMin(cfgCmdBackup, cfgDefOptDbTimeout), 0.1, "range min");
        TEST_RESULT_DOUBLE(cfgDefOptionAllowRangeMax(cfgCmdBackup, cfgDefOptCompressLevel), 9, "range max");
        TEST_RESULT_DOUBLE(cfgDefOptionAllowRangeMin(cfgCmdArchivePush, cfgDefOptArchivePushQueueMax), 0, "range min");
        TEST_RESULT_DOUBLE(
            cfgDefOptionAllowRangeMax(cfgCmdArchivePush, cfgDefOptArchivePushQueueMax), 4503599627370496, "range max");

        TEST_ERROR(
            cfgDefOptionDefault(cfgDefCommandTotal(), cfgDefOptCompressLevel), AssertError,
            "assertion 'commandId < cfgDefCommandTotal()' failed");
        TEST_ERROR(cfgDefOptionDefault(
            cfgCmdBackup, cfgDefOptionTotal()), AssertError,
            "assertion 'optionDefId < cfgDefOptionTotal()' failed");
        TEST_RESULT_Z(cfgDefOptionDefault(cfgCmdRestore, cfgDefOptType), "default", "command default exists");
        TEST_RESULT_Z(cfgDefOptionDefault(cfgCmdBackup, cfgDefOptRepoHost), NULL, "default does not exist");

        TEST_RESULT_BOOL(cfgDefOptionDepend(cfgCmdRestore, cfgDefOptRepoS3Key), true, "has depend option");
        TEST_RESULT_BOOL(cfgDefOptionDepend(cfgCmdRestore, cfgDefOptType), false, "does not have depend option");

        TEST_RESULT_INT(cfgDefOptionDependOption(cfgCmdBackup, cfgDefOptPgHostUser), cfgDefOptPgHost, "depend option id");
        TEST_RESULT_INT(cfgDefOptionDependOption(cfgCmdBackup, cfgDefOptRepoHostCmd), cfgDefOptRepoHost, "depend option id");

        TEST_RESULT_INT(cfgDefOptionDependValueTotal(cfgCmdRestore, cfgDefOptTarget), 3, "depend option value total");
        TEST_RESULT_Z(cfgDefOptionDependValue(cfgCmdRestore, cfgDefOptTarget, 0), "name", "depend option value 0");
        TEST_RESULT_Z(cfgDefOptionDependValue(cfgCmdRestore, cfgDefOptTarget, 1), "time", "depend option value 1");
        TEST_RESULT_Z(cfgDefOptionDependValue(cfgCmdRestore, cfgDefOptTarget, 2), "xid", "depend option value 2");
        TEST_ERROR(
            cfgDefOptionDependValue(cfgCmdRestore, cfgDefOptTarget, 3), AssertError,
            "assertion 'valueId < cfgDefOptionDependValueTotal(commandId, optionDefId)' failed");

        TEST_RESULT_BOOL(
                cfgDefOptionDependValueValid(cfgCmdRestore, cfgDefOptTarget, "time"), true, "depend option value valid");
        TEST_RESULT_BOOL(
            cfgDefOptionDependValueValid(cfgCmdRestore, cfgDefOptTarget, BOGUS_STR), false, "depend option value not valid");

        TEST_ERROR(
            cfgDefOptionIndexTotal(cfgDefOptionTotal()), AssertError,
            "assertion 'optionDefId < cfgDefOptionTotal()' failed");
        TEST_RESULT_INT(cfgDefOptionIndexTotal(cfgDefOptPgPath), 8, "index total > 1");
        TEST_RESULT_INT(cfgDefOptionIndexTotal(cfgDefOptRepoPath), 1, "index total == 1");

        TEST_RESULT_BOOL(cfgDefOptionInternal(cfgCmdRestore, cfgDefOptSet), false, "option set is not internal");
        TEST_RESULT_BOOL(cfgDefOptionInternal(cfgCmdRestore, cfgDefOptPgHost), true, "option pg-host is internal");

        TEST_RESULT_BOOL(cfgDefOptionMulti(cfgDefOptRecoveryOption), true, "recovery-option is multi");
        TEST_RESULT_BOOL(cfgDefOptionMulti(cfgDefOptDbInclude), true, "db-include is multi");
        TEST_RESULT_BOOL(cfgDefOptionMulti(cfgDefOptStartFast), false, "start-fast is not multi");

        TEST_RESULT_BOOL(cfgDefOptionRequired(cfgCmdBackup, cfgDefOptConfig), true, "option required");
        TEST_RESULT_BOOL(cfgDefOptionRequired(cfgCmdRestore, cfgDefOptRepoHost), false, "option not required");
        TEST_RESULT_BOOL(cfgDefOptionRequired(cfgCmdInfo, cfgDefOptStanza), false, "command option not required");

        TEST_RESULT_INT(cfgDefOptionSection(cfgDefOptRepoS3Key), cfgDefSectionGlobal, "global section");
        TEST_RESULT_INT(cfgDefOptionSection(cfgDefOptPgPath), cfgDefSectionStanza, "stanza section");
        TEST_RESULT_INT(cfgDefOptionSection(cfgDefOptType), cfgDefSectionCommandLine, "command line only");

        TEST_RESULT_BOOL(cfgDefOptionSecure(cfgDefOptRepoS3Key), true, "option secure");
        TEST_RESULT_BOOL(cfgDefOptionSecure(cfgDefOptRepoHost), false, "option not secure");

        TEST_RESULT_INT(cfgDefOptionType(cfgDefOptType), cfgDefOptTypeString, "string type");
        TEST_RESULT_INT(cfgDefOptionType(cfgDefOptDelta), cfgDefOptTypeBoolean, "boolean type");

        TEST_ERROR(
            cfgDefOptionValid(cfgCmdInfo, cfgDefOptionTotal()), AssertError,
            "assertion 'optionDefId < cfgDefOptionTotal()' failed");
        TEST_RESULT_BOOL(cfgDefOptionValid(cfgCmdBackup, cfgDefOptType), true, "option valid");
        TEST_RESULT_BOOL(cfgDefOptionValid(cfgCmdInfo, cfgDefOptType), false, "option not valid");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgDefCommandHelp*() and cfgDefOptionHelp*()"))
    {
        TEST_RESULT_BOOL(cfgDefOptionHelpNameAlt(cfgDefOptRepoHost), true, "name alt exists");
        TEST_RESULT_BOOL(cfgDefOptionHelpNameAlt(cfgDefOptSet), false, "name alt not exists");
        TEST_RESULT_INT(cfgDefOptionHelpNameAltValueTotal(cfgDefOptRepoHost), 1, "name alt value total");
        TEST_RESULT_Z(cfgDefOptionHelpNameAltValue(cfgDefOptRepoHost, 0), "backup-host", "name alt value 0");
        TEST_ERROR(
            cfgDefOptionHelpNameAltValue(cfgDefOptRepoHost, 1), AssertError,
            "assertion 'valueId < cfgDefOptionHelpNameAltValueTotal(optionDefId)' failed");

        TEST_RESULT_Z(cfgDefCommandHelpSummary(cfgCmdBackup), "Backup a database cluster.", "backup command help summary");
        TEST_RESULT_Z(
            cfgDefCommandHelpDescription(cfgCmdBackup),
            "pgBackRest does not have a built-in scheduler so it's best to run it from cron or some other scheduling mechanism.",
            "backup command help description");

        TEST_RESULT_Z(cfgDefOptionHelpSection(cfgDefOptDelta), "general", "delta option help section");
        TEST_RESULT_Z(
            cfgDefOptionHelpSummary(cfgCmdBackup, cfgDefOptBufferSize), "Buffer size for file operations.",
            "backup command, delta option help summary");
        TEST_RESULT_Z(
            cfgDefOptionHelpSummary(cfgCmdBackup, cfgDefOptType), "Backup type.", "backup command, type option help summary");
        TEST_RESULT_Z(
            cfgDefOptionHelpDescription(cfgCmdBackup, cfgDefOptLogSubprocess),
            "Enable file logging for any subprocesses created by this process using the log level specified by log-level-file.",
            "backup command, log-subprocess option help description");
        TEST_RESULT_Z(
            cfgDefOptionHelpDescription(cfgCmdBackup, cfgDefOptType),
            "The following backup types are supported:\n"
            "\n"
            "* full - all database cluster files will be copied and there will be no dependencies on previous backups.\n"
            "* incr - incremental from the last successful backup.\n"
            "* diff - like an incremental backup but always based on the last full backup.",
            "backup command, type option help description");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
