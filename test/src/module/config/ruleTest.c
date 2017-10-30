/***********************************************************************************************************************************
Test Configuration Command and Option Rules
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
void testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("iterate all option/command combinations"))
    {
        for (int optionId = 0; optionId < cfgOptionTotal(); optionId++)
        {
            // Ensure that option name maps back to id
            const char *optionName = cfgOptionName(optionId);
            TEST_RESULT_INT(cfgOptionId(optionName), optionId, "option name to id");

            // Not much done with these except to ensure they don't blow up
            cfgRuleOptionNameAlt(optionId);
            cfgRuleOptionNegate(optionId);
            cfgRuleOptionPrefix(optionId);
            cfgRuleOptionSection(optionId);
            cfgRuleOptionSecure(optionId);
            cfgRuleOptionType(optionId);

            for (int commandId = 0; commandId < cfgCommandTotal(); commandId++)
            {
                // Ensure that command name maps back to id
                const char *commandName = cfgCommandName(commandId);
                TEST_RESULT_INT(cfgCommandId(commandName), commandId, "command name to id");

                if (cfgRuleOptionValid(commandId, optionId))
                {
                    if (cfgRuleOptionAllowList(commandId, optionId))
                    {
                        for (int valueId = 0; valueId < cfgRuleOptionAllowListValueTotal(commandId, optionId); valueId++)
                        {
                            const char *value = cfgRuleOptionAllowListValue(commandId, optionId, valueId);

                            TEST_RESULT_STR_NE(value, NULL, "allow list value exists");
                            TEST_RESULT_BOOL(
                                cfgRuleOptionAllowListValueValid(commandId, optionId, value), true, "allow list value valid");
                        }

                        TEST_RESULT_STR(
                            cfgRuleOptionAllowListValueValid(commandId, optionId, BOGUS_STR), NULL, "bogus allow list value");
                    }

                    if (cfgRuleOptionAllowRange(commandId, optionId))
                    {
                        cfgRuleOptionAllowRangeMax(commandId, optionId);
                        cfgRuleOptionAllowRangeMin(commandId, optionId);
                    }

                    cfgRuleOptionDefault(commandId, optionId);

                    if (cfgRuleOptionDepend(commandId, optionId))
                    {
                        TEST_RESULT_STR_NE(
                            cfgOptionName(cfgRuleOptionDependOption(commandId, optionId)), NULL, "depend option exists");

                        for (int valueId = 0; valueId < cfgRuleOptionDependValueTotal(commandId, optionId); valueId++)
                        {
                            const char *value = cfgRuleOptionDependValue(commandId, optionId, valueId);

                            TEST_RESULT_STR_NE(value, NULL, "depend option value exists");
                            TEST_RESULT_BOOL(
                                cfgRuleOptionDependValueValid(commandId, optionId, value), true, "depend option valid exists");
                        }

                        TEST_RESULT_STR(
                            cfgRuleOptionDependValueValid(commandId, optionId, BOGUS_STR), NULL, "bogus depend option value");
                    }

                    cfgRuleOptionRequired(commandId, optionId);
                }
            }
        }
    }

    // Static tests against known values -- these may break as options change so will need to be kept up to date.  The tests have
    // generally been selected to favor values that are not expected to change but adjustments are welcome as long as the type of
    // test is not drastically changed.
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("check known values"))
    {
        TEST_RESULT_INT(cfgCommandId("archive-push"), CFGCMD_ARCHIVE_PUSH, "command id from name");
        TEST_RESULT_INT(cfgCommandId(BOGUS_STR), -1, "command id from invalid command name");

        TEST_RESULT_INT(cfgOptionId("target"), CFGOPT_TARGET, "option id from name");
        TEST_RESULT_INT(cfgOptionId(BOGUS_STR), -1, "option id from invalid option name");

        TEST_RESULT_BOOL(cfgRuleOptionAllowList(CFGCMD_BACKUP, CFGOPT_TYPE), true, "allow list valid");
        TEST_RESULT_BOOL(cfgRuleOptionAllowList(CFGCMD_BACKUP, CFGOPT_DB_HOST), false, "allow list not valid");

        TEST_RESULT_INT(cfgRuleOptionAllowListValueTotal(CFGCMD_BACKUP, CFGOPT_TYPE), 3, "allow list total");

        TEST_RESULT_STR(cfgRuleOptionAllowListValue(CFGCMD_BACKUP, CFGOPT_TYPE, 0), "full", "allow list value 0");
        TEST_RESULT_STR(cfgRuleOptionAllowListValue(CFGCMD_BACKUP, CFGOPT_TYPE, 1), "diff", "allow list value 1");
        TEST_RESULT_STR(cfgRuleOptionAllowListValue(CFGCMD_BACKUP, CFGOPT_TYPE, 2), "incr", "allow list value 2");

        TEST_RESULT_BOOL(cfgRuleOptionAllowListValueValid(CFGCMD_BACKUP, CFGOPT_TYPE, "diff"), true, "allow list value valid");
        TEST_RESULT_BOOL(
            cfgRuleOptionAllowListValueValid(CFGCMD_BACKUP, CFGOPT_TYPE, BOGUS_STR), false, "allow list value not valid");

        TEST_RESULT_BOOL(cfgRuleOptionAllowRange(CFGCMD_BACKUP, CFGOPT_COMPRESS_LEVEL), true, "range allowed");
        TEST_RESULT_BOOL(cfgRuleOptionAllowRange(CFGCMD_BACKUP, CFGOPT_BACKUP_HOST), false, "range not allowed");

        TEST_RESULT_DOUBLE(cfgRuleOptionAllowRangeMin(CFGCMD_BACKUP, CFGOPT_DB_TIMEOUT), 0.1, "range min");
        TEST_RESULT_DOUBLE(cfgRuleOptionAllowRangeMax(CFGCMD_BACKUP, CFGOPT_COMPRESS_LEVEL), 9, "range max");

        TEST_RESULT_STR(cfgRuleOptionDefault(CFGCMD_BACKUP, CFGOPT_COMPRESS_LEVEL), "6", "default exists");
        TEST_RESULT_STR(cfgRuleOptionDefault(CFGCMD_BACKUP, CFGOPT_BACKUP_HOST), NULL, "default does not exist");

        TEST_RESULT_BOOL(cfgRuleOptionDepend(CFGCMD_RESTORE, CFGOPT_REPO_S3_KEY), true, "has depend option");
        TEST_RESULT_BOOL(cfgRuleOptionDepend(CFGCMD_RESTORE, CFGOPT_TYPE), false, "does not have depend option");

        TEST_RESULT_INT(cfgRuleOptionDependOption(CFGCMD_BACKUP, CFGOPT_DB_USER), CFGOPT_DB_HOST, "depend option id");

        TEST_RESULT_INT(cfgRuleOptionDependValueTotal(CFGCMD_RESTORE, CFGOPT_TARGET), 3, "depend option value total");
        TEST_RESULT_STR(cfgRuleOptionDependValue(CFGCMD_RESTORE, CFGOPT_TARGET, 0), "name", "depend option value 0");
        TEST_RESULT_STR(cfgRuleOptionDependValue(CFGCMD_RESTORE, CFGOPT_TARGET, 1), "time", "depend option value 1");
        TEST_RESULT_STR(cfgRuleOptionDependValue(CFGCMD_RESTORE, CFGOPT_TARGET, 2), "xid", "depend option value 2");

        TEST_RESULT_BOOL(cfgRuleOptionDependValueValid(CFGCMD_RESTORE, CFGOPT_TARGET, "time"), true, "depend option value valid");
        TEST_RESULT_BOOL(
            cfgRuleOptionDependValueValid(CFGCMD_RESTORE, CFGOPT_TARGET, BOGUS_STR), false, "depend option value not valid");

        TEST_RESULT_INT(cfgOptionIndexTotal(CFGOPT_DB_PATH), 8, "index total > 1");
        TEST_RESULT_INT(cfgOptionIndexTotal(CFGOPT_REPO_PATH), 1, "index total == 1");

        TEST_RESULT_STR(cfgRuleOptionNameAlt(CFGOPT_DB1_HOST), "db-host", "alt name for indexed option");
        TEST_RESULT_STR(cfgRuleOptionNameAlt(CFGOPT_PROCESS_MAX), "thread-max", "alt name for non-indexed option");
        TEST_RESULT_STR(cfgRuleOptionNameAlt(CFGOPT_TYPE), NULL, "no alt name");

        TEST_RESULT_BOOL(cfgRuleOptionNegate(CFGOPT_ONLINE), true, "option can be negated");
        TEST_RESULT_BOOL(cfgRuleOptionNegate(CFGOPT_TYPE), false, "option cannot be negated");

        TEST_RESULT_STR(cfgRuleOptionPrefix(CFGOPT_DB_HOST), "db", "option prefix, index 1");
        TEST_RESULT_STR(cfgRuleOptionPrefix(CFGOPT_DB8_HOST), "db", "option prefix, index 8");
        TEST_RESULT_STR(cfgRuleOptionPrefix(CFGOPT_TYPE), NULL, "option has no prefix");

        TEST_RESULT_BOOL(cfgRuleOptionRequired(CFGCMD_BACKUP, CFGOPT_CONFIG), true, "option required");
        TEST_RESULT_BOOL(cfgRuleOptionRequired(CFGCMD_RESTORE, CFGOPT_BACKUP_HOST), false, "option not required");

        TEST_RESULT_STR(cfgRuleOptionSection(CFGOPT_REPO_S3_KEY), "global", "global section");
        TEST_RESULT_STR(cfgRuleOptionSection(CFGOPT_TYPE), NULL, "any section");

        TEST_RESULT_BOOL(cfgRuleOptionSecure(CFGOPT_REPO_S3_KEY), true, "option secure");
        TEST_RESULT_BOOL(cfgRuleOptionSecure(CFGOPT_BACKUP_HOST), false, "option not secure");

        TEST_RESULT_INT(cfgRuleOptionType(CFGOPT_TYPE), CFGOPTDEF_TYPE_STRING, "string type");
        TEST_RESULT_INT(cfgRuleOptionType(CFGOPT_COMPRESS), CFGOPTDEF_TYPE_BOOLEAN, "boolean type");

        TEST_RESULT_BOOL(cfgRuleOptionValid(CFGCMD_BACKUP, CFGOPT_TYPE), true, "option valid");
        TEST_RESULT_BOOL(cfgRuleOptionValid(CFGCMD_INFO, CFGOPT_TYPE), false, "option not valid");

        TEST_RESULT_STR(cfgCommandName(CFGCMD_ARCHIVE_GET), "archive-get", "command name from id");
        TEST_RESULT_STR(cfgCommandName(-1), NULL, "invalid command id (lower bound)");
        TEST_RESULT_STR(cfgCommandName(999999), NULL, "invalid command id (upper bound)");

        TEST_RESULT_STR(cfgOptionName(CFGOPT_COMPRESS_LEVEL), "compress-level", "option name from invalid id");
        TEST_RESULT_STR(cfgOptionName(-1), NULL, "invalid option id (lower bound)");
        TEST_RESULT_STR(cfgOptionName(999999), NULL, "invalid option id (upper bound)");
    }
}
