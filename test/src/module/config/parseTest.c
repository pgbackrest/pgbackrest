/***********************************************************************************************************************************
Test Configuration Parse
***********************************************************************************************************************************/
#include "common/type/json.h"
#include "protocol/helper.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"

#define TEST_BACKREST_EXE                                           "pgbackrest"

#define TEST_COMMAND_ARCHIVE_GET                                    "archive-get"
#define TEST_COMMAND_BACKUP                                         "backup"
#define TEST_COMMAND_HELP                                           "help"
#define TEST_COMMAND_RESTORE                                        "restore"
#define TEST_COMMAND_VERSION                                        "version"

/***********************************************************************************************************************************
Option find test -- this is done a lot in the deprecated tests
***********************************************************************************************************************************/
static void
testOptionFind(const char *optionName, unsigned int optionId, unsigned int optionKeyIdx, bool negate, bool reset, bool deprecated)
{
    CfgParseOptionResult option = cfgParseOptionP(STR(optionName));

    TEST_RESULT_BOOL(option.found, true, zNewFmt("check %s found", optionName));
    TEST_RESULT_UINT(option.id, optionId, zNewFmt("check %s id %u", optionName, optionId));
    TEST_RESULT_UINT(option.keyIdx, optionKeyIdx, zNewFmt("check %s key idx %u", optionName, optionKeyIdx));
    TEST_RESULT_BOOL(option.negate, negate, zNewFmt("check %s negate %d", optionName, negate));
    TEST_RESULT_BOOL(option.reset, reset, zNewFmt("check %s reset %d", optionName, reset));
    TEST_RESULT_BOOL(option.deprecated, deprecated, zNewFmt("check %s deprecated %d", optionName, deprecated));
}

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(FSLASH_STR);
    Storage *storageTestWrite = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("size"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check size of parse structures");

        TEST_RESULT_UINT(sizeof(ParseRuleOption), TEST_64BIT() ? 40 : 28, "ParseRuleOption size");
        TEST_RESULT_UINT(sizeof(ParseRuleOptionDeprecate), TEST_64BIT() ? 16 : 12, "ParseRuleOptionDeprecate size");

        // Each pack must be <= 127 bytes because only one varint byte is used for the size. The compiler will catch packs larger
        // than 127 bytes and in that case PARSE_RULE_PACK_SIZE must be increased. There would be little cost to increasing this as
        // a preventative measure but a check would still be required, so may as well be as efficient as possible.
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("gather pack size statistics");

        unsigned int packTotal = 0;
        size_t packMaxSize = 0;
        const char *packMaxName = NULL;
        size_t packTotalSize = 0;

        for (unsigned int optIdx = 0; optIdx < CFG_OPTION_TOTAL; optIdx++)
        {
            packTotal += parseRuleOption[optIdx].pack != NULL;
            packTotalSize += parseRuleOption[optIdx].packSize;

            if (parseRuleOption[optIdx].packSize > packMaxSize)
            {
                packMaxName = parseRuleOption[optIdx].name;
                packMaxSize = parseRuleOption[optIdx].packSize;
            }
        }

        TEST_LOG_FMT("total size of option packs is %zu bytes", packTotalSize);
        TEST_LOG_FMT("avg option pack size is %0.2f bytes", (float)packTotalSize / (float)packTotal);
        TEST_LOG_FMT("max option pack size is '%s' at %zu bytes", packMaxName, packMaxSize);
        TEST_LOG_FMT("total options with packs is %u (out of %d options) ", packTotal, CFG_OPTION_TOTAL);
    }

    // Config functions that are not tested with parse
    // *****************************************************************************************************************************
    if (testBegin("cfgInited()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config command defaults to none before cfgInit()");

        TEST_RESULT_BOOL(cfgInited(), false, "config is not inited");
    }

    // config and config-include-path options
    // *****************************************************************************************************************************
    if (testBegin("cfgFileLoad()"))
    {
        StringList *argList = NULL;
        const String *configFile = STRDEF(TEST_PATH "/test.config");

        const String *configIncludePath = STRDEF(TEST_PATH "/conf.d");
        HRN_SYSTEM_FMT("mkdir -m 750 %s", strZ(configIncludePath));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no stanzas loaded yet");

        TEST_RESULT_STRLST_Z(cfgParseStanzaList(), NULL, "stanza list");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check old config file constants");

        TEST_RESULT_Z(PGBACKREST_CONFIG_ORIG_PATH_FILE, "/etc/pgbackrest.conf", "check old config path");
        TEST_RESULT_STR_Z(PGBACKREST_CONFIG_ORIG_PATH_FILE_STR, "/etc/pgbackrest.conf", "check old config path str");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("confirm same behavior with multiple config include files");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRaw(argList, cfgOptConfig, configFile);
        hrnCfgArgRaw(argList, cfgOptConfigIncludePath, configIncludePath);
        hrnCfgArgRawNegate(argList, cfgOptOnline);
        hrnCfgArgKeyRawReset(argList, cfgOptPgHost, 1);
        hrnCfgArgRawReset(argList, cfgOptBackupStandby);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);

        storagePut(
            storageNewWriteP(storageTestWrite, configFile),
            BUFSTRDEF(
                "[global]\n"
                "compress-level=3\n"
                "spool-path=/path/to/spool\n"));

        storagePut(
            storageNewWriteP(storageTestWrite, strNewFmt("%s/global-backup.conf", strZ(configIncludePath))),
            BUFSTRDEF(
                "[global:backup]\n"
                "repo1-hardlink=y\n"
                "bogus=bogus\n"
                "no-delta=y\n"
                "reset-delta=y\n"
                "archive-copy=y\n"
                "online=y\n"
                "pg1-path=/not/path/to/db\n"
                "backup-standby=y\n"
                "buffer-size=65536\n"));

        storagePut(
            storageNewWriteP(storageTestWrite, strNewFmt("%s/db-backup.conf", strZ(configIncludePath))),
            BUFSTRDEF(
                "[db:backup]\n"
                "delta=n\n"
                "recovery-option=a=b\n"));

        storagePut(
            storageNewWriteP(storageTestWrite, strNewFmt("%s/stanza.db.conf", strZ(configIncludePath))),
            BUFSTRDEF(
                "[db]\n"
                "pg1-host=db\n"
                "pg1-path=/path/to/db\n"
                "recovery-option=c=d\n"));

        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true),
            TEST_COMMAND_BACKUP " command with config-include");
        TEST_RESULT_LOG(
            "P00   WARN: configuration file contains option 'recovery-option' invalid for section 'db:backup'\n"
            "P00   WARN: configuration file contains invalid option 'bogus'\n"
            "P00   WARN: configuration file contains negate option 'no-delta'\n"
            "P00   WARN: configuration file contains reset option 'reset-delta'\n"
            "P00   WARN: configuration file contains command-line only option 'online'\n"
            "P00   WARN: configuration file contains stanza-only option 'pg1-path' in global section 'global:backup'");

        TEST_RESULT_BOOL(cfgOptionTest(cfgOptPgHost), false, "pg1-host is not set (command line reset override)");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptPgPath), "/path/to/db", "pg1-path is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptPgPath), cfgSourceConfig, "pg1-path is source config");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptDelta), false, "delta not is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptDelta), cfgSourceConfig, "delta is source config");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptArchiveCheck), false, "archive-check is false");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptArchiveCopy), false, "archive-copy is false");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptRepoHardlink), true, "repo-hardlink is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptRepoHardlink), cfgSourceConfig, "repo-hardlink is source config");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 3, "compress-level is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptCompressLevel), cfgSourceConfig, "compress-level is source config");
        TEST_RESULT_UINT(cfgOptionStrId(cfgOptBackupStandby), CFGOPTVAL_BACKUP_STANDBY_N, "backup-standby not is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBackupStandby), cfgSourceDefault, "backup-standby is source default");
        TEST_RESULT_INT(cfgOptionInt64(cfgOptBufferSize), 65536, "buffer-size is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBufferSize), cfgSourceConfig, "backup-standby is source config");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("rename conf files - ensure read of conf extension only is attempted");

        HRN_SYSTEM_FMT("mv %s/db-backup.conf %s/db-backup.conf.save", strZ(configIncludePath), strZ(configIncludePath));
        HRN_SYSTEM_FMT("mv %s/global-backup.conf %s/global-backup.confsave", strZ(configIncludePath), strZ(configIncludePath));

        // Set up defaults
        const String *const backupCmdDefConfigValue =
            (const String *)&parseRuleValueStr[parseRuleValStrCFGOPTDEF_CONFIG_PATH_SP_QT_FS_QT_SP_PROJECT_CONFIG_FILE];
        const String *const backupCmdDefConfigInclPathValue =
            (const String *)&parseRuleValueStr[parseRuleValStrCFGOPTDEF_CONFIG_PATH_SP_QT_FS_QT_SP_PROJECT_CONFIG_INCLUDE_PATH];
        const String *oldConfigDefault = STRDEF(TEST_PATH PGBACKREST_CONFIG_ORIG_PATH_FILE);

        // Create the option structure and initialize with 0
        ParseOption parseOptionList[CFG_OPTION_TOTAL] = {{0}};

        StringList *value = strLstNew();
        strLstAdd(value, configFile);

        parseOptionList[cfgOptConfig].indexListTotal = 1;
        parseOptionList[cfgOptConfig].indexList = memNew(sizeof(ParseOptionValue));
        parseOptionList[cfgOptConfig].indexList[0] = (ParseOptionValue)
        {
            .found = true,
            .source = cfgSourceParam,
            .valueList = value,
        };

        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfigIncludePath].indexListTotal = 1;
        parseOptionList[cfgOptConfigIncludePath].indexList = memNew(sizeof(ParseOptionValue));
        parseOptionList[cfgOptConfigIncludePath].indexList[0] = (ParseOptionValue)
        {
            .found = true,
            .source = cfgSourceParam,
            .valueList = value,
        };

        TEST_RESULT_VOID(cfgFileLoadPart(NULL, NULL), "check null part");

        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n"
            "\n"
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config-include-path with .conf files and non-.conf files");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pass invalid values");

        // --config valid, --config-include-path invalid (does not exist)
        value = strLstNew();
        strLstAddZ(value, BOGUS_STR);

        parseOptionList[cfgOptConfigIncludePath].indexList[0].valueList = value;
        TEST_ERROR(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            PathMissingError, "unable to list file info for missing path '/BOGUS'");

        // --config-include-path valid, --config invalid (does not exist)
        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfigIncludePath].indexList[0].valueList = value;

        value = strLstNew();
        strLstAddZ(value, TEST_PATH "/" BOGUS_STR);

        parseOptionList[cfgOptConfig].indexList[0].valueList = value;

        TEST_ERROR_FMT(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            FileMissingError, STORAGE_ERROR_READ_MISSING, TEST_PATH "/BOGUS");

        strLstFree(parseOptionList[cfgOptConfig].indexList[0].valueList);
        strLstFree(parseOptionList[cfgOptConfigIncludePath].indexList[0].valueList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("neither config nor config-include-path passed as parameter (defaults but none exist)");

        parseOptionList[cfgOptConfig].indexList[0].found = false;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = false;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceDefault;

        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            NULL, "config default, config-include-path default but nothing to read");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config not passed as parameter (config does not exist), config-include-path passed - only include read");

        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].indexList[0].found = false;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = true;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config default and doesn't exist, config-include-path passed read");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config and config-include-path are 'default' with files existing - old location ignored");

        // config file exists in both current default and old default location - old location ignored.
        parseOptionList[cfgOptConfig].indexList[0].found = false;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = false;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceDefault;

        HRN_SYSTEM_FMT("mkdir -m 750 %s", strZ(strPath(oldConfigDefault)));
        storagePut(
            storageNewWriteP(storageTestWrite, oldConfigDefault),
            BUFSTRDEF(
                "[global:backup]\n"
                "buffer-size=65536\n"));

        // Pass actual location of config files as "default" - not setting valueList above, so these are the only possible values
        // to choose.
        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, configFile, configIncludePath, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n"
            "\n"
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config and config-include-path default, files appended, original config not read");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config not passed (only old default exists), config-include-path passed - old default and include read");

        // Config not passed as parameter - config does not exist in new default but does exist in old default. config-include-path
        // passed as parameter - both include and old default read
        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].indexList[0].found = false;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = true;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global:backup]\n"
            "buffer-size=65536\n"
            "\n"
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config old default read, config-include-path passed read");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("files read only from include path and not current or old config");

        // --no-config and config-include-path passed as parameter (files read only from include path and not current or old config)
        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].indexList[0].found = true;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceParam;
        parseOptionList[cfgOptConfig].indexList[0].negate = true;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = true;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "--no-config, only config-include-path read");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("--no-config and config-include-path default exists with files - nothing to read");

        parseOptionList[cfgOptConfig].indexList[0].found = true;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceParam;
        parseOptionList[cfgOptConfig].indexList[0].negate = true;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = false;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceDefault;

        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, configIncludePath, oldConfigDefault),
            NULL, "--no-config, config-include-path default, nothing read");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config passed and config-include-path default exists with files - only config read");

        value = strLstNew();
        strLstAdd(value, configFile);

        parseOptionList[cfgOptConfig].indexList[0].found = true;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceParam;
        parseOptionList[cfgOptConfig].indexList[0].negate = false;
        parseOptionList[cfgOptConfig].indexList[0].valueList = value;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = false;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceDefault;

        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, configIncludePath, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n",
            "config param specified, config-include-path default, only config read");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config-include-path and new config default read");

        // config new default and config-include-path passed - both exists with files. config-include-path & new config default read
        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].indexList[0].found = false;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = true;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, configFile, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n"
            "\n"
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config new default exists with files, config-include-path passed, default config and config-include-path read");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config-path overrides - config and config-include-path are 'default'");

        parseOptionList[cfgOptConfig].indexList[0].found = false;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = false;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceDefault;

        // File exists in old default config location but not in current default.
        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, configIncludePath, oldConfigDefault),
            "[global:backup]\n"
            "buffer-size=65536\n"
            "\n"
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config-path override - config-include-path files and old config default read since no config in current path");

        // Pass --config-path
        value = strLstNew();
        strLstAddZ(value, TEST_PATH);

        parseOptionList[cfgOptConfigPath].indexListTotal = 1;
        parseOptionList[cfgOptConfigPath].indexList = memNew(sizeof(ParseOptionValue));
        parseOptionList[cfgOptConfigPath].indexList[0] = (ParseOptionValue)
        {
            .found = true,
            .source = cfgSourceParam,
            .valueList = value,
        };

        // Override default paths for config and config-include-path - but no pgbackrest.conf file in override path only in old
        // default so ignored
        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config-path override: config-include-path files read but config not read - does not exist");

        // Pass --config
        value = strLstNew();
        strLstAdd(value, configFile);

        parseOptionList[cfgOptConfig].indexList[0].found = true;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceParam;
        parseOptionList[cfgOptConfig].indexList[0].negate = false;
        parseOptionList[cfgOptConfig].indexList[0].valueList = value;

        // Passing --config and --config-path - default config-include-path overwritten and config is required and is loaded and
        // config-include-path files will attempt to be loaded but not required
        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n"
            "\n"
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config-path changed config-include-path default - files exist, config and config includes read");

        value = strLstNew();
        strLstAddZ(value, BOGUS_STR);

        parseOptionList[cfgOptConfigPath].indexList[0].valueList = value;

        // Passing --config and bogus --config-path - default config-include-path overwritten, config is required and is loaded and
        // config-include-path files will attempt to be loaded but doesn't exist - no error since not required
        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n",
            "config-path changed config-include-path default but directory does not exist - only config read");

        // Copy the configFile to pgbackrest.conf (default is /etc/pgbackrest/pgbackrest.conf and new value is testPath so copy the
        // config file (that was not read in the previous test) to pgbackrest.conf so it will be read by the override
        HRN_SYSTEM_FMT("cp %s " TEST_PATH "/pgbackrest.conf", strZ(configFile));

        parseOptionList[cfgOptConfig].indexList[0].found = false;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceDefault;
        parseOptionList[cfgOptConfig].indexList[0].negate = false;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = false;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceDefault;

        value = strLstNew();
        strLstAddZ(value, TEST_PATH);

        parseOptionList[cfgOptConfigPath].indexList[0].valueList = value;

        // Override default paths for config and config-include-path with --config-path
        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n"
            "\n"
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config-path override: config-include-path and config file read");

        // Clear config-path
        parseOptionList[cfgOptConfigPath].indexList[0].found = false;
        parseOptionList[cfgOptConfigPath].indexList[0].source = cfgSourceDefault;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config default and config-include-path passed, no config files in the include path - only in the default path");

        // rm command is split here because code counter is confused by what looks like a comment
        HRN_SYSTEM_FMT("rm -rf %s/" "*", strZ(configIncludePath));

        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].indexList[0].found = false;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = true;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(storageTest, parseOptionList, configFile, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n",
            "config default exists with files but config-include-path path passed is empty - only config read");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config default and config-include-path passed, only empty file in include path, nothing in config defaults");

        HRN_SYSTEM_FMT("touch %s/empty.conf", strZ(configIncludePath));

        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].indexList[0].found = false;
        parseOptionList[cfgOptConfig].indexList[0].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].found = true;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].indexList[0].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(
                storageTest, parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, backupCmdDefConfigValue),
            NULL, "config default does not exist, config-include-path passed but only empty conf file - nothing read");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgParseSize() and cfgParseTime()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cfgParseSize()");

        TEST_ERROR(cfgParseSizeQualifier('w'), AssertError, "'w' is not a valid size qualifier");
        TEST_RESULT_INT(cfgParseSize(STRDEF("10B")), 10, "10B");
        TEST_RESULT_INT(cfgParseSize(STRDEF("1k")), 1024, "1k");
        TEST_RESULT_INT(cfgParseSize(STRDEF("1KiB")), 1024, "1KiB");
        TEST_RESULT_INT(cfgParseSize(STRDEF("5G")), (uint64_t)5 * 1024 * 1024 * 1024, "5G");
        TEST_RESULT_INT(cfgParseSize(STRDEF("3Tb")), (uint64_t)3 * 1024 * 1024 * 1024 * 1024, "3Tb");
        TEST_RESULT_INT(cfgParseSize(STRDEF("11")), 11, "11 - no qualifier, default bytes");
        TEST_RESULT_INT(cfgParseSize(STRDEF("4pB")), 4503599627370496, "4pB");
        TEST_RESULT_INT(cfgParseSize(STRDEF("15MB")), (uint64_t)15 * 1024 * 1024, "15MB");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cfgParseSize()");

        TEST_ERROR(cfgParseTime(STRDEF("1md")), FormatError, "unable to convert base 10 string '1m' to int64");
        TEST_ERROR(cfgParseTime(STRDEF("")), FormatError, "value '' is not valid");
        TEST_ERROR(cfgParseTime(STRDEF("s")), FormatError, "value 's' is not valid");
        TEST_ERROR(cfgParseTime(STRDEF("999999999999w")), FormatError, "value '999999999999w' is not valid");
        TEST_RESULT_INT(cfgParseTime(STRDEF("1M")), 60 * 1000, "1m");
        TEST_RESULT_INT(cfgParseTime(STRDEF("2H")), 2 * 60 * 60 * 1000, "2h");
        TEST_RESULT_INT(cfgParseTime(STRDEF("3W")), 3 * 7 * 24 * 60 * 60 * 1000, "3w");
    }

    // *****************************************************************************************************************************
    if (testBegin("cfgParseP()"))
    {
        StringList *argList = NULL;
        const String *configFile = STRDEF(TEST_PATH "/test.config");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option resolve list contains an entry for every option");

        TEST_RESULT_INT(
            LENGTH_OF(optionResolveOrder), CFG_OPTION_TOTAL,
            "check that the option resolve list contains an entry for every option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on single - option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "-bogus");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true),
            OptionInvalidError, "option '-bogus' must begin with --");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when option argument is invalid");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptOnline, "bogus");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "boolean option '--online' argument must be 'y' or 'n'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("boolean option with negation argument");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptNeutralUmask, "n");
        strLstAddZ(argList, CFGCMD_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionRequiredError,
            "backup command requires option: stanza");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("boolean option with affirmative argument");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptNeutralUmask, "y");
        strLstAddZ(argList, CFGCMD_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionRequiredError,
            "backup command requires option: stanza");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when negated boolean option with negative argument");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--no-neutral-umask=n");
        strLstAddZ(argList, CFGCMD_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'no-neutral-umask' does not allow an argument");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when negated boolean option with affirmative argument");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--no-online=y");
        strLstAddZ(argList, CFGCMD_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'no-online' does not allow an argument");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when argumentless option is passed with an argument");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--reset-pg1-host=xxx");
        strLstAddZ(argList, CFGCMD_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'reset-pg1-host' does not allow an argument");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid command");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, BOGUS_STR);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), CommandInvalidError,
            "invalid command 'BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid command and role but invalid command/role combination");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, CFGCMD_BACKUP ":" CONFIG_COMMAND_ROLE_ASYNC);

        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), CommandInvalidError,
            "invalid command/role combination 'backup:async'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid command and role");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, BOGUS_STR ":" BOGUS_STR);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), CommandInvalidError,
            "invalid command 'BOGUS:BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on ambiguous partial option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--c");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "invalid option '--c'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on ambiguous deprecated partial option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--retention");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "invalid option '--retention'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on spaces in option name");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, " --config=/path/to");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), CommandInvalidError,
            "invalid command ' --config=/path/to'");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--config =/path/to");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "invalid option '--config =/path/to'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--" BOGUS_STR);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "invalid option '--BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error if option exceeds maximum size");

        char optionMax[OPTION_NAME_SIZE_MAX + 2];

        for (unsigned int optionMaxIdx = 0; optionMaxIdx < OPTION_NAME_SIZE_MAX + 1; optionMaxIdx++)
            optionMax[optionMaxIdx] = 'X';

        optionMax[OPTION_NAME_SIZE_MAX + 1] = '\0';

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddFmt(argList, "--%s", optionMax);
        TEST_ERROR_FMT(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option '%s' exceeds maximum size of 64", optionMax);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error if option cannot be negated or reset");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--no-force");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'no-force' cannot be negated");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--reset-force");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'reset-force' cannot be reset");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error if option less than key min");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--pg0-path=/dude");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'pg0-path' key must be between 1 and 256");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error if option greater than key max");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--pg257-path");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'pg257-path' key must be between 1 and 256");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error if option begins with a number");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--257-path");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option '257-path' cannot begin with a number");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error if option begins with a dash");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "---pg1-path");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option '-pg1-path' cannot begin with a dash");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error if unindexed option cannot have an index");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--compress77-type");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'compress77-type' cannot have an index");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--retention128-full");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "deprecated option 'retention128-full' cannot have an index");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error if indexed option does not have an index");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--repo-storage-ca-file");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'repo-storage-ca-file' requires an index\n"
            "HINT: add the required index, e.g. repo1-storage-ca-file.");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--repo-azure-ca-path");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "deprecated option 'repo-azure-ca-path' requires an index\n"
            "HINT: add the required index, e.g. repo1-azure-ca-path.\n"
            "HINT: consider using the non-deprecated name, e.g. repo1-storage-ca-path.");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("allow option parse without an index");

        TEST_RESULT_BOOL(cfgParseOptionP(STRDEF("repo-storage-ca-file"), .ignoreMissingIndex = true).found, true, "option");
        TEST_RESULT_BOOL(
            cfgParseOptionP(STRDEF("repo-azure-ca-path"), .ignoreMissingIndex = true).found, true, "deprecated option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--pg1-host");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option '--pg1-host' requires an argument");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("parameters passed for command that does not allow parameters");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "param1");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), ParamInvalidError,
            "command does not allow parameters");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("help not available for roles other than main");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup:remote");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), CommandInvalidError,
            "help is not available for the 'remote' role");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("help ignores parameters");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup");
        strLstAddZ(argList, "param1");
        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true),
            "ignore params when help command");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("duplicate negation");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawNegate(argList, cfgOptOnline);
        hrnCfgArgRawNegate(argList, cfgOptOnline);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'online' is negated multiple times");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("duplicate reset");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgKeyRawReset(argList, cfgOptPgHost, 1);
        hrnCfgArgKeyRawReset(argList, cfgOptPgHost, 1);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'pg1-host' is reset multiple times");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option set and negated");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawNegate(argList, cfgOptConfig);
        hrnCfgArgRawZ(argList, cfgOptConfig, "/etc/config");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'config' cannot be set and negated");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option set and reset");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawReset(argList, cfgOptLogPath);
        hrnCfgArgRawZ(argList, cfgOptLogPath, "/var/log");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'log-path' cannot be set and reset");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option negated and reset");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawNegate(argList, cfgOptDelta);
        hrnCfgArgRawReset(argList, cfgOptDelta);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'delta' cannot be negated and reset");

        // reverse the option order
        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawReset(argList, cfgOptDelta);
        hrnCfgArgRawNegate(argList, cfgOptDelta);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'delta' cannot be negated and reset");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("duplicate options");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawBool(argList, cfgOptDelta, true);
        hrnCfgArgRawBool(argList, cfgOptDelta, true);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'delta' cannot be set multiple times");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptCompressLevel, "3");
        hrnCfgArgRawZ(argList, cfgOptCompressLevel, "3");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'compress-level' cannot be set multiple times");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command missing");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawBool(argList, cfgOptOnline, true);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), CommandRequiredError,
            "no command found");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid values");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptManifestSaveThreshold, "123Q");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'123Q' is not valid for 'manifest-save-threshold' option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptManifestSaveThreshold, "999999999999999999p");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'999999999999999999p' is not valid for 'manifest-save-threshold' option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptManifestSaveThreshold, "999t");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'999t' is out of range for 'manifest-save-threshold' option\n"
            "HINT: allowed range is 1B to 1TiB inclusive");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("value missing");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'' must be >= 1 character for 'pg1-path' option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid path values");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "bogus");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'bogus' must begin with / for 'pg1-path' option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        strLstAddZ(argList, "--stanz=db");                          // Partial option to test matching
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/path1//path2");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'/path1//path2' cannot contain // for 'pg1-path' option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/path1/path2//");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'/path1/path2//' cannot contain // for 'pg1-path' option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("non-default roles should not modify log levels");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPg, "2");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/path/to/1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 2, "/path/to/2");
        hrnCfgArgRawZ(argList, cfgOptProcess, "1");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
        hrnCfgArgRawZ(argList, cfgOptLogLevelStderr, "info");
        strLstAddZ(argList, CFGCMD_BACKUP ":" CONFIG_COMMAND_ROLE_LOCAL);

        hrnLogLevelStdOutSet(logLevelError);
        hrnLogLevelStdErrSet(logLevelError);
        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList)), "load local config");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptPgPath), "/path/to/2", "default pg-path");
        TEST_RESULT_INT(cfgCommandRole(), cfgCmdRoleLocal, "command role is local");
        TEST_RESULT_BOOL(cfgLockRequired(), false, "backup:local command does not require lock");
        TEST_RESULT_STR_Z(cfgCommandRoleName(), "backup:local", "command/role name is backup:local");
        TEST_RESULT_INT(hrnLogLevelStdOut(), logLevelError, "console logging is error");
        TEST_RESULT_INT(hrnLogLevelStdErr(), logLevelError, "stderr logging is error");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to");
        hrnCfgArgRawZ(argList, cfgOptProcess, "1");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
        hrnCfgArgRawZ(argList, cfgOptLogLevelStderr, "info");
        strLstAddZ(argList, CFGCMD_BACKUP ":" CONFIG_COMMAND_ROLE_REMOTE);

        hrnLogLevelStdOutSet(logLevelError);
        hrnLogLevelStdErrSet(logLevelError);
        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList)), "load remote config");
        TEST_RESULT_INT(cfgCommandRole(), cfgCmdRoleRemote, "command role is remote");
        TEST_RESULT_STR_Z(cfgParseCommandRoleStr(cfgCmdRoleRemote), "remote", "remote role name");
        TEST_RESULT_INT(hrnLogLevelStdOut(), logLevelError, "console logging is error");
        TEST_RESULT_INT(hrnLogLevelStdErr(), logLevelError, "stderr logging is error");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptLogLevelStderr, "info");
        strLstAddZ(argList, CFGCMD_ARCHIVE_GET ":" CONFIG_COMMAND_ROLE_ASYNC);

        hrnLogLevelStdOutSet(logLevelError);
        hrnLogLevelStdErrSet(logLevelError);
        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList)), "load async config");
        TEST_RESULT_INT(cfgCommandRole(), cfgCmdRoleAsync, "command role is async");
        TEST_RESULT_INT(hrnLogLevelStdOut(), logLevelError, "console logging is error");
        TEST_RESULT_INT(hrnLogLevelStdErr(), logLevelError, "stderr logging is error");

        harnessLogLevelReset();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("required options missing");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionRequiredError,
            "backup command requires option: pg1-path\nHINT: does this stanza exist?");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionRequiredError,
            "backup command requires option: stanza");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command-line option not allowed");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoType, 1, "s3");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoS3Key, 1, "xxx");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoS3Bucket, 1, "xxx");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoS3Endpoint, 1, "xxx");
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'repo1-s3-key' is not allowed on the command-line\n"
            "HINT: this option could expose secrets in the process list.\n"
            "HINT: specify the option in a configuration file or an environment variable instead.");

        // These tests cheat a bit but there may not always be a beta option available for more general testing
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("beta option dependency");

        // Need to parse a command so there will be a valid index for the error message
        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        strLstAddZ(argList, TEST_COMMAND_BACKUP);

        ParseOption betaOption = {0};

        TEST_ERROR(
            parseOptionBeta(cfgOptRepoPath, 0, true, &betaOption), OptionInvalidError,
            "option 'repo1-path' is not valid without option 'beta'\n"
            "HINT: beta features require the --beta option to prevent accidental usage.");

        betaOption.indexListTotal = 1;
        ParseOptionValue betaOptionValue = {.negate = true};
        betaOption.indexList = &betaOptionValue;

        TEST_ERROR(
            parseOptionBeta(cfgOptRepoPath, 0, true, &betaOption), OptionInvalidError,
            "option 'repo1-path' is not valid without option 'beta'\n"
            "HINT: beta features require the --beta option to prevent accidental usage.");

        betaOption.indexList[0].negate = false;

        TEST_RESULT_VOID(parseOptionBeta(cfgOptRepoPath, 0, true, &betaOption), "beta option set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("dependent option missing");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawNegate(argList, cfgOptConfig);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoS3Bucket, 1, "xxx");
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'repo1-s3-bucket' not valid without option 'repo1-type' = 's3'");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHostUser, 1, "xxx");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'repo1-host-user' not valid without option 'repo1-host'");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawBool(argList, cfgOptForce, true);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'force' not valid without option 'no-online'");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawBool(argList, cfgOptRepoBlock, true);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'repo1-block' not valid without option 'repo1-bundle'");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, "/path/to/spool");
        strLstAddZ(argList, TEST_COMMAND_ARCHIVE_GET);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'spool-path' not valid without option 'archive-async'");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawBool(argList, cfgOptTargetExclusive, true);
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'target-exclusive' not valid without option 'type' in ('lsn', 'time', 'xid')");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("dependent option missing (indexed option depends on non-indexed option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        strLstAddZ(argList, CFGCMD_CHECK);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'pg1-path' not valid without option 'stanza'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option invalid for command");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "a=b");
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'recovery-option' not valid for command 'backup'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option value invalid (string)");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptType, "^bogus");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'^bogus' is not allowed for 'type' option\n"
            "HINT: allowed values are 'lsn', 'name', 'time', 'xid', 'preserve', 'none', 'immediate', 'default', 'standby'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option value invalid (size)");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptBufferSize, "777");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'777' is not allowed for 'buffer-size' option\n"
            "HINT: allowed values are '16KiB', '32KiB', '64KiB', '128KiB', '256KiB', '512KiB', '1MiB', '2MiB', '4MiB', '8MiB',"
            " '16MiB'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option value not in allowed list");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptType, "bogus");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'bogus' is not allowed for 'type' option\n"
            "HINT: allowed values are 'lsn', 'name', 'time', 'xid', 'preserve', 'none', 'immediate', 'default', 'standby'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("lower and upper bounds for integer ranges");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptProcessMax, "0");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'0' is out of range for 'process-max' option\n"
            "HINT: allowed range is 1 to 999 inclusive");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptCompressLevelNetwork, "-6");
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'-6' is out of range for 'compress-level-network' option\n"
            "HINT: allowed range is -5 to 12 inclusive");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptCompressLevelNetwork, "-5");
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "load config");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevelNetwork), -5, "compress level is set");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptProcessMax, "65536");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'65536' is out of range for 'process-max' option\n"
            "HINT: allowed range is 1 to 999 inclusive");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptCompressType, "gz");
        hrnCfgArgRawZ(argList, cfgOptCompressLevel, "-2");
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'-2' is out of range for 'compress-level' option when 'compress-type' option = 'gz'\n"
            "HINT: allowed range is -1 to 9 inclusive");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("character value when integer expected");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptProcessMax, "bogus");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'bogus' is not valid for 'process-max' option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid time values");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptProtocolTimeout, "10ms");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'10ms' is out of range for 'protocol-timeout' option\n"
            "HINT: allowed range is 100ms to 7d inclusive");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptProtocolTimeout, "bogus");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "'bogus' is not valid for 'protocol-timeout' option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option as environment variables");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgEnvRawZ(cfgOptProtocolTimeout, "");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "environment variable 'protocol-timeout' must have a value");

        hrnCfgEnvRemoveRaw(cfgOptProtocolTimeout);

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgEnvRawZ(cfgOptDelta, "x");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "environment boolean option 'delta' must be 'y' or 'n'");

        hrnCfgEnvRemoveRaw(cfgOptDelta);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config file - invalid values");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRaw(argList, cfgOptConfig, configFile);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);

        storagePutP(
            storageNewWriteP(storageTestWrite, configFile),
            BUFSTRDEF(
                "[global]\n"
                "delta=bogus\n"));

        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "boolean option 'delta' must be 'y' or 'n'");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRaw(argList, cfgOptConfig, configFile);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);

        storagePutP(
            storageNewWriteP(storageTestWrite, configFile),
            BUFSTRDEF(
                "[global]\n"
                "delta=\n"));

        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidValueError,
            "section 'global', key 'delta' must have a value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config file - duplicate deprecated/new option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRaw(argList, cfgOptConfig, configFile);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);

        storagePutP(
            storageNewWriteP(storageTestWrite, configFile),
            BUFSTRDEF(
                "[db]\n"
                "pg1-path=/path/to/db\n"
                "db-path=/also/path/to/db\n"));

        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "configuration file contains duplicate options ('db-path', 'pg1-path') in section '[db]'");
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true, .noConfigLoad = true),
            OptionInvalidError, "configuration file contains duplicate options ('db-path', 'pg1-path') in section '[db]'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config file - option set multiple times");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRaw(argList, cfgOptConfig, configFile);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);

        storagePutP(
            storageNewWriteP(storageTestWrite, configFile),
            BUFSTRDEF(
                "[db]\n"
                "pg1-path=/path/to/db\n"
                "pg1-path=/also/path/to/db\n"));

        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'pg1-path' cannot be set multiple times");

        // Also test with a boolean option since this gets converted immediately and will blow up if it is multi
        storagePutP(
            storageNewWriteP(storageTestWrite, configFile),
            BUFSTRDEF(
                "[db]\n"
                "start-fast=y\n"
                "start-fast=n\n"));

        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'start-fast' cannot be set multiple times");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("test that log levels are set correctly when reset is enabled, then set them back to harness defaults");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);

        hrnLogLevelStdOutSet(logLevelOff);
        hrnLogLevelStdErrSet(logLevelOff);
        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList)), "no command");
        TEST_RESULT_BOOL(cfgCommandHelp(), false, "help is not set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdHelp, "command is help");
        TEST_RESULT_INT(hrnLogLevelStdOut(), logLevelWarn, "console logging is warn");
        TEST_RESULT_INT(hrnLogLevelStdErr(), logLevelOff, "stderr logging is off");
        harnessLogLevelReset();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("help command");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "help");

        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "help command");
        TEST_RESULT_BOOL(cfgCommandHelp(), false, "command help is not set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdHelp, "command is help");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "version");

        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "help for version command");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "command help is set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdVersion, "command is version");
        TEST_RESULT_Z(cfgCommandName(), "version", "command name is version");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "help");

        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "load config");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "command help is set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdHelp, "command is help");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("help option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--help");

        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "load config");
        TEST_RESULT_INT(cfgCommand(), cfgCmdHelp, "command is help");
        TEST_RESULT_BOOL(cfgCommandHelp(), false, "command help is not set");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptHelp), true, "help option is set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("version option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--version");

        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "load config");
        TEST_RESULT_BOOL(cfgCommandHelp(), false, "command help is not set");
        TEST_RESULT_UINT(cfgCommand(), cfgCmdHelp, "command is help");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptVersion), true, "version option is set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("help and version options");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--help");
        strLstAddZ(argList, "--version");

        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "load config");
        TEST_RESULT_BOOL(cfgCommandHelp(), false, "help is not set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdHelp, "command is help");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptHelp), true, "version option is set");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptVersion), true, "version option is set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on command with --help option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--help");
        strLstAddZ(argList, "backup");

        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'help' not valid for command 'backup'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on command with --version option");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "--version");
        strLstAddZ(argList, "backup");

        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "option 'version' not valid for command 'backup'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("help command - should not fail on missing options");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, "help");
        strLstAddZ(argList, "backup");

        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "help for backup command");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "help is set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdBackup, "command is backup");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptPgPath), true, "pg1-path is valid");
        TEST_RESULT_PTR(cfgOptionVar(cfgOptPgPath), NULL, "pg1-path is not set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command argument valid");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, TEST_COMMAND_ARCHIVE_GET);
        strLstAddZ(argList, "000000010000000200000003");
        strLstAddZ(argList, "/path/to/wal/RECOVERYWAL");
        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "command arguments");
        TEST_RESULT_STRLST_Z(
            cfgCommandParam(), "000000010000000200000003\n/path/to/wal/RECOVERYWAL\n", "check command arguments");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bool-like");

        StringList *argListBase = strLstNew();
        strLstAddZ(argListBase, TEST_BACKREST_EXE);
        strLstAddZ(argListBase, "backup");
        strLstAddZ(argListBase, "--stanza=test");
        strLstAddZ(argListBase, "--pg1-path=/");

        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--no-backup-standby");

        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "negate");
        TEST_RESULT_UINT(cfgOptionStrId(cfgOptBackupStandby), CFGOPTVAL_BACKUP_STANDBY_N, "backup-standby is n");

        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--backup-standby");

        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "no arg");
        TEST_RESULT_UINT(cfgOptionStrId(cfgOptBackupStandby), CFGOPTVAL_BACKUP_STANDBY_Y, "backup-standby is y");

        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--backup-standby=prefer");

        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "prefer arg");
        TEST_RESULT_UINT(cfgOptionStrId(cfgOptBackupStandby), CFGOPTVAL_BACKUP_STANDBY_PREFER, "backup-standby is prefer");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("various configuration settings");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db/");
        hrnCfgArgRawNegate(argList, cfgOptOnline);
        hrnCfgArgRawNegate(argList, cfgOptConfig);
        strLstAddZ(argList, "--repo1-type");
        strLstAddZ(argList, "s3");                                  // Argument for the option above
        hrnCfgArgKeyRawZ(argList, cfgOptRepoS3Bucket, 1, " test ");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoS3Endpoint, 1, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoS3Region, 1, "test");
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        hrnCfgEnvKeyRawZ(cfgOptRepoS3Key, 1, "xxx");
        hrnCfgEnvKeyRawZ(cfgOptRepoS3KeySecret, 1, "xxx");
        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true),
            TEST_COMMAND_BACKUP " command");
        TEST_RESULT_INT(cfgCommand(), cfgCmdBackup, "command is " TEST_COMMAND_BACKUP);
        TEST_RESULT_BOOL(cfgLockRequired(), true, "backup command requires lock");
        TEST_RESULT_UINT(cfgLockType(), lockTypeBackup, "backup command requires backup lock type");
        TEST_RESULT_UINT(cfgLogLevelDefault(), logLevelInfo, "backup defaults to log level warn");
        TEST_RESULT_BOOL(cfgLogFile(), true, "backup command does file logging");
        TEST_RESULT_BOOL(cfgLockRemoteRequired(), true, "backup command requires remote lock");
        TEST_RESULT_STRLST_Z(cfgCommandParam(), NULL, "check command arguments");
        TEST_RESULT_UINT(cfgParseCommandRoleEnum(NULL), cfgCmdRoleMain, "command role main enum");
        TEST_ERROR(cfgParseCommandRoleEnum("bogus"), CommandInvalidError, "invalid command role 'bogus'");
        TEST_RESULT_INT(cfgCommandRole(), cfgCmdRoleMain, "command role is main");
        TEST_RESULT_STR_Z(cfgCommandRoleName(), "backup", "command/role name is backup");
        TEST_RESULT_STR_Z(cfgParseCommandRoleStr(cfgCmdRoleMain), NULL, "main role name is NULL");

        TEST_RESULT_STR_Z(cfgBin(), TEST_BACKREST_EXE, "bin is set");

        TEST_RESULT_BOOL(cfgOptionTest(cfgOptConfig), false, "config is not set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptConfig), cfgSourceParam, "config is source param");
        TEST_RESULT_BOOL(cfgOptionIdxNegate(cfgOptConfig, 0), true, "config is negated");
        TEST_RESULT_INT(cfgOptionSource(cfgOptStanza), cfgSourceParam, "stanza is source param");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptStanza), "db", "stanza is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptStanza), cfgSourceParam, "stanza is source param");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptPgPath, 0), "/path/to/db", "pg1-path is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptPgPath), cfgSourceParam, "pg1-path is source param");
        TEST_RESULT_UINT(cfgOptionIdxStrId(cfgOptRepoType, 0), strIdFromZ("s3"), "repo-type is set");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoS3Bucket), " test ", "repo1-s3-bucket is set and preserves spaces");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoS3KeySecret), "xxx", "repo1-s3-secret is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptRepoS3KeySecret), cfgSourceConfig, "repo1-s3-secret is source env");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptOnline), false, "online is not set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptOnline), cfgSourceParam, "online is source default");
        TEST_RESULT_INT(cfgOptionIdxInt(cfgOptBufferSize, 0), 1048576, "buffer-size is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBufferSize), cfgSourceDefault, "buffer-size is source default");
        TEST_RESULT_Z(cfgOptionName(cfgOptBufferSize), "buffer-size", "buffer-size name");

        hrnCfgEnvKeyRemoveRaw(cfgOptRepoS3Key, 1);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoS3KeySecret, 1);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("global is a valid stanza prefix");

        storagePutP(
            storageNewWriteP(storageTestWrite, configFile),
            BUFSTRDEF(
                "[global_stanza]\n"
                "pg1-path=/path/to/global/stanza\n"));

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptStanza, "global_stanza");
        hrnCfgArgRaw(argList, cfgOptConfig, configFile);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);

        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "parse config");

        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptPgPath), "/path/to/global/stanza", "default pg-path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("warnings for environment variables, command-line and config file options");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRaw(argList, cfgOptConfig, configFile);
        hrnCfgArgRawNegate(argList, cfgOptOnline);
        hrnCfgArgKeyRawBool(argList, cfgOptPgLocal, 2, true);
        hrnCfgArgKeyRawReset(argList, cfgOptPgHost, 1);
        hrnCfgArgKeyRawReset(argList, cfgOptPgHost, 3);
        hrnCfgArgRawReset(argList, cfgOptBackupStandby);
        strLstAddZ(argList, "--retention-ful=55");                  // Partial match for deprecated option
        strLstAddZ(argList, TEST_COMMAND_BACKUP);

        setenv("PGBACKRESTXXX_NOTHING", "xxx", true);
        setenv("PGBACKREST_BOGUS", "xxx", true);
        setenv("PGBACKREST_ONLIN", "xxx", true);                    // Option prefix matching not allowed in environment
        setenv("PGBACKREST_NO_DELTA", "xxx", true);
        setenv("PGBACKREST_RESET_REPO1_HOST", "", true);
        setenv("PGBACKREST_TARGET", "xxx", true);
        setenv("PGBACKREST_ONLINE", "y", true);
        setenv("PGBACKREST_DELTA", "y", true);
        setenv("PGBACKREST_START_FAST", "n", true);
        setenv("PGBACKREST_PG1_SOCKET_PATH", "@socket", true);

        storagePutP(
            storageNewWriteP(storageTestWrite, configFile),
            BUFSTR(
                strNewFmt(
                    "[global]\n"
                    "compress-type=gz\n"
                    "spool-path=/path/to/spool\n"
                    "lock-path=/\n"
                    "pg1-path=/not/path/to/db\n"
                    "\n"
                    "[global:backup]\n"
                    "repo1-hardlink=y\n"
                    "bogus=bogus\n"
                    "no-delta=y\n"
                    "reset-delta=y\n"
                    "archive-copy=y\n"
                    "start-fast=y\n"
                    "online=y\n"
                    "pg1-path=/not/path/to/db\n"
                    "backup-standby=y\n"
                    "backup-standb=y\n"                             // Option prefix matching not allowed in config files
                    "buffer-size=65536\n"
                    "protocol-timeout=3600\n"
                    CFGOPT_JOB_RETRY "=3\n"
                    CFGOPT_JOB_RETRY_INTERVAL "=33\n"
                    "\n"
                    "[db:backup]\n"
                    "delta=n\n"
                    "recovery-option=a=b\n"
                    "\n"
                    "[db]\n"
                    "pg1-host=db\n"
                    "pg1-path=/path/to/db\n"
                    "pg256-path=/path/to/db256\n"
                    "%s=ignore\n"
                    "%s=/path/to/db2\n"
                    "pg3-host=ignore\n"
                    "repo1-host=ssh-host\n"
                    "recovery-option=c=d\n",
                    cfgParseOptionKeyIdxName(cfgOptPgHost, 1), cfgParseOptionKeyIdxName(cfgOptPgPath, 1))));

        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true),
            TEST_COMMAND_BACKUP " command");
        TEST_RESULT_LOG(
            "P00   WARN: environment contains invalid option 'bogus'\n"
            "P00   WARN: environment contains invalid option 'onlin'\n"
            "P00   WARN: environment contains invalid negate option 'no-delta'\n"
            "P00   WARN: environment contains invalid reset option 'reset-repo1-host'\n"
            "P00   WARN: configuration file contains option 'recovery-option' invalid for section 'db:backup'\n"
            "P00   WARN: configuration file contains invalid option 'bogus'\n"
            "P00   WARN: configuration file contains negate option 'no-delta'\n"
            "P00   WARN: configuration file contains reset option 'reset-delta'\n"
            "P00   WARN: configuration file contains command-line only option 'online'\n"
            "P00   WARN: configuration file contains stanza-only option 'pg1-path' in global section 'global:backup'\n"
            "P00   WARN: configuration file contains invalid option 'backup-standb'\n"
            "P00   WARN: configuration file contains stanza-only option 'pg1-path' in global section 'global'");

        TEST_RESULT_PTR(cfgParseIni(), configParseLocal.ini, "parsed ini");
        TEST_RESULT_STRLST_Z(cfgParseStanzaList(), "db\n", "stanza list");
        TEST_RESULT_STR_Z(jsonFromVar(varNewVarLst(cfgCommandJobRetry())), "[0,33000,33000]", "custom job retries");
        TEST_RESULT_BOOL(cfgOptionIdxTest(cfgOptPgHost, 0), false, "pg1-host is not set (command line reset override)");
        TEST_RESULT_BOOL(cfgOptionIdxReset(cfgOptPgHost, 0), true, "pg1-host was reset");
        TEST_RESULT_UINT(cfgOptionGroupIdxDefault(cfgOptGrpPg), 0, "pg1 is default");
        TEST_RESULT_UINT(cfgOptionGroupIdxToKey(cfgOptGrpPg, 1), 2, "pg2 is index 2");
        TEST_RESULT_Z(cfgOptionIdxName(cfgOptPgPath, 0), "pg1-path", "pg1-path option name");
        TEST_RESULT_Z(cfgOptionIdxName(cfgOptPgPath, 1), "pg2-path", "pg2-path option name");
        TEST_RESULT_Z(cfgOptionGroupName(cfgOptGrpPg, 1), "pg2", "pg2 group display");
        TEST_RESULT_Z(cfgOptionGroupName(cfgOptGrpPg, 0), "pg1", "pg1 group display (cached)");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptPgPath), "/path/to/db", "default pg-path");
        TEST_RESULT_STR_Z(varStr(cfgOptionVar(cfgOptPgPath)), "/path/to/db", "default pg-path as variant");
        TEST_RESULT_UINT(cfgOptionGroupIdxTotal(cfgOptGrpPg), 3, "pg1, pg2, and pg256 are set");
        TEST_RESULT_BOOL(cfgOptionIdxBool(cfgOptPgLocal, 1), true, "pg2-local is set");
        TEST_RESULT_BOOL(cfgOptionIdxTest(cfgOptPgHost, 1), false, "pg2-host is not set (pg2-local override)");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptPgPath, cfgOptionKeyToIdx(cfgOptPgPath, 2)), "/path/to/db2", "pg2-path is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptPgPath), cfgSourceConfig, "pg1-path is source config");
        TEST_RESULT_STR_Z(
            cfgOptionIdxStr(cfgOptPgPath, cfgOptionKeyToIdx(cfgOptPgPath, 256)), "/path/to/db256", "pg256-path is set");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptRepoHost, 0), "ssh-host", "repo1-host is set");
        TEST_RESULT_BOOL(cfgOptionIdxTest(cfgOptRepoHostPort, 0), false, "repo1-host-port is not set for ssh");
        TEST_RESULT_UINT(varUInt64(cfgOptionVar(cfgOptType)), STRID5("incr", 0x90dc90), "check type");
        TEST_RESULT_STR_Z(
            cfgOptionDisplayVar(VARUINT64(STRID5("incr", 0x90dc90)), cfgOptTypeStringId), "incr", "check type display");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptLockPath), "/", "lock-path is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptLockPath), cfgSourceConfig, "lock-path is source config");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptPgSocketPath, 0), "@socket", "pg1-socket-path is set");
        TEST_RESULT_INT(cfgOptionIdxSource(cfgOptPgSocketPath, 0), cfgSourceConfig, "pg1-socket-path is config param");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptOnline), false, "online not is set");
        TEST_RESULT_STR_Z(cfgOptionDisplay(cfgOptOnline), "false", "online display is false");
        TEST_RESULT_INT(cfgOptionSource(cfgOptOnline), cfgSourceParam, "online is source param");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptStartFast), false, "start-fast not is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptStartFast), cfgSourceConfig, "start-fast is config param");
        TEST_RESULT_UINT(cfgOptionIdxTotal(cfgOptDelta), 1, "delta not indexed");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptDelta), true, "delta not set");
        TEST_RESULT_BOOL(varBool(cfgOptionVar(cfgOptDelta)), true, "delta as variant");
        TEST_RESULT_INT(cfgOptionSource(cfgOptDelta), cfgSourceConfig, "delta is source config");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptArchiveCheck), false, "archive-check is false");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptArchiveCopy), false, "archive-copy is false");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptRepoHardlink), true, "repo-hardlink is set");
        TEST_RESULT_STR_Z(cfgOptionDisplay(cfgOptRepoHardlink), "true", "repo-hardlink display is true");
        TEST_RESULT_INT(cfgOptionSource(cfgOptRepoHardlink), cfgSourceConfig, "repo-hardlink is source config");
        TEST_RESULT_UINT(cfgOptionUInt(cfgOptRepoRetentionFull), 55, "repo-retention-full is set");
        TEST_RESULT_INT(varInt64(cfgOptionVar(cfgOptRepoRetentionFull)), 55, "repo-retention-full as variant");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 6, "compress-level is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptCompressLevel), cfgSourceDefault, "compress-level is source config");
        TEST_RESULT_UINT(cfgOptionStrId(cfgOptBackupStandby), CFGOPTVAL_BACKUP_STANDBY_N, "backup-standby not is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBackupStandby), cfgSourceDefault, "backup-standby is source default");
        TEST_RESULT_BOOL(cfgOptionIdxReset(cfgOptBackupStandby, 0), true, "backup-standby was reset");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptDelta), true, "delta is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptDelta), cfgSourceConfig, "delta is source config");
        TEST_RESULT_INT(cfgOptionInt64(cfgOptBufferSize), 65536, "buffer-size is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBufferSize), cfgSourceConfig, "backup-standby is source config");
        TEST_RESULT_UINT(cfgOptionUInt64(cfgOptDbTimeout), 1800000, "db-timeout is set");
        TEST_RESULT_STR_Z(cfgOptionDisplay(cfgOptDbTimeout), "30m", "db-timeout display");
        TEST_RESULT_UINT(cfgOptionUInt64(cfgOptProtocolTimeout), 3600000, "protocol-timeout is set");
        TEST_RESULT_UINT(cfgOptionIdxUInt(cfgOptPgPort, 1), 5432, "pg2-port is set");
        TEST_RESULT_UINT(cfgOptionIdxUInt64(cfgOptPgPort, 1), 5432, "pg2-port is set");
        TEST_RESULT_STR(cfgOptionIdxStrNull(cfgOptPgHost, 1), NULL, "pg2-host is NULL");
        TEST_RESULT_STR(cfgOptionStrNull(cfgOptPgHost), NULL, "pg2-host is NULL");
        TEST_ERROR(cfgOptionIdxStr(cfgOptPgHost, 1), AssertError, "option 'pg2-host' is null but non-null was requested");
        TEST_RESULT_UINT(cfgOptionUInt64(cfgOptIoTimeout), 60000, "io-timeout is set");

        TEST_RESULT_STR_Z(cfgOptionIdxDefaultValue(cfgOptBackupStandby, 0), "n", "backup-standby default is false");
        TEST_RESULT_STR_Z(cfgOptionIdxDefaultValue(cfgOptBackupStandby, 0), "n", "backup-standby default is false (again)");
        TEST_RESULT_PTR(cfgOptionIdxDefaultValue(cfgOptPgHost, 0), NULL, "pg-host default is NULL");
        TEST_RESULT_STR_Z(cfgOptionIdxDefaultValue(cfgOptLogLevelConsole, 0), "warn", "log-level-console default is warn");
        TEST_RESULT_STR_Z(cfgOptionIdxDefaultValue(cfgOptPgPort, 0), "5432", "pg-port default is 5432");
        TEST_RESULT_STR_Z(cfgOptionIdxDisplay(cfgOptPgPort, 0), "5432", "pg-port display is 5432");
        TEST_RESULT_VOID(cfgOptionSet(cfgOptDbTimeout, cfgSourceDefault, VARINT64(30000)), "set db-timeout default");
        TEST_RESULT_STR_Z(cfgOptionIdxDefaultValue(cfgOptDbTimeout, 0), "30", "db-timeout default is 30m");

        TEST_RESULT_VOID(
            cfgOptionIdxSet(cfgOptPgSocketPath, 1, cfgSourceDefault, VARSTRDEF("/default")), "set pg-socket-path default");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptPgSocketPath, 0), "@socket", "pg1-socket-path unchanged");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptPgSocketPath, 1), "/default", "pg2-socket-path is new default");
        TEST_RESULT_STR_Z(cfgOptionIdxDisplay(cfgOptPgSocketPath, 1), "/default", "pg2-socket-path display");
        TEST_RESULT_UINT(cfgOptionIdxTotal(cfgOptPgSocketPath), 3, "pg-socket-path index total");

        TEST_ERROR(cfgOptionDisplay(cfgOptTarget), AssertError, "option 'target' is not valid for the current command");
        TEST_ERROR(cfgOptionLst(cfgOptDbInclude), AssertError, "option 'db-include' is not valid for the current command");
        TEST_ERROR(cfgOptionKv(cfgOptPgPath), AssertError, "option 'pg1-path' is type 4 but 1 was requested");

        TEST_RESULT_VOID(cfgOptionInvalidate(cfgOptPgPath), "invalidate pg-path");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptPgPath), false, "pg-path no longer valid");

        TEST_RESULT_UINT(cfgOptionKeyToIdx(cfgOptArchiveTimeout, 1), 0, "check archive-timeout");
        TEST_ERROR(cfgOptionKeyToIdx(cfgOptPgPath, 4), AssertError, "key '4' is not valid for 'pg-path' option");

        unsetenv("PGBACKREST_BOGUS");
        unsetenv("PGBACKREST_ONLIN");
        unsetenv("PGBACKREST_NO_DELTA");
        unsetenv("PGBACKREST_RESET_REPO1_HOST");
        unsetenv("PGBACKREST_TARGET");
        unsetenv("PGBACKREST_ONLINE");
        unsetenv("PGBACKREST_START_FAST");
        unsetenv("PGBACKREST_PG1_SOCKET_PATH");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("set command to expire");

        TEST_RESULT_VOID(cfgCommandSet(cfgCmdExpire, cfgParseCommandRoleEnum("async")), "set command");
        TEST_RESULT_STR_Z(cfgCommandRoleName(), "expire:async", "command/role name is expire:async");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive-push command");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRaw(argList, cfgOptConfig, configFile);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptArchivePushQueueMax, "4503599627370496");
        hrnCfgArgRawZ(argList, cfgOptBufferSize, "2MB");
        strLstAddZ(argList, "archive-push:async");

        storagePutP(
            storageNewWriteP(storageTestWrite, configFile),
            BUFSTRDEF(
                "[global]\n"
                "spool-path=/path/to/spool\n"));

        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "archive-push command");

        TEST_RESULT_STR_Z(jsonFromVar(varNewVarLst(cfgCommandJobRetry())), "[0]", "default job retry");
        TEST_RESULT_BOOL(cfgLockRequired(), true, "archive-push:async command requires lock");
        TEST_RESULT_BOOL(cfgLogFile(), true, "archive-push:async command does file logging");
        TEST_RESULT_INT(cfgOptionInt64(cfgOptArchivePushQueueMax), 4503599627370496, "archive-push-queue-max is set");
        TEST_RESULT_UINT(cfgOptionUInt64(cfgOptArchivePushQueueMax), 4503599627370496, "archive-push-queue-max is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptArchivePushQueueMax), cfgSourceParam, "archive-push-queue-max is source config");
        TEST_RESULT_INT(cfgOptionIdxInt64(cfgOptBufferSize, 0), 2097152, "buffer-size is set to bytes from MB");
        TEST_RESULT_STR_Z(cfgOptionDisplay(cfgOptBufferSize), "2MB", "buffer-size display is 2MB");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBufferSize), cfgSourceParam, "buffer-size is source config");
        TEST_RESULT_PTR(cfgOptionVar(cfgOptSpoolPath), NULL, "spool-path is not set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptSpoolPath), cfgSourceDefault, "spool-path is source default");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid key/value");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "a");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_ERROR(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), OptionInvalidError,
            "key/value 'a' not valid for 'recovery-option' option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option cmd");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/path/to/1");
        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "load local config");

        TEST_RESULT_STR_Z(cfgBin(), "pgbackrest", "--cmd not provided; cfgBin() returns " TEST_BACKREST_EXE);

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/path/to/1");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptCmd), TEST_PROJECT_EXE, "--cmd not provided; cmd is defaulted to " TEST_PROJECT_EXE);

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_BACKUP);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/path/to/1");
        hrnCfgArgRawZ(argList, cfgOptCmd, "pgbackrest_wrapper.sh");
        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "load local config");

        TEST_RESULT_STR_Z(
            cfgOptionStr(cfgOptCmd), "pgbackrest_wrapper.sh", "--cmd provided; cmd is returned as pgbackrest_wrapper.sh");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("default job retry and valid duplicate options");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "abc");
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "def");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true),
            TEST_COMMAND_RESTORE " command");

        TEST_RESULT_STR_Z(jsonFromVar(varNewVarLst(cfgCommandJobRetry())), "[0,15000]", "default job retry");

        const VariantList *includeList = NULL;
        TEST_ASSIGN(includeList, varVarLst(cfgOptionVar(cfgOptDbInclude)), "get db include options");
        TEST_RESULT_STR_Z(varStr(varLstGet(includeList, 0)), "abc", "check db include option");
        TEST_RESULT_STR_Z(varStr(varLstGet(includeList, 1)), "def", "check db include option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery options, no job retry, command-line options only");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/db");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "a=b");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "c=d");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "c=de=fg hi");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptJobRetry, "0");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);
        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true),
            TEST_COMMAND_RESTORE " command");

        TEST_RESULT_LOG("P00   WARN: key 'c' value 'd' is overwritten with 'de=fg hi' for 'recovery-option' option");

        TEST_RESULT_PTR(cfgCommandJobRetry(), NULL, "no job retries");

        const KeyValue *recoveryKv = NULL;
        TEST_RESULT_PTR(cfgOptionKvNull(cfgOptLinkMap), NULL, "link map is null");
        TEST_RESULT_PTR(cfgOptionIdxKvNull(cfgOptLinkMap, 0), NULL, "link map is null");
        TEST_ASSIGN(recoveryKv, cfgOptionKv(cfgOptRecoveryOption), "get recovery options");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, VARSTRDEF("a"))), "b", "check recovery option");
        TEST_ASSIGN(recoveryKv, varKv(cfgOptionIdxVar(cfgOptRecoveryOption, 0)), "get recovery options");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, VARSTRDEF("c"))), "de=fg hi", "check recovery option");
        TEST_RESULT_BOOL(cfgLockRequired(), true, "restore command requires lock");
        TEST_RESULT_UINT(cfgLockType(), lockTypeRestore, "restore command requires restore lock type");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery options, config file");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRaw(argList, cfgOptConfig, configFile);
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        strLstAddZ(argList, TEST_COMMAND_RESTORE);

        storagePutP(
            storageNewWriteP(storageTestWrite, configFile),
            BUFSTRDEF(
                "[global:restore]\n"
                "recovery-option=f=g\n"
                "recovery-option=hijk=l\n"
                "recovery-option=hijk=override\n"                   // Overrides prior value of hijk
                "\n"
                "[db]\n"
                "pg1-path=/path/to/db\n"));

        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true),
            TEST_COMMAND_RESTORE " command");

        TEST_RESULT_LOG("P00   WARN: key 'hijk' value 'l' is overwritten with 'override' for 'recovery-option' option");

        TEST_ASSIGN(recoveryKv, cfgOptionKv(cfgOptRecoveryOption), "get recovery options");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, VARSTRDEF("f"))), "g", "check recovery option");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, VARSTRDEF("hijk"))), "override", "check recovery option");
        TEST_RESULT_UINT(varLstSize(cfgOptionLst(cfgOptDbInclude)), 0, "check db include option size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery options, environment variables only");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        strLstAddZ(argList, TEST_COMMAND_RESTORE);

        hrnCfgEnvRawZ(cfgOptStanza, "db");
        hrnCfgEnvKeyRawZ(cfgOptPgPath, 1, "/path/to/db");
        hrnCfgEnvRawZ(cfgOptRecoveryOption, "f=g:hijk=l");
        hrnCfgEnvRawZ(cfgOptDbInclude, "77");

        TEST_RESULT_VOID(
            cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true),
            TEST_COMMAND_RESTORE " command");

        TEST_ASSIGN(recoveryKv, cfgOptionKv(cfgOptRecoveryOption), "get recovery options");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, VARSTRDEF("f"))), "g", "check recovery option");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, VARSTRDEF("hijk"))), "l", "check recovery option");
        TEST_RESULT_STR_Z(varStr(varLstGet(cfgOptionLst(cfgOptDbInclude), 0)), "77", "check db include option");
        TEST_RESULT_UINT(varLstSize(cfgOptionLst(cfgOptDbInclude)), 1, "check db include option size");

        hrnCfgEnvRemoveRaw(cfgOptStanza);
        hrnCfgEnvKeyRemoveRaw(cfgOptPgPath, 1);
        hrnCfgEnvRemoveRaw(cfgOptRecoveryOption);
        hrnCfgEnvRemoveRaw(cfgOptDbInclude);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cfgOptionSet() and cfgOptionIdxSet()");

        TEST_RESULT_VOID(cfgOptionSet(cfgOptForce, cfgSourceParam, BOOL_FALSE_VAR), "set boolean");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptForce), false, "check boolean");

        TEST_RESULT_VOID(cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, VARINT64(1000)), "set protocol-timeout to 1");
        TEST_RESULT_UINT(cfgOptionUInt64(cfgOptProtocolTimeout), 1000, "check protocol-timeout");
        TEST_RESULT_VOID(cfgOptionSet(cfgOptProtocolTimeout, cfgSourceParam, VARINT64(2200)), "set protocol-timeout to 2.2");
        TEST_RESULT_UINT(cfgOptionUInt64(cfgOptProtocolTimeout), 2200, "check protocol-timeout");

        TEST_RESULT_VOID(cfgOptionSet(cfgOptProcessMax, cfgSourceParam, VARINT64(51)), "set process-max to 51");
        TEST_RESULT_INT(cfgOptionInt(cfgOptProcessMax), 51, "check process-max");

        TEST_ERROR(
            cfgOptionSet(cfgOptDbInclude, cfgSourceParam, VARINT(1)), AssertError, "set not available for option data type 3");

        TEST_ERROR(
            cfgOptionIdxSet(cfgOptPgPath, 0, cfgSourceParam, VARINT(1)), AssertError,
            "option 'pg1-path' must be set with String variant");
        TEST_RESULT_VOID(cfgOptionIdxSet(cfgOptPgPath, 0, cfgSourceParam, VARSTRDEF("/new")), "set pg1-path");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptPgPath, 0), "/new", "check pg1-path");

        TEST_RESULT_VOID(cfgOptionIdxSet(cfgOptPgPath, 0, cfgSourceParam, NULL), "set pg1-path to NULL");
        TEST_RESULT_STR_Z(cfgOptionIdxStrNull(cfgOptPgPath, 0), NULL, "check pg1-path");

        TEST_RESULT_VOID(cfgOptionIdxSet(cfgOptType, 0, cfgSourceParam, VARUINT64(STRID5("preserve", 0x2da45996500))), "set type");
        TEST_RESULT_UINT(cfgOptionIdxStrId(cfgOptType, 0), STRID5("preserve", 0x2da45996500), "check type");

        TEST_RESULT_VOID(cfgOptionIdxSet(cfgOptType, 0, cfgSourceParam, VARSTRDEF("standby")), "set type");
        TEST_RESULT_UINT(cfgOptionIdxStrId(cfgOptType, 0), STRID5("standby", 0x6444706930), "check type");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("integer default when dependency invalid");
        {
            argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test");
            hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
            hrnCfgEnvKeyRawZ(cfgOptRepoRetentionFull, 1, "1");
            hrnCfgEnvRawZ(cfgOptCompressType, "none");
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            TEST_RESULT_UINT(cfgOptionUInt(cfgOptCompressLevel), 0, "compress-level is 0");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza options should not be loaded for commands that don't take a stanza");

        argList = strLstNew();
        strLstAddZ(argList, TEST_BACKREST_EXE);
        hrnCfgArgRaw(argList, cfgOptConfig, configFile);
        strLstAddZ(argList, "info");

        storagePutP(
            storageNewWriteP(storageTestWrite, configFile),
            BUFSTRDEF(
                "[global]\n"
                "repo1-path=/path/to/repo\n"
                "\n"
                "[db]\n"
                "repo1-path=/not/the/path\n"));

        TEST_RESULT_VOID(cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true), "info command");
        TEST_RESULT_BOOL(cfgLogFile(), false, "info command does not do file logging");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoPath), "/path/to/repo", "check repo1-path option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info command can be forced to do file logging");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptLogLevelFile, "detail");
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        TEST_RESULT_BOOL(cfgLogFile(), true, "check logging");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("version command does not do file logging");

        HRN_CFG_LOAD(cfgCmdVersion, strLstNew());
        TEST_RESULT_BOOL(cfgLogFile(), false, "check logging");

        // -------------------------------------------------------------------------------------------------------------------------
        // TEST_TITLE("option key 1 is not required");
        //
        // argList = strLstNew();
        // hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        // hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 2, "/pg2");
        // hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 8, "/pg8");
        // HRN_CFG_LOAD(cfgCmdCheck, argList);
        //
        // TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptPgPath, 0), "/pg2", "check pg1-path");
        // TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptPgPath, cfgOptionKeyToIdx(cfgOptPgPath, 8)), "/pg8", "check pg8-path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid pg option");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 128, "/pg128");
        hrnCfgArgRawZ(argList, cfgOptPg, "4");
        TEST_ERROR(
            hrnCfgLoadP(cfgCmdBackup, argList, .role = cfgCmdRoleLocal), OptionInvalidValueError,
            "key '4' is not valid for 'pg' option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove groups indexes that do not have a non-default option");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 5, "repo5");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 112, "/repo112");
        hrnCfgEnvKeyRawZ(cfgOptRepoS3Key, 1, "x1x");
        hrnCfgEnvKeyRawZ(cfgOptRepoS3Key, 3, "x3x");
        hrnCfgEnvKeyRawZ(cfgOptRepoS3Key, 255, "x255x");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        hrnCfgEnvKeyRemoveRaw(cfgOptRepoS3Key, 1);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoS3Key, 3);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoS3Key, 255);

        TEST_RESULT_UINT(cfgOptionGroupIdxTotal(cfgOptGrpRepo), 2, "check repo group total");
        TEST_RESULT_UINT(cfgOptionGroupIdxToKey(cfgOptGrpRepo, 0), 5, "check repo5 key");
        TEST_RESULT_Z(cfgOptionGroupName(cfgOptGrpRepo, 0), "repo5", "check repo5 name");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptRepoHost, 0), "repo5", "check repo5-host");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptRepoPath, 0), "/var/lib/pgbackrest", "check repo5-path");
        TEST_RESULT_UINT(cfgOptionGroupIdxToKey(cfgOptGrpRepo, 1), 112, "check repo112 key");
        TEST_RESULT_Z(cfgOptionGroupName(cfgOptGrpRepo, 1), "repo112", "check repo112 name");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptRepoPath, 1), "/repo112", "check repo112-path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("two indexes with default options (first is remapped to repo1)");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
        hrnCfgEnvKeyRawZ(cfgOptRepoS3Key, 4, "x4x");
        hrnCfgEnvKeyRawZ(cfgOptRepoS3Key, 255, "x255x");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        hrnCfgEnvKeyRemoveRaw(cfgOptRepoS3Key, 4);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoS3Key, 255);

        TEST_RESULT_UINT(cfgOptionGroupIdxTotal(cfgOptGrpRepo), 1, "check repo group total");
        TEST_RESULT_UINT(cfgOptionGroupIdxToKey(cfgOptGrpRepo, 0), 1, "check repo1 key");
        TEST_RESULT_Z(cfgOptionGroupName(cfgOptGrpRepo, 0), "repo1", "check repo1 name");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptRepoPath, 0), "/var/lib/pgbackrest", "check repo1-path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("conditional option value -- value not found and no next value");
        {
            PackWrite *packWrite = pckWriteNewP();
            pckWriteBoolP(packWrite, false, .defaultWrite = true);
            pckWriteEndP(packWrite);

            PackRead *packRead = pckReadNew(pckWriteResult(packWrite));
            pckReadNext(packRead);

            TEST_RESULT_BOOL(cfgParseOptionValueCondition(true, packRead, false, 0, 0, STRDEF("")), false, "check condition");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("conditional option value -- value found");
        {
            argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test");
            hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
            hrnCfgEnvKeyRawZ(cfgOptRepoRetentionFull, 1, "1");
            hrnCfgEnvRawZ(cfgOptCompressType, "lz4");
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            PackWrite *packWrite = pckWriteNewP();
            pckWriteBoolP(packWrite, false, .defaultWrite = true);
            pckWriteEndP(packWrite);

            PackRead *packRead = pckReadNew(pckWriteResult(packWrite));
            pckReadNext(packRead);

            TEST_ERROR(
                cfgParseOptionValueCondition(true, packRead, true, cfgOptCompressType, 0, STRDEF("lz4")), OptionInvalidValueError,
                "pgBackRest not built with 'compress-type=lz4' support\n"
                "HINT: if pgBackRest was installed from a package, does the package support this feature?\n"
                "HINT: if pgBackRest was built from source, were the required development packages installed?");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("version and help (without command) do not load env");
        {
            argList = strLstNew();
            strLstAddZ(argList, "backup");
            setenv("PGBACKREST_INVALID", "xxx", true);

            // Warn on invalid env var
            HRN_CFG_LOAD(cfgCmdHelp, argList);
            TEST_RESULT_LOG("P00   WARN: environment contains invalid option 'invalid'");

            // No warning for invalid env var
            argList = strLstNew();
            HRN_CFG_LOAD(cfgCmdHelp, argList);
            HRN_CFG_LOAD(cfgCmdVersion, argList);

            unsetenv("PGBACKREST_INVALID");
        }

        // -------------------------------------------------------------------------------------------------------------------------
#ifdef TEST_CONTAINER_REQUIRED
        TEST_TITLE("version and help (without command) do not load config or read env");
        {
            HRN_SYSTEM(
                "echo '[global' | sudo tee " PGBACKREST_CONFIG_ORIG_PATH_FILE
                " && sudo chmod 600 " PGBACKREST_CONFIG_ORIG_PATH_FILE);

            argList = strLstNew();
            strLstAddZ(argList, TEST_BACKREST_EXE);
            strLstAddZ(argList, "help");
            strLstAddZ(argList, "backup");

            // Error on unreadable config
            TEST_ERROR(
                cfgParseP(storageTest, strLstSize(argList), strLstPtr(argList), .noResetLogLevel = true),
                FileOpenError, "unable to open file '/etc/pgbackrest.conf' for read: [13] Permission denied");

            // No error on unreadable config
            argList = strLstNew();
            HRN_CFG_LOAD(cfgCmdHelp, argList);
            HRN_CFG_LOAD(cfgCmdVersion, argList);

            HRN_SYSTEM("sudo rm " PGBACKREST_CONFIG_ORIG_PATH_FILE);
        }
#endif
    }

    // *****************************************************************************************************************************
    if (testBegin("deprecated option names"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repository options");

        testOptionFind("hardlink", cfgOptRepoHardlink, 0, false, false, true);
        testOptionFind("no-hardlink", cfgOptRepoHardlink, 0, true, false, true);

        testOptionFind("archive-queue-max", cfgOptArchivePushQueueMax, 0, false, false, true);
        testOptionFind("reset-archive-queue-max", cfgOptArchivePushQueueMax, 0, false, true, true);

        testOptionFind("backup-cmd", cfgOptRepoHostCmd, 0, false, false, true);
        testOptionFind("backup-config", cfgOptRepoHostConfig, 0, false, false, true);
        testOptionFind("backup-host", cfgOptRepoHost, 0, false, false, true);
        testOptionFind("backup-ssh-port", cfgOptRepoHostPort, 0, false, false, true);
        testOptionFind("backup-user", cfgOptRepoHostUser, 0, false, false, true);

        testOptionFind("repo1-azure-ca-file", cfgOptRepoStorageCaFile, 0, false, false, true);
        testOptionFind("reset-repo1-azure-ca-file", cfgOptRepoStorageCaFile, 0, false, true, true);

        testOptionFind("repo1-azure-ca-path", cfgOptRepoStorageCaPath, 0, false, false, true);
        testOptionFind("reset-repo1-azure-ca-path", cfgOptRepoStorageCaPath, 0, false, true, true);

        testOptionFind("repo1-azure-host", cfgOptRepoStorageHost, 0, false, false, true);
        testOptionFind("reset-repo1-azure-host", cfgOptRepoStorageHost, 0, false, true, true);

        testOptionFind("repo1-azure-port", cfgOptRepoStoragePort, 0, false, false, true);
        testOptionFind("reset-repo1-azure-port", cfgOptRepoStoragePort, 0, false, true, true);

        testOptionFind("repo1-azure-verify-tls", cfgOptRepoStorageVerifyTls, 0, false, false, true);
        testOptionFind("no-repo1-azure-verify-tls", cfgOptRepoStorageVerifyTls, 0, true, false, true);
        testOptionFind("reset-repo1-azure-verify-tls", cfgOptRepoStorageVerifyTls, 0, false, true, true);

        testOptionFind("repo-cipher-pass", cfgOptRepoCipherPass, 0, false, false, true);
        testOptionFind("repo-cipher-type", cfgOptRepoCipherType, 0, false, false, true);
        testOptionFind("repo-path", cfgOptRepoPath, 0, false, false, true);
        testOptionFind("repo-type", cfgOptRepoType, 0, false, false, true);

        testOptionFind("repo-s3-bucket", cfgOptRepoS3Bucket, 0, false, false, true);

        testOptionFind("repo-s3-ca-file", cfgOptRepoStorageCaFile, 0, false, false, true);
        testOptionFind("repo1-s3-ca-file", cfgOptRepoStorageCaFile, 0, false, false, true);
        testOptionFind("reset-repo1-s3-ca-file", cfgOptRepoStorageCaFile, 0, false, true, true);

        testOptionFind("repo-s3-ca-path", cfgOptRepoStorageCaPath, 0, false, false, true);
        testOptionFind("repo1-s3-ca-path", cfgOptRepoStorageCaPath, 0, false, false, true);
        testOptionFind("reset-repo1-s3-ca-path", cfgOptRepoStorageCaPath, 0, false, true, true);

        testOptionFind("repo-s3-endpoint", cfgOptRepoS3Endpoint, 0, false, false, true);

        testOptionFind("repo-s3-host", cfgOptRepoStorageHost, 0, false, false, true);
        testOptionFind("repo1-s3-host", cfgOptRepoStorageHost, 0, false, false, true);
        testOptionFind("reset-repo1-s3-host", cfgOptRepoStorageHost, 0, false, true, true);

        testOptionFind("repo-s3-key", cfgOptRepoS3Key, 0, false, false, true);
        testOptionFind("repo-s3-key-secret", cfgOptRepoS3KeySecret, 0, false, false, true);

        testOptionFind("repo1-s3-port", cfgOptRepoStoragePort, 0, false, false, true);
        testOptionFind("reset-repo1-s3-port", cfgOptRepoStoragePort, 0, false, true, true);

        testOptionFind("repo-s3-region", cfgOptRepoS3Region, 0, false, false, true);

        testOptionFind("repo-s3-verify-ssl", cfgOptRepoStorageVerifyTls, 0, false, false, true);
        testOptionFind("repo1-s3-verify-ssl", cfgOptRepoStorageVerifyTls, 0, false, false, true);
        testOptionFind("no-repo-s3-verify-ssl", cfgOptRepoStorageVerifyTls, 0, true, false, true);
        testOptionFind("repo1-s3-verify-tls", cfgOptRepoStorageVerifyTls, 0, false, false, true);
        testOptionFind("no-repo1-s3-verify-tls", cfgOptRepoStorageVerifyTls, 0, true, false, true);
        testOptionFind("reset-repo1-s3-verify-tls", cfgOptRepoStorageVerifyTls, 0, false, true, true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("postreSQL options");

        testOptionFind("db-cmd", cfgOptPgHostCmd, 0, false, false, true);
        testOptionFind("db-config", cfgOptPgHostConfig, 0, false, false, true);
        testOptionFind("db-host", cfgOptPgHost, 0, false, false, true);
        testOptionFind("db-path", cfgOptPgPath, 0, false, false, true);
        testOptionFind("db-port", cfgOptPgPort, 0, false, false, true);
        testOptionFind("db-socket-path", cfgOptPgSocketPath, 0, false, false, true);
        testOptionFind("db-ssh-port", cfgOptPgHostPort, 0, false, false, true);
        testOptionFind("db-user", cfgOptPgHostUser, 0, false, false, true);

        TEST_ERROR(cfgParseOptionP(STR("no-db-user")), OptionInvalidError, "option 'no-db-user' cannot be negated");

        // Only check 1-8 since 8 was the max index when these option names were deprecated
        for (unsigned int optionIdx = 0; optionIdx < 8; optionIdx++)
        {
            testOptionFind(zNewFmt("db%u-cmd", optionIdx + 1), cfgOptPgHostCmd, optionIdx, false, false, true);
            testOptionFind(zNewFmt("db%u-config", optionIdx + 1), cfgOptPgHostConfig, optionIdx, false, false, true);
            testOptionFind(zNewFmt("db%u-host", optionIdx + 1), cfgOptPgHost, optionIdx, false, false, true);
            testOptionFind(zNewFmt("db%u-path", optionIdx + 1), cfgOptPgPath, optionIdx, false, false, true);
            testOptionFind(zNewFmt("db%u-port", optionIdx + 1), cfgOptPgPort, optionIdx, false, false, true);
            testOptionFind(zNewFmt("db%u-socket-path", optionIdx + 1), cfgOptPgSocketPath, optionIdx, false, false, true);
            testOptionFind(zNewFmt("db%u-ssh-port", optionIdx + 1), cfgOptPgHostPort, optionIdx, false, false, true);
            testOptionFind(zNewFmt("db%u-user", optionIdx + 1), cfgOptPgHostUser, optionIdx, false, false, true);
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
