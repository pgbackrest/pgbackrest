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
    if (testBegin("configParse()"))
    {
        StringList *argList = NULL;
        String *configFile = strNewFmt("%s/test.config", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(BOGUS_STR));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList)), CommandInvalidError, "invalid command 'BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--" BOGUS_STR));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError, "invalid option '--BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-host"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError, "option '--db-host' requires argument");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--no-online"));
        strLstAdd(argList, strNew("--no-online"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError, "option 'online' is negated multiple times");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--config=/etc/config"));
        strLstAdd(argList, strNew("--no-config"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError, "option 'config' cannot be set and negated");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--compress-level=3"));
        strLstAdd(argList, strNew("--compress-level=3"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "option 'compress-level' cannot have multiple arguments");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--online"));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList)), CommandRequiredError, "no command found");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionRequiredError,
            "backup command requires option: db1-path\nHINT: does this stanza exist?");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionRequiredError,
            "backup command requires option: stanza");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--no-config"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--repo-s3-key=xxx"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "option 'repo-s3-key' not valid without option 'repo-type' = 's3'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--backup-user=xxx"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "option 'backup-user' not valid without option 'backup-host'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--force"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "option 'force' not valid without option 'no-online'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--test-delay=1"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "option 'test-delay' not valid without option 'test'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--recovery-option=a=b"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "option 'recovery-option' not valid for command 'backup'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--target-exclusive"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "option 'target-exclusive' not valid without option 'type' in ('time', 'xid')");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--type=bogus"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidValueError,
            "'bogus' is not valid for 'type' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--process-max=0"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidValueError,
            "'0' is not valid for 'process-max' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--process-max=bogus"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidValueError,
            "'bogus' is not valid for 'process-max' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--protocol-timeout=.01"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidValueError,
            "'.01' is not valid for 'protocol-timeout' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--protocol-timeout=bogus"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidValueError,
            "'bogus' is not valid for 'protocol-timeout' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNewFmt("--config=%s", strPtr(configFile)));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));

        storagePut(storageLocal(), configFile, bufNewStr(strNew(
            "[global]\n"
            "compress=bogus\n"
        )));

        TEST_ERROR(configParse(
            strLstSize(argList), strLstPtr(argList)), OptionInvalidError, "boolean option 'compress' must be 'y' or 'n'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNewFmt("--config=%s", strPtr(configFile)));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));

        storagePut(storageLocal(), configFile, bufNewStr(strNew(
            "[global]\n"
            "compress=\n"
        )));

        TEST_ERROR(configParse(
            strLstSize(argList), strLstPtr(argList)), OptionInvalidValueError,
            "section 'global', key 'compress' must have a value");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), "no command");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "    help is set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdNone, "    command is none");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("help"));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), "help command");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "    help is set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdHelp, "    command is help");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("help"));
        strLstAdd(argList, strNew("version"));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), "help for version command");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "    help is set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdVersion, "    command is version");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_ARCHIVE_GET));
        strLstAdd(argList, strNew("000000010000000200000003"));
        strLstAdd(argList, strNew("/path/to/wal/RECOVERYWAL"));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), "command arguments");
        TEST_RESULT_STR(
            strPtr(strLstJoin(cfgCommandParam(), "|")), "000000010000000200000003|/path/to/wal/RECOVERYWAL",
            "    check command arguments");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--backup-host=backup"));
        strLstAdd(argList, strNew("--backup-user=pgbackrest"));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--no-online"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), TEST_COMMAND_BACKUP " command");
        TEST_RESULT_INT(cfgCommand(), cfgCmdBackup, "    command is " TEST_COMMAND_BACKUP);

        TEST_RESULT_STR(strPtr(cfgExe()), TEST_BACKREST_EXE, "    exe is set");

        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptStanza)), "db", "    stanza is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptStanza), cfgSourceParam, "    stanza is source param");
        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptDbPath)), "/path/to/db", "    db-path is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptDbPath), cfgSourceParam, "    db-path is source param");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptOnline), false, "    online is not set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptOnline), cfgSourceParam, "    online is source default");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptCompress), true, "    compress is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptCompress), cfgSourceDefault, "    compress is source default");
        TEST_RESULT_INT(cfgOptionInt(cfgOptBufferSize), 4194304, "    buffer-size is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBufferSize), cfgSourceDefault, "    buffer-size is source default");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNewFmt("--config=%s", strPtr(configFile)));
        strLstAdd(argList, strNew("--no-online"));
        strLstAdd(argList, strNew("--db1-host=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));

        storagePut(storageLocal(), configFile, bufNewStr(strNew(
            "[global]\n"
            "compress-level=3\n"
            "\n"
            "[global:backup]\n"
            "hardlink=y\n"
            "bogus=bogus\n"
            "no-compress=y\n"
            "archive-copy=y\n"
            "online=y\n"
            "\n"
            "[db:backup]\n"
            "compress=n\n"
            "recovery-option=a=b\n"
            "\n"
            "[db]\n"
            "db1-host=db\n"
            "db1-path=/path/to/db\n"
            "recovery-option=c=d\n"
        )));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), TEST_COMMAND_BACKUP " command");

        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptDbPath)), "/path/to/db", "    db-path is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptDbPath), cfgSourceConfig, "    db-path is source config");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptCompress), false, "    compress not is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptCompress), cfgSourceConfig, "    compress is source config");
        TEST_RESULT_PTR(cfgOption(cfgOptArchiveCheck), NULL, "    archive-check is not set");
        TEST_RESULT_PTR(cfgOption(cfgOptArchiveCopy), NULL, "    archive-copy is not set");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptHardlink), true, "    hardlink is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptHardlink), cfgSourceConfig, "    hardlink is source config");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 3, "    compress-level is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptCompressLevel), cfgSourceConfig, "    compress-level is source config");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--recovery-option=a"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "key/value 'a' not valid for 'recovery-option' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--recovery-option=a"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList)), OptionInvalidError,
            "key/value 'a' not valid for 'recovery-option' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--db-include=abc"));
        strLstAdd(argList, strNew("--db-include=def"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), TEST_COMMAND_RESTORE " command");

        const VariantList *includeList = NULL;
        TEST_ASSIGN(includeList, cfgOptionLst(cfgOptDbInclude), "get db include options");
        TEST_RESULT_STR(strPtr(varStr(varLstGet(includeList, 0))), "abc", "check db include option");
        TEST_RESULT_STR(strPtr(varStr(varLstGet(includeList, 1))), "def", "check db include option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--db-path=/path/to/db"));
        strLstAdd(argList, strNew("--recovery-option=a=b"));
        strLstAdd(argList, strNew("--recovery-option=c=de=fg hi"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), TEST_COMMAND_RESTORE " command");

        const KeyValue *recoveryKv = NULL;
        TEST_ASSIGN(recoveryKv, cfgOptionKv(cfgOptRecoveryOption), "get recovery options");
        TEST_RESULT_STR(strPtr(varStr(kvGet(recoveryKv, varNewStr(strNew("a"))))), "b", "check recovery option");
        TEST_RESULT_STR(strPtr(varStr(kvGet(recoveryKv, varNewStr(strNew("c"))))), "de=fg hi", "check recovery option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNewFmt("--config=%s", strPtr(configFile)));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));

        storagePut(storageLocal(), configFile, bufNewStr(strNew(
            "[global:restore]\n"
            "recovery-option=f=g\n"
            "recovery-option=hijk=l\n"
            "\n"
            "[db]\n"
            "db1-path=/path/to/db\n"
        )));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList)), TEST_COMMAND_RESTORE " command");

        TEST_ASSIGN(recoveryKv, cfgOptionKv(cfgOptRecoveryOption), "get recovery options");
        TEST_RESULT_STR(strPtr(varStr(kvGet(recoveryKv, varNewStr(strNew("f"))))), "g", "check recovery option");
        TEST_RESULT_STR(strPtr(varStr(kvGet(recoveryKv, varNewStr(strNew("hijk"))))), "l", "check recovery option");
    }
}
