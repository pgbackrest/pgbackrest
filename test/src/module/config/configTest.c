/***********************************************************************************************************************************
Test Configuration Commands and Options
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
        TEST_ERROR(cfgCommandId(BOGUS_STR, true), AssertError, "invalid command 'BOGUS'");
        TEST_RESULT_INT(cfgCommandId(BOGUS_STR, false), cfgCmdNone, "command none id from bogus");
        TEST_RESULT_INT(cfgCommandId("archive-push", true), cfgCmdArchivePush, "command id from name");

        TEST_ERROR(
            cfgCommandDefIdFromId(CFG_COMMAND_TOTAL), AssertError, "assertion 'commandId < cfgCmdNone' failed");
        TEST_RESULT_INT(cfgCommandDefIdFromId(cfgCmdBackup), cfgDefCmdBackup, "command id to def id");

        TEST_RESULT_Z(cfgCommandName(cfgCmdBackup), "backup", "command name from id");

        TEST_RESULT_INT(cfgOptionDefIdFromId(cfgOptPgHost + 6), cfgDefOptPgHost, "option id to def id");

        TEST_RESULT_INT(cfgOptionId("target"), cfgOptTarget, "option id from name");
        TEST_RESULT_INT(cfgOptionId(BOGUS_STR), -1, "option id from invalid option name");

        TEST_ERROR(
            cfgOptionIdFromDefId(999999, 6), AssertError,
            "assertion 'optionId != CFG_OPTION_TOTAL' failed");
        TEST_ERROR(
            cfgOptionIdFromDefId(0, 999999), AssertError,
            "assertion 'index < cfgDefOptionIndexTotal(optionDefId)' failed");
        TEST_RESULT_INT(cfgOptionIdFromDefId(cfgDefOptPgHost, 6), cfgOptPgHost + 6, "option def id to id");

        TEST_ERROR(cfgOptionIndex(CFG_OPTION_TOTAL), AssertError, "assertion 'optionId < CFG_OPTION_TOTAL' failed");
        TEST_RESULT_INT(cfgOptionIndex(cfgOptPgHostCmd + 6), 6, "option index");
        TEST_RESULT_INT(cfgOptionIndex(cfgOptCompressLevel), 0, "option index");

        TEST_RESULT_INT(cfgOptionIndexTotal(cfgOptPgPath), 8, "option index total");
        TEST_RESULT_INT(cfgOptionIndexTotal(cfgOptLogLevelConsole), 1, "option index total");

        TEST_RESULT_Z(cfgOptionName(cfgOptBackupStandby), "backup-standby", "option id from name");
    }

    // *****************************************************************************************************************************
    if (testBegin("configuration"))
    {
        TEST_RESULT_VOID(cfgInit(), "config init");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(cfgCommand(), cfgCmdNone, "command begins as none");
        TEST_RESULT_VOID(cfgCommandSet(cfgCmdBackup, cfgCmdRoleDefault), "command set to backup");
        TEST_RESULT_INT(cfgCommand(), cfgCmdBackup, "command is backup");
        TEST_RESULT_STR_Z(cfgCommandRoleName(), "backup", "command:role is backup");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cfgCommandSet(cfgCmdBackup, cfgCmdRoleLocal), "command set to backup:local");
        TEST_RESULT_STR_Z(cfgCommandRoleName(), "backup:local", "command:role is backup:local");
        TEST_RESULT_INT(cfgCommandInternal(cfgCmdBackup), false, "backup is external");
        TEST_RESULT_INT(cfgLogLevelDefault(), logLevelInfo, "default log level is info");
        TEST_RESULT_BOOL(cfgLogFile(), true, "log file is on");
        TEST_RESULT_BOOL(cfgLockRequired(), false, "lock is not required");
        TEST_RESULT_BOOL(cfgLockRemoteRequired(), true, "remote lock is required");
        TEST_RESULT_INT(cfgLockType(), lockTypeBackup, "lock is type backup");
        TEST_RESULT_BOOL(cfgParameterAllowed(), false, "parameters not allowed");

        TEST_RESULT_VOID(cfgCommandSet(cfgCmdInfo, cfgCmdRoleDefault), "command set to info");
        TEST_RESULT_INT(cfgLogLevelDefault(), logLevelDebug, "default log level is debug");
        TEST_RESULT_BOOL(cfgLogFile(), false, "log file is off");
        TEST_RESULT_BOOL(cfgLockRequired(), false, "lock is not required");
        TEST_RESULT_BOOL(cfgLockRemoteRequired(), false, "remote lock is not required");
        TEST_RESULT_INT(cfgLockType(), lockTypeNone, "lock is type none");

        TEST_RESULT_VOID(cfgCommandSet(cfgCmdStanzaCreate, cfgCmdRoleDefault), "command set to stanza-create");
        TEST_RESULT_BOOL(cfgLockRequired(), true, "lock is required");
        TEST_RESULT_INT(cfgLockType(), lockTypeAll, "lock is type all");

        TEST_RESULT_VOID(cfgCommandSet(cfgCmdArchiveGet, cfgCmdRoleAsync), "command set to archive-get:async");
        TEST_RESULT_BOOL(cfgLockRequired(), true, "lock is required");
        TEST_RESULT_BOOL(cfgLogFile(), true, "log file is on");

        TEST_RESULT_VOID(cfgCommandSet(cfgCmdInfo, cfgCmdRoleDefault), "command set to info");

        cfgOptionSet(cfgOptLogLevelFile, cfgSourceParam, VARSTRDEF("info"));
        cfgOptionValidSet(cfgOptLogLevelFile, true);

        TEST_RESULT_BOOL(cfgLogFile(), true, "log file is on");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command roles");

        TEST_ERROR(cfgCommandRoleEnum(strNew(BOGUS_STR)), CommandInvalidError, "invalid command role 'BOGUS'");
        TEST_RESULT_UINT(cfgCommandRoleEnum(NULL), cfgCmdRoleDefault, "command default role enum");
        TEST_RESULT_UINT(cfgCommandRoleEnum(strNew("async")), cfgCmdRoleAsync, "command async role enum");
        TEST_RESULT_UINT(cfgCommandRoleEnum(strNew("local")), cfgCmdRoleLocal, "command local role enum");
        TEST_RESULT_UINT(cfgCommandRoleEnum(strNew("remote")), cfgCmdRoleRemote, "command remote role enum");

        TEST_RESULT_STR(cfgCommandRoleStr(cfgCmdRoleDefault), NULL, "command default role str");
        TEST_RESULT_STR_Z(cfgCommandRoleStr(cfgCmdRoleAsync), "async", "command async role str");
        TEST_RESULT_STR_Z(cfgCommandRoleStr(cfgCmdRoleLocal), "local", "command local role str");
        TEST_RESULT_STR_Z(cfgCommandRoleStr(cfgCmdRoleRemote), "remote", "command remote role str");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(cfgCommandHelp(), false, "command help defaults to false");
        TEST_RESULT_VOID(cfgCommandHelpSet(true), "set command help");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "command help is set");

        // -------------------------------------------------------------------------------------------------------------------------
        StringList *param = strLstNew();
        strLstAddZ(param, "param");

        TEST_RESULT_INT(strLstSize(cfgCommandParam()), 0, "command param list defaults to empty");
        TEST_RESULT_VOID(cfgCommandParamSet(param), "set command param list");
        TEST_RESULT_INT(strLstSize(cfgCommandParam()), 1, "command param list is set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(cfgExe(), NULL, "exe defaults to null");
        TEST_RESULT_VOID(cfgExeSet(strNew("/path/to/exe")), "set exe");
        TEST_RESULT_Z(strPtr(cfgExe()), "/path/to/exe", "exe is set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(cfgOptionNegate(cfgOptConfig), false, "negate defaults to false");
        TEST_RESULT_VOID(cfgOptionNegateSet(cfgOptConfig, true), "set negate");
        TEST_RESULT_BOOL(cfgOptionNegate(cfgOptConfig), true, "negate is set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(cfgOptionReset(cfgOptConfig), false, "reset defaults to false");
        TEST_RESULT_VOID(cfgOptionResetSet(cfgOptConfig, true), "set reset");
        TEST_RESULT_BOOL(cfgOptionReset(cfgOptConfig), true, "reset is set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptConfig), false, "valid defaults to false");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptConfig), false, "option not valid for the command");
        TEST_RESULT_VOID(cfgOptionValidSet(cfgOptConfig, true), "set valid");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptConfig), true, "valid is set");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptConfig), false, "option valid but value is null");
        TEST_RESULT_VOID(cfgOptionSet(cfgOptConfig, cfgSourceParam, varNewStrZ("cfg")), "set option config");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptConfig), true, "option valid and value not null");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(cfgOption(cfgOptOnline), NULL, "online is null");
        TEST_RESULT_VOID(cfgOptionSet(cfgOptOnline, cfgSourceParam, varNewBool(false)), "set online");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptOnline), false, "online is set");
        TEST_RESULT_VOID(cfgOptionSet(cfgOptOnline, cfgSourceParam, varNewStrZ("1")), "set online");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptOnline), true, "online is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptOnline), cfgSourceParam, "online source is set");
        TEST_ERROR(
            cfgOptionDbl(cfgOptOnline), AssertError,
            "assertion 'varType(configStatic.option[optionId].value) == varTypeDouble' failed");
        TEST_ERROR(
            cfgOptionInt64(cfgOptOnline), AssertError,
            "assertion 'varType(configStatic.option[optionId].value) == varTypeInt64' failed");

        TEST_RESULT_VOID(cfgOptionSet(cfgOptCompressLevel, cfgSourceParam, varNewInt64(1)), "set compress-level");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 1, "compress-level is set");
        TEST_RESULT_VOID(cfgOptionSet(cfgOptCompressLevel, cfgSourceDefault, varNewStrZ("3")), "set compress-level");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 3, "compress-level is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptCompressLevel), cfgSourceDefault, "compress source is set");
        TEST_ERROR(
            cfgOptionBool(cfgOptCompressLevel), AssertError,
            "assertion 'varType(configStatic.option[optionId].value) == varTypeBool' failed");

        TEST_RESULT_VOID(
            cfgOptionSet(cfgOptArchivePushQueueMax, cfgSourceParam, varNewInt64(999999999999)), "set archive-push-queue-max");
        TEST_RESULT_INT(cfgOptionInt64(cfgOptArchivePushQueueMax), 999999999999, "archive-push-queue-max is set");
        TEST_RESULT_INT(cfgOptionUInt64(cfgOptArchivePushQueueMax), 999999999999, "archive-push-queue-max is set");

        TEST_RESULT_VOID(cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, varNewDbl(1.1)), "set protocol-timeout");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptProtocolTimeout), 1.1, "protocol-timeout is set");
        TEST_RESULT_VOID(cfgOptionSet(cfgOptProtocolTimeout, cfgSourceConfig, varNewStrZ("3.3")), "set protocol-timeout");
        TEST_RESULT_DOUBLE(cfgOptionDbl(cfgOptProtocolTimeout), 3.3, "protocol-timeout is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptProtocolTimeout), cfgSourceConfig, "protocol-timeout source is set");
        TEST_ERROR(
            cfgOptionKv(cfgOptProtocolTimeout), AssertError,
            "assertion 'varType(configStatic.option[optionId].value) == varTypeKeyValue' failed");

        TEST_RESULT_VOID(cfgOptionSet(cfgOptProtocolTimeout, cfgSourceConfig, NULL), "set protocol-timeout to NULL");
        TEST_RESULT_PTR(cfgOption(cfgOptProtocolTimeout), NULL, "protocol-timeout is not set");

        TEST_ERROR(
            cfgOptionSet(cfgOptRecoveryOption, cfgSourceParam, varNewDbl(1.1)), AssertError,
            "option 'recovery-option' must be set with KeyValue variant");
        TEST_RESULT_VOID(cfgOptionSet(cfgOptRecoveryOption, cfgSourceConfig, varNewKv(kvNew())), "set recovery-option");
        TEST_RESULT_INT(varLstSize(kvKeyList(cfgOptionKv(cfgOptRecoveryOption))), 0, "recovery-option is set");
        TEST_ERROR(
            cfgOptionLst(cfgOptRecoveryOption), AssertError,
            "assertion 'configStatic.option[optionId].value == NULL"
                " || varType(configStatic.option[optionId].value) == varTypeVariantList' failed");

        TEST_RESULT_INT(varLstSize(cfgOptionLst(cfgOptDbInclude)), 0, "db-include defaults to empty");
        TEST_ERROR(
            cfgOptionSet(cfgOptDbInclude, cfgSourceParam, varNewDbl(1.1)), AssertError,
            "option 'db-include' must be set with VariantList variant");
        TEST_RESULT_VOID(cfgOptionSet(cfgOptDbInclude, cfgSourceConfig, varNewVarLst(varLstNew())), "set db-include");
        TEST_RESULT_INT(varLstSize(cfgOptionLst(cfgOptDbInclude)), 0, "db-include is set");
        TEST_ERROR(
            cfgOptionStr(cfgOptDbInclude), AssertError,
            "assertion 'configStatic.option[optionId].value == NULL"
                " || varType(configStatic.option[optionId].value) == varTypeString' failed");

        TEST_RESULT_PTR(cfgOptionStr(cfgOptStanza), NULL, "stanza defaults to null");
        TEST_ERROR(
            cfgOptionSet(cfgOptStanza, cfgSourceParam, varNewDbl(1.1)), AssertError,
            "option 'stanza' must be set with String variant");
        TEST_RESULT_VOID(cfgOptionSet(cfgOptStanza, cfgSourceConfig, varNewStrZ("db")), "set stanza");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptStanza), "db", "stanza is set");
        TEST_ERROR(
            cfgOptionInt(cfgOptStanza), AssertError,
            "assertion 'varType(configStatic.option[optionId].value) == varTypeInt64' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cfgInit(), "config init resets value");
        TEST_RESULT_INT(cfgCommand(), cfgCmdNone, "command begins as none");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgOptionHostPort()"))
    {
        unsigned int port = 55555;

        cfgInit();
        cfgCommandSet(cfgCmdBackup, cfgCmdRoleDefault);

        cfgOptionValidSet(cfgOptRepoS3Host, true);
        cfgOptionSet(cfgOptRepoS3Host, cfgSourceConfig, varNewStrZ("host.com")) ;
        TEST_RESULT_STR_Z(cfgOptionHostPort(cfgOptRepoS3Host, &port), "host.com", "check plain host");
        TEST_RESULT_UINT(port, 55555, "check that port was not updated");

        cfgOptionSet(cfgOptRepoS3Host, cfgSourceConfig, varNewStrZ("myhost.com:777")) ;
        TEST_RESULT_STR_Z(cfgOptionHostPort(cfgOptRepoS3Host, &port), "myhost.com", "check host with port");
        TEST_RESULT_UINT(port, 777, "check that port was updated");

        TEST_RESULT_STR_Z(cfgOptionHostPort(cfgOptRepoS3Endpoint, &port), NULL, "check null host");
        TEST_RESULT_UINT(port, 777, "check that port was not updated");

        cfgOptionSet(cfgOptRepoS3Host, cfgSourceConfig, varNewStrZ("myhost.com:777:888")) ;
        TEST_ERROR(
            cfgOptionHostPort(cfgOptRepoS3Host, &port), OptionInvalidError,
            "'myhost.com:777:888' is not valid for option 'repo1-s3-host'"
                "\nHINT: is more than one port specified?");
        TEST_RESULT_UINT(port, 777, "check that port was not updated");

        cfgOptionValidSet(cfgOptRepoS3Endpoint, true);
        cfgOptionSet(cfgOptRepoS3Endpoint, cfgSourceConfig, varNewStrZ("myendpoint.com:ZZZ")) ;
        TEST_ERROR(
            cfgOptionHostPort(cfgOptRepoS3Endpoint, &port), OptionInvalidError,
            "'myendpoint.com:ZZZ' is not valid for option 'repo1-s3-endpoint'"
                "\nHINT: port is not a positive integer.");
        TEST_RESULT_UINT(port, 777, "check that port was not updated");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgOptionDefault() and cfgOptionDefaultSet()"))
    {
        TEST_RESULT_VOID(cfgInit(), "config init");
        TEST_RESULT_VOID(cfgCommandSet(cfgCmdBackup, cfgCmdRoleDefault), "backup command");

        TEST_ERROR(
            strPtr(varStr(cfgOptionDefaultValue(cfgOptDbInclude))), AssertError, "default value not available for option type 4");
        TEST_RESULT_STR_Z(varStr(cfgOptionDefault(cfgOptType)), "incr", "backup type default");
        TEST_RESULT_BOOL(varBool(cfgOptionDefault(cfgOptArchiveAsync)), false, "archive async default");
        TEST_RESULT_DOUBLE(varDbl(cfgOptionDefault(cfgOptProtocolTimeout)), 1830, "backup protocol-timeout default");
        TEST_RESULT_INT(varIntForce(cfgOptionDefault(cfgOptProcessMax)), 1, "backup process-max default");
        TEST_RESULT_PTR(cfgOptionDefault(cfgOptDbInclude), NULL, "backup db-include default is null");

        TEST_RESULT_VOID(cfgOptionSet(cfgOptPgHost, cfgSourceParam, varNewStrZ("backup")), "backup host set");
        TEST_RESULT_VOID(cfgOptionDefaultSet(cfgOptPgHost, varNewStrZ("backup-default")), "backup host default");
        TEST_RESULT_VOID(cfgOptionDefaultSet(cfgOptPgHost, varNewStrZ("backup-default2")), "reset backup host default");
        TEST_RESULT_STR_Z(varStr(cfgOption(cfgOptPgHost)), "backup", "backup host value");
        TEST_RESULT_STR_Z(varStr(cfgOptionDefault(cfgOptPgHost)), "backup-default2", "backup host default");

        TEST_RESULT_VOID(cfgOptionSet(cfgOptPgSocketPath, cfgSourceDefault, NULL), "backup pg-socket-path set");
        TEST_RESULT_VOID(cfgOptionDefaultSet(cfgOptPgSocketPath, varNewStrZ("/to/socket")), "backup pg-socket-path default");
        TEST_RESULT_VOID(cfgOptionDefaultSet(cfgOptPgSocketPath, varNewStrZ("/to/socket2")), "reset backup pg-socket-path default");
        TEST_RESULT_STR_Z(varStr(cfgOption(cfgOptPgSocketPath)), "/to/socket2", "backup pg-socket-path value");
        TEST_RESULT_STR_Z(varStr(cfgOptionDefault(cfgOptPgSocketPath)), "/to/socket2", "backup pg-socket-path value default");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
