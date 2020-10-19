/***********************************************************************************************************************************
Test Configuration Parse
***********************************************************************************************************************************/
#include "protocol/helper.h"
#include "storage/storage.intern.h"

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
testOptionFind(const char *option, unsigned int value)
{
    // If not testing for a missing option, then add the option offset that is already added to each option in the list
    if (value != 0)
        value |= PARSE_OPTION_FLAG;

    TEST_RESULT_INT(optionList[optionFind(strNew(option))].val, value, "check %s", option);
}

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // config and config-include-path options
    // *****************************************************************************************************************************
    if (testBegin("cfgFileLoad()"))
    {
        StringList *argList = NULL;
        String *configFile = strNewFmt("%s/test.config", testPath());

        String *configIncludePath = strNewFmt("%s/conf.d", testPath());
        mkdir(strZ(configIncludePath), 0750);

        // Check old config file constants
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_Z(PGBACKREST_CONFIG_ORIG_PATH_FILE, "/etc/pgbackrest.conf", "check old config path");
        TEST_RESULT_STR_Z(PGBACKREST_CONFIG_ORIG_PATH_FILE_STR, "/etc/pgbackrest.conf", "check old config path str");

        // Confirm same behavior with multiple config include files
        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNewFmt("--config=%s", strZ(configFile)));
        strLstAdd(argList, strNewFmt("--config-include-path=%s", strZ(configIncludePath)));
        strLstAdd(argList, strNew("--no-online"));
        strLstAdd(argList, strNew("--reset-pg1-host"));
        strLstAdd(argList, strNew("--reset-backup-standby"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));

        storagePut(
            storageNewWriteP(storageLocalWrite(), configFile),
            BUFSTRDEF(
                "[global]\n"
                "compress-level=3\n"
                "spool-path=/path/to/spool\n"));

        storagePut(
            storageNewWriteP(storageLocalWrite(), strNewFmt("%s/global-backup.conf", strZ(configIncludePath))),
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
            storageNewWriteP(storageLocalWrite(), strNewFmt("%s/db-backup.conf", strZ(configIncludePath))),
            BUFSTRDEF(
                "[db:backup]\n"
                "delta=n\n"
                "recovery-option=a=b\n"));

        storagePut(
            storageNewWriteP(storageLocalWrite(), strNewFmt("%s/stanza.db.conf", strZ(configIncludePath))),
            BUFSTRDEF(
                "[db]\n"
                "pg1-host=db\n"
                "pg1-path=/path/to/db\n"
                "recovery-option=c=d\n"));

        TEST_RESULT_VOID(
            configParse(strLstSize(argList), strLstPtr(argList), false), TEST_COMMAND_BACKUP " command with config-include");
        harnessLogResult(
            strZ(
                strNew(
                    "P00   WARN: configuration file contains option 'recovery-option' invalid for section 'db:backup'\n"
                    "P00   WARN: configuration file contains invalid option 'bogus'\n"
                    "P00   WARN: configuration file contains negate option 'no-delta'\n"
                    "P00   WARN: configuration file contains reset option 'reset-delta'\n"
                    "P00   WARN: configuration file contains command-line only option 'online'\n"
                    "P00   WARN: configuration file contains stanza-only option 'pg1-path' in global section 'global:backup'")));

        TEST_RESULT_BOOL(cfgOptionTest(cfgOptPgHost), false, "    pg1-host is not set (command line reset override)");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptPgPath), "/path/to/db", "    pg1-path is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptPgPath), cfgSourceConfig, "    pg1-path is source config");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptDelta), false, "    delta not is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptDelta), cfgSourceConfig, "    delta is source config");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptArchiveCheck), false, "    archive-check is not set");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptArchiveCopy), false, "    archive-copy is not set");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptRepoHardlink), true, "    repo-hardlink is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptRepoHardlink), cfgSourceConfig, "    repo-hardlink is source config");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 3, "    compress-level is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptCompressLevel), cfgSourceConfig, "    compress-level is source config");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptBackupStandby), false, "    backup-standby not is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBackupStandby), cfgSourceDefault, "    backup-standby is source default");
        TEST_RESULT_INT(cfgOptionInt64(cfgOptBufferSize), 65536, "    buffer-size is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBufferSize), cfgSourceConfig, "    backup-standby is source config");

        // Rename conf files - ensure read of conf extension only is attempted
        //--------------------------------------------------------------------------------------------------------------------------
        rename(strZ(strNewFmt("%s/db-backup.conf", strZ(configIncludePath))),
            strZ(strNewFmt("%s/db-backup.conf.save", strZ(configIncludePath))));
        rename(strZ(strNewFmt("%s/global-backup.conf", strZ(configIncludePath))),
            strZ(strNewFmt("%s/global-backup.confsave", strZ(configIncludePath))));

        // Set up defaults
        String *backupCmdDefConfigValue = strNew(cfgDefOptionDefault(
            cfgCommandDefIdFromId(cfgCommandId(TEST_COMMAND_BACKUP, true)), cfgOptionDefIdFromId(cfgOptConfig)));
        String *backupCmdDefConfigInclPathValue = strNew(cfgDefOptionDefault(
                cfgCommandDefIdFromId(cfgCommandId(TEST_COMMAND_BACKUP, true)), cfgOptionDefIdFromId(cfgOptConfigIncludePath)));
        String *oldConfigDefault = strNewFmt("%s%s", testPath(), PGBACKREST_CONFIG_ORIG_PATH_FILE);

        // Create the option structure and initialize with 0
        ParseOption parseOptionList[CFG_OPTION_TOTAL] = {{.found = false}};

        StringList *value = strLstNew();
        strLstAdd(value, configFile);

        parseOptionList[cfgOptConfig].found = true;
        parseOptionList[cfgOptConfig].source = cfgSourceParam;
        parseOptionList[cfgOptConfig].valueList = value;

        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfigIncludePath].found = true;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].valueList = value;

        TEST_RESULT_VOID(cfgFileLoadPart(NULL, NULL), "check null part");

        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n"
            "\n"
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config-include-path with .conf files and non-.conf files");

        // Pass invalid values
        //--------------------------------------------------------------------------------------------------------------------------
        // --config valid, --config-include-path invalid (does not exist)
        value = strLstNew();
        strLstAddZ(value, BOGUS_STR);

        parseOptionList[cfgOptConfigIncludePath].valueList = value;
        TEST_ERROR(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue,
                backupCmdDefConfigInclPathValue, oldConfigDefault), PathMissingError,
                "unable to list file info for missing path '/BOGUS'");

        // --config-include-path valid, --config invalid (does not exist)
        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfigIncludePath].valueList = value;

        value = strLstNew();
        strLstAdd(value, strNewFmt("%s/%s", testPath(), BOGUS_STR));

        parseOptionList[cfgOptConfig].valueList = value;

        TEST_ERROR_FMT(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(strNewFmt("%s/BOGUS", testPath())));

        strLstFree(parseOptionList[cfgOptConfig].valueList);
        strLstFree(parseOptionList[cfgOptConfigIncludePath].valueList);

        // Neither config nor config-include-path passed as parameter (defaults but none exist)
        //--------------------------------------------------------------------------------------------------------------------------
        parseOptionList[cfgOptConfig].found = false;
        parseOptionList[cfgOptConfig].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].found = false;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceDefault;

        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            NULL, "config default, config-include-path default but nothing to read");

        // Config not passed as parameter - config does not exist. config-include-path passed as parameter - only include read
        //--------------------------------------------------------------------------------------------------------------------------
        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].found = false;
        parseOptionList[cfgOptConfig].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].found = true;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config default and doesn't exist, config-include-path passed read");

        // config and config-include-path are "default" with files existing. Config file exists in both current default and old
        // default location - old location ignored.
        //--------------------------------------------------------------------------------------------------------------------------
        parseOptionList[cfgOptConfig].found = false;
        parseOptionList[cfgOptConfig].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].found = false;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceDefault;

        mkdir(strZ(strPath(oldConfigDefault)), 0750);
        storagePut(
            storageNewWriteP(storageLocalWrite(), oldConfigDefault),
            BUFSTRDEF(
                "[global:backup]\n"
                "buffer-size=65536\n"));

        // Pass actual location of config files as "default" - not setting valueList above, so these are the only possible values
        // to choose.
        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, configFile,  configIncludePath, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n"
            "\n"
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config and config-include-path default, files appended, original config not read");

        // Config not passed as parameter - config does not exist in new default but does exist in old default. config-include-path
        // passed as parameter - both include and old default read
        //--------------------------------------------------------------------------------------------------------------------------
        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].found = false;
        parseOptionList[cfgOptConfig].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].found = true;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global:backup]\n"
            "buffer-size=65536\n"
            "\n"
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config old default read, config-include-path passed read");

        // --no-config and config-include-path passed as parameter (files read only from include path and not current or old config)
        //--------------------------------------------------------------------------------------------------------------------------
        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].found = true;
        parseOptionList[cfgOptConfig].source = cfgSourceParam;
        parseOptionList[cfgOptConfig].negate = true;
        parseOptionList[cfgOptConfigIncludePath].found = true;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "--no-config, only config-include-path read");

        // --no-config and config-include-path default exists with files - nothing to read
        //--------------------------------------------------------------------------------------------------------------------------
        parseOptionList[cfgOptConfig].found = true;
        parseOptionList[cfgOptConfig].source = cfgSourceParam;
        parseOptionList[cfgOptConfig].negate = true;
        parseOptionList[cfgOptConfigIncludePath].found = false;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceDefault;

        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, configIncludePath, oldConfigDefault),
            NULL, "--no-config, config-include-path default, nothing read");

        // config passed and config-include-path default exists with files - only config read
        //--------------------------------------------------------------------------------------------------------------------------
        value = strLstNew();
        strLstAdd(value, configFile);

        parseOptionList[cfgOptConfig].found = true;
        parseOptionList[cfgOptConfig].source = cfgSourceParam;
        parseOptionList[cfgOptConfig].negate = false;
        parseOptionList[cfgOptConfig].valueList = value;
        parseOptionList[cfgOptConfigIncludePath].found = false;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceDefault;

        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, configIncludePath, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n",
            "config param specified, config-include-path default, only config read");

        // config new default and config-include-path passed - both exists with files. config-include-path & new config default read
        //--------------------------------------------------------------------------------------------------------------------------
        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].found = false;
        parseOptionList[cfgOptConfig].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].found = true;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, configFile, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n"
            "\n"
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config new default exists with files, config-include-path passed, default config and config-include-path read");

        // config and config-include-path are "default".
        //--------------------------------------------------------------------------------------------------------------------------
        parseOptionList[cfgOptConfig].found = false;
        parseOptionList[cfgOptConfig].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].found = false;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceDefault;

        // File exists in old default config location but not in current default.
        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, configIncludePath, oldConfigDefault),
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
        strLstAddZ(value, testPath());

        parseOptionList[cfgOptConfigPath].found = true;
        parseOptionList[cfgOptConfigPath].source = cfgSourceParam;
        parseOptionList[cfgOptConfigPath].valueList = value;

        // Override default paths for config and config-include-path - but no pgbackrest.conf file in override path only in old
        // default so ignored
        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[db]\n"
            "pg1-host=db\n"
            "pg1-path=/path/to/db\n"
            "recovery-option=c=d\n",
            "config-path override: config-include-path files read but config not read - does not exist");

        // Pass --config
        value = strLstNew();
        strLstAdd(value, configFile);

        parseOptionList[cfgOptConfig].found = true;
        parseOptionList[cfgOptConfig].source = cfgSourceParam;
        parseOptionList[cfgOptConfig].negate = false;
        parseOptionList[cfgOptConfig].valueList = value;

        // Passing --config and --config-path - default config-include-path overwritten and config is required and is loaded and
        // config-include-path files will attempt to be loaded but not required
        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
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

        parseOptionList[cfgOptConfigPath].valueList = value;

        // Passing --config and bogus --config-path - default config-include-path overwritten, config is required and is loaded and
        // config-include-path files will attempt to be loaded but doesn't exist - no error since not required
        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n",
            "config-path changed config-include-path default but directory does not exist - only config read");

        // Copy the configFile to pgbackrest.conf (default is /etc/pgbackrest/pgbackrest.conf and new value is testPath so copy the
        // config file (that was not read in the previous test) to pgbackrest.conf so it will be read by the override
        TEST_RESULT_INT(
            system(
                strZ(strNewFmt("cp %s %s", strZ(configFile), strZ(strNewFmt("%s/pgbackrest.conf", testPath()))))), 0,
                "copy configFile to pgbackrest.conf");

        parseOptionList[cfgOptConfig].found = false;
        parseOptionList[cfgOptConfig].source = cfgSourceDefault;
        parseOptionList[cfgOptConfig].negate = false;
        parseOptionList[cfgOptConfigIncludePath].found = false;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceDefault;

        value = strLstNew();
        strLstAddZ(value, testPath());

        parseOptionList[cfgOptConfigPath].valueList = value;

        // Override default paths for config and config-include-path with --config-path
        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, oldConfigDefault),
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
        parseOptionList[cfgOptConfigPath].found = false;
        parseOptionList[cfgOptConfigPath].source = cfgSourceDefault;

        // config default and config-include-path passed - but no config files in the include path - only in the default path
        // rm command is split below because code counter is confused by what looks like a comment.
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(system(strZ(strNewFmt("rm -rf %s/" "*", strZ(configIncludePath)))), 0, "remove all include files");

        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].found = false;
        parseOptionList[cfgOptConfig].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].found = true;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, configFile, backupCmdDefConfigInclPathValue, oldConfigDefault),
            "[global]\n"
            "compress-level=3\n"
            "spool-path=/path/to/spool\n",
            "config default exists with files but config-include-path path passed is empty - only config read");

        // config default and config-include-path passed - only empty file in the include path and nothing in either config defaults
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(
            system(strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/empty.conf", strZ(configIncludePath)))))), 0,
            "add empty conf file to include directory");

        value = strLstNew();
        strLstAdd(value, configIncludePath);

        parseOptionList[cfgOptConfig].found = false;
        parseOptionList[cfgOptConfig].source = cfgSourceDefault;
        parseOptionList[cfgOptConfigIncludePath].found = true;
        parseOptionList[cfgOptConfigIncludePath].source = cfgSourceParam;
        parseOptionList[cfgOptConfigIncludePath].valueList = value;

        TEST_RESULT_STR_Z(
            cfgFileLoad(parseOptionList, backupCmdDefConfigValue, backupCmdDefConfigInclPathValue, backupCmdDefConfigValue),
            NULL, "config default does not exist, config-include-path passed but only empty conf file - nothing read");
    }

    // *****************************************************************************************************************************
    if (testBegin("convertToByte()"))
    {
        double valueDbl = 0;
        String *value = strNew("10.0");

        TEST_ERROR(sizeQualifierToMultiplier('w'), AssertError, "'w' is not a valid size qualifier");
        TEST_ERROR(convertToByte(&value, &valueDbl), FormatError, "value '10.0' is not valid");
        strTrunc(value, strChr(value, '.'));
        strCatZ(value, "K2");
        TEST_ERROR(convertToByte(&value, &valueDbl), FormatError, "value '10K2' is not valid");
        strTrunc(value, strChr(value, '1'));
        strCatZ(value, "ab");
        TEST_ERROR(convertToByte(&value, &valueDbl), FormatError, "value 'ab' is not valid");

        strTrunc(value, strChr(value, 'a'));
        strCatZ(value, "10");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_DOUBLE(valueDbl, 10, "valueDbl no character identifier - straight to bytes");
        TEST_RESULT_STR_Z(value, "10", "value no character identifier - straight to bytes");

        strCatZ(value, "B");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_DOUBLE(valueDbl, 10, "valueDbl B to bytes");
        TEST_RESULT_STR_Z(value, "10", "value B to bytes");

        strCatZ(value, "Kb");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_DOUBLE(valueDbl, 10240, "valueDbl KB to bytes");
        TEST_RESULT_STR_Z(value, "10240", "value KB to bytes");

        strTrunc(value, strChr(value, '2'));
        strCatZ(value, "k");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_DOUBLE(valueDbl, 10240, "valueDbl k to bytes");
        TEST_RESULT_STR_Z(value, "10240", "value k to bytes");

        strCatZ(value, "pB");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_DOUBLE(valueDbl, 11529215046068469760U, "valueDbl Pb to bytes");
        TEST_RESULT_STR_Z(value, "11529215046068469760", "value Pb to bytes");

        strTrunc(value, strChr(value, '5'));
        strCatZ(value, "GB");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_DOUBLE(valueDbl, 11811160064U, "valueDbl GB to bytes");
        TEST_RESULT_STR_Z(value, "11811160064", "value GB to bytes");

        strTrunc(value, strChr(value, '8'));
        strCatZ(value, "g");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_DOUBLE(valueDbl, 11811160064U, "valueDbl g to bytes");
        TEST_RESULT_STR_Z(value, "11811160064", "value g to bytes");

        strTrunc(value, strChr(value, '8'));
        strCatZ(value, "T");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_DOUBLE(valueDbl, 12094627905536U, "valueDbl T to bytes");
        TEST_RESULT_STR_Z(value, "12094627905536", "value T to bytes");

        strTrunc(value, strChr(value, '0'));
        strCatZ(value, "tb");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_DOUBLE(valueDbl, 13194139533312U, "valueDbl tb to bytes");
        TEST_RESULT_STR_Z(value, "13194139533312", "value tb to bytes");

        strTrunc(value, strChr(value, '3'));
        strCatZ(value, "0m");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_DOUBLE(valueDbl, 10485760, "valueDbl m to bytes");
        TEST_RESULT_STR_Z(value, "10485760", "value m to bytes");

        strCatZ(value, "mb");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_DOUBLE(valueDbl, 10995116277760U, "valueDbl mb to bytes");
        TEST_RESULT_STR_Z(value, "10995116277760", "value mb to bytes");

        strTrunc(value, strChr(value, '0'));
        strCatZ(value, "99999999999999999999p");
        convertToByte(&value, &valueDbl);
        TEST_RESULT_STR_Z(value, "225179981368524800000000000000000000", "value really large  to bytes");
    }

    // *****************************************************************************************************************************
    if (testBegin("configParse()"))
    {
        StringList *argList = NULL;
        String *configFile = strNewFmt("%s/test.config", testPath());

        TEST_RESULT_INT(
            sizeof(optionResolveOrder) / sizeof(ConfigOption), CFG_OPTION_TOTAL,
            "check that the option resolve list contains an entry for every option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(BOGUS_STR));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList), false), CommandInvalidError, "invalid command 'BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(BOGUS_STR ":" BOGUS_STR));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), CommandInvalidError, "invalid command 'BOGUS:BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--" BOGUS_STR));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError, "invalid option '--BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-host"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option '--pg1-host' requires argument");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("backup"));
        strLstAdd(argList, strNew("param1"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), ParamInvalidError, "command does not allow parameters");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("help"));
        strLstAdd(argList, strNew("backup"));
        strLstAdd(argList, strNew("param1"));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), "ignore params when help command");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--no-online"));
        strLstAdd(argList, strNew("--no-online"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'online' is negated multiple times");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--reset-pg1-host"));
        strLstAdd(argList, strNew("--reset-pg1-host"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'pg1-host' is reset multiple times");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--no-config"));
        strLstAdd(argList, strNew("--config=/etc/config"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'config' cannot be set and negated");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--reset-log-path"));
        strLstAdd(argList, strNew("--log-path=/var/log"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'log-path' cannot be set and reset");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--no-delta"));
        strLstAdd(argList, strNew("--reset-delta"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'delta' cannot be negated and reset");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--reset-delta"));
        strLstAdd(argList, strNew("--no-delta"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'delta' cannot be negated and reset");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--delta"));
        strLstAdd(argList, strNew("--delta"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'delta' cannot be set multiple times");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--compress-level=3"));
        strLstAdd(argList, strNew("--compress-level=3"));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'compress-level' cannot be set multiple times");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--online"));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList), false), CommandRequiredError, "no command found");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--manifest-save-threshold=123Q"));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'123Q' is not valid for 'manifest-save-threshold' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--manifest-save-threshold=199999999999999999999p"));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'225179981368524800000000000000000000' is out of range for 'manifest-save-threshold' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--pg1-path="));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'' must be >= 1 character for 'pg1-path' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--pg1-path=bogus"));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'bogus' must begin with / for 'pg1-path' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--pg1-path=/path1//path2"));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'/path1//path2' cannot contain // for 'pg1-path' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--pg1-path=/path1/path2//"));
        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'/path1/path2//' cannot contain // for 'pg1-path' option");

        // Local and remove commands should not modify log levels during parsing
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        strLstAdd(argList, strNew("--host-id=1"));
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to");
        strLstAdd(argList, strNew("--process=1"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_REPO);
        strLstAdd(argList, strNew("--log-level-stderr=info"));
        strLstAddZ(argList, CFGCMD_BACKUP ":" CONFIG_COMMAND_ROLE_LOCAL);

        logLevelStdOut = logLevelError;
        logLevelStdErr = logLevelError;
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), "load local config");
        TEST_RESULT_INT(logLevelStdOut, logLevelError, "console logging is error");
        TEST_RESULT_INT(logLevelStdErr, logLevelError, "stderr logging is error");

        argList = strLstNew();
        strLstAdd(argList, strNew("pgbackrest"));
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to");
        strLstAdd(argList, strNew("--process=1"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_REPO);
        strLstAdd(argList, strNew("--log-level-stderr=info"));
        strLstAddZ(argList, CFGCMD_BACKUP ":" CONFIG_COMMAND_ROLE_REMOTE);

        logLevelStdOut = logLevelError;
        logLevelStdErr = logLevelError;
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), "load remote config");
        TEST_RESULT_INT(logLevelStdOut, logLevelError, "console logging is error");
        TEST_RESULT_INT(logLevelStdErr, logLevelError, "stderr logging is error");
        harnessLogLevelReset();

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionRequiredError,
            "backup command requires option: pg1-path\nHINT: does this stanza exist?");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionRequiredError,
            "backup command requires option: stanza");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--repo1-type=s3"));
        strLstAdd(argList, strNew("--repo1-s3-key=xxx"));
        strLstAdd(argList, strNew("--repo1-s3-bucket=xxx"));
        strLstAdd(argList, strNew("--repo1-s3-endpoint=xxx"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'repo1-s3-key' is not allowed on the command-line\n"
            "HINT: this option could expose secrets in the process list.\n"
            "HINT: specify the option in a configuration file or an environment variable instead.");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--no-config"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--repo1-s3-host=xxx"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'repo1-s3-host' not valid without option 'repo1-type' = 's3'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--repo1-host-user=xxx"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'repo1-host-user' not valid without option 'repo1-host'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--force"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'force' not valid without option 'no-online'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--spool-path=/path/to/spool"));
        strLstAdd(argList, strNew(TEST_COMMAND_ARCHIVE_GET));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'spool-path' not valid without option 'archive-async'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--recovery-option=a=b"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'recovery-option' not valid for command 'backup'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--target-exclusive"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "option 'target-exclusive' not valid without option 'type' in ('time', 'xid')");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--type=bogus"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'bogus' is not allowed for 'type' option");

        // Lower and upper bounds for integer ranges
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--process-max=0"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'0' is out of range for 'process-max' option");

        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--process-max=65536"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'65536' is out of range for 'process-max' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--process-max=bogus"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'bogus' is not valid for 'process-max' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--protocol-timeout=.01"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'.01' is out of range for 'protocol-timeout' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--protocol-timeout=bogus"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "'bogus' is not valid for 'protocol-timeout' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        setenv("PGBACKREST_PROTOCOL_TIMEOUT", "", true);
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "environment variable 'protocol-timeout' must have a value");

        unsetenv("PGBACKREST_PROTOCOL_TIMEOUT");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--stanza=db"));
        setenv("PGBACKREST_DELTA", "x", true);
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "environment boolean option 'delta' must be 'y' or 'n'");

        unsetenv("PGBACKREST_DELTA");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNewFmt("--config=%s", strZ(configFile)));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));

        storagePutP(
            storageNewWriteP(storageLocalWrite(), configFile),
            BUFSTRDEF(
                "[global]\n"
                "delta=bogus\n"));

        TEST_ERROR(configParse(
            strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "boolean option 'delta' must be 'y' or 'n'");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNewFmt("--config=%s", strZ(configFile)));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));

        storagePutP(
            storageNewWriteP(storageLocalWrite(), configFile),
            BUFSTRDEF(
                "[global]\n"
                "delta=\n"));

        TEST_ERROR(configParse(
            strLstSize(argList), strLstPtr(argList), false), OptionInvalidValueError,
            "section 'global', key 'delta' must have a value");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNewFmt("--config=%s", strZ(configFile)));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));

        storagePutP(
            storageNewWriteP(storageLocalWrite(), configFile),
            BUFSTRDEF(
                "[db]\n"
                "pg1-path=/path/to/db\n"
                "db-path=/also/path/to/db\n"));

        TEST_ERROR(configParse(
            strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            strZ(strNew("configuration file contains duplicate options ('db-path', 'pg1-path') in section '[db]'")));

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNewFmt("--config=%s", strZ(configFile)));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));

        storagePutP(
            storageNewWriteP(storageLocalWrite(), configFile),
            BUFSTRDEF(
                "[db]\n"
                "pg1-path=/path/to/db\n"
                "pg1-path=/also/path/to/db\n"));

        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList), false),
            OptionInvalidError,
            "option 'pg1-path' cannot be set multiple times");

        // Also test with a boolean option since this gets converted immediately and will blow up if it is multi
        storagePutP(
            storageNewWriteP(storageLocalWrite(), configFile),
            BUFSTRDEF(
                "[db]\n"
                "start-fast=y\n"
                "start-fast=n\n"));

        TEST_ERROR(configParse(strLstSize(argList), strLstPtr(argList), false),
            OptionInvalidError,
            "option 'start-fast' cannot be set multiple times");

        // Test that log levels are set correctly when reset is enabled, then set them back to harness defaults
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));

        logLevelStdOut = logLevelOff;
        logLevelStdErr = logLevelOff;
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), true), "no command");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "    help is set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdNone, "    command is none");
        TEST_RESULT_INT(logLevelStdOut, logLevelWarn, "console logging is warn");
        TEST_RESULT_INT(logLevelStdErr, logLevelWarn, "stderr logging is warn");
        harnessLogLevelReset();

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("help"));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), "help command");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "    help is set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdHelp, "    command is help");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("help"));
        strLstAdd(argList, strNew("version"));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), "help for version command");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "    help is set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdVersion, "    command is version");

        // Help should not fail on missing options
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("help"));
        strLstAdd(argList, strNew("backup"));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), "help for backup command");
        TEST_RESULT_BOOL(cfgCommandHelp(), true, "    help is set");
        TEST_RESULT_INT(cfgCommand(), cfgCmdBackup, "    command is backup");
        TEST_RESULT_BOOL(cfgOptionValid(cfgOptPgPath), true, "    pg1-path is valid");
        TEST_RESULT_PTR(cfgOption(cfgOptPgPath), NULL, "    pg1-path is not set");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAdd(argList, strNew(TEST_COMMAND_ARCHIVE_GET));
        strLstAdd(argList, strNew("000000010000000200000003"));
        strLstAdd(argList, strNew("/path/to/wal/RECOVERYWAL"));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), "command arguments");
        TEST_RESULT_STR_Z(
            strLstJoin(cfgCommandParam(), "|"), "000000010000000200000003|/path/to/wal/RECOVERYWAL",
            "    check command arguments");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db/"));
        strLstAdd(argList, strNew("--no-online"));
        strLstAdd(argList, strNew("--no-config"));
        strLstAdd(argList, strNew("--repo1-type=s3"));
        strLstAdd(argList, strNew("--repo1-s3-bucket=test"));
        strLstAdd(argList, strNew("--repo1-s3-endpoint=test"));
        strLstAdd(argList, strNew("--repo1-s3-region=test"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));
        setenv("PGBACKREST_REPO1_S3_KEY", "xxx", true);
        setenv("PGBACKREST_REPO1_S3_KEY_SECRET", "xxx", true);
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), TEST_COMMAND_BACKUP " command");
        TEST_RESULT_INT(cfgCommand(), cfgCmdBackup, "    command is " TEST_COMMAND_BACKUP);

        TEST_RESULT_STR_Z(cfgExe(), TEST_BACKREST_EXE, "    exe is set");

        TEST_RESULT_BOOL(cfgOptionTest(cfgOptConfig), false, "    config is not set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptConfig), cfgSourceParam, "    config is source param");
        TEST_RESULT_BOOL(cfgOptionNegate(cfgOptConfig), true, "    config is negated");
        TEST_RESULT_INT(cfgOptionSource(cfgOptStanza), cfgSourceParam, "    stanza is source param");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptStanza), "db", "    stanza is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptStanza), cfgSourceParam, "    stanza is source param");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptPgPath), "/path/to/db", "    pg1-path is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptPgPath), cfgSourceParam, "    pg1-path is source param");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoS3KeySecret), "xxx", "    repo1-s3-secret is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptRepoS3KeySecret), cfgSourceConfig, "    repo1-s3-secret is source env");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptOnline), false, "    online is not set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptOnline), cfgSourceParam, "    online is source default");
        TEST_RESULT_INT(cfgOptionInt(cfgOptBufferSize), 1048576, "    buffer-size is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBufferSize), cfgSourceDefault, "    buffer-size is source default");

        unsetenv("PGBACKREST_REPO1_S3_KEY");
        unsetenv("PGBACKREST_REPO1_S3_KEY_SECRET");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNewFmt("--config=%s", strZ(configFile)));
        strLstAdd(argList, strNew("--no-online"));
        hrnCfgArgKeyRawBool(argList, cfgOptPgLocal, 2, true);
        strLstAdd(argList, strNew("--reset-pg1-host"));
        strLstAdd(argList, strNew("--reset-backup-standby"));
        strLstAdd(argList, strNew(TEST_COMMAND_BACKUP));

        setenv("PGBACKRESTXXX_NOTHING", "xxx", true);
        setenv("PGBACKREST_BOGUS", "xxx", true);
        setenv("PGBACKREST_NO_DELTA", "xxx", true);
        setenv("PGBACKREST_RESET_REPO1_HOST", "", true);
        setenv("PGBACKREST_TARGET", "xxx", true);
        setenv("PGBACKREST_ONLINE", "y", true);
        setenv("PGBACKREST_DELTA", "y", true);
        setenv("PGBACKREST_START_FAST", "n", true);
        setenv("PGBACKREST_PG1_SOCKET_PATH", "/path/to/socket", true);

        storagePutP(
            storageNewWriteP(storageLocalWrite(), configFile),
            BUFSTR(
                strNewFmt(
                    "[global]\n"
                    "compress-level=3\n"
                    "spool-path=/path/to/spool\n"
                    "lock-path=/\n"
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
                    "buffer-size=65536\n"
                    "\n"
                    "[db:backup]\n"
                    "delta=n\n"
                    "recovery-option=a=b\n"
                    "\n"
                    "[db]\n"
                    "pg1-host=db\n"
                    "pg1-path=/path/to/db\n"
                    "%s=ignore\n"
                    "%s=/path/to/db2\n"
                    "recovery-option=c=d\n",
                    cfgOptionName(cfgOptPgHost + 1), cfgOptionName(cfgOptPgPath + 1))));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), TEST_COMMAND_BACKUP " command");
        harnessLogResult(
            strZ(
                strNew(
                    "P00   WARN: environment contains invalid option 'bogus'\n"
                    "P00   WARN: environment contains invalid negate option 'no-delta'\n"
                    "P00   WARN: environment contains invalid reset option 'reset-repo1-host'\n"
                    "P00   WARN: configuration file contains option 'recovery-option' invalid for section 'db:backup'\n"
                    "P00   WARN: configuration file contains invalid option 'bogus'\n"
                    "P00   WARN: configuration file contains negate option 'no-delta'\n"
                    "P00   WARN: configuration file contains reset option 'reset-delta'\n"
                    "P00   WARN: configuration file contains command-line only option 'online'\n"
                    "P00   WARN: configuration file contains stanza-only option 'pg1-path' in global section 'global:backup'")));

        TEST_RESULT_BOOL(cfgOptionTest(cfgOptPgHost), false, "    pg1-host is not set (command line reset override)");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptPgPath), "/path/to/db", "    pg1-path is set");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptPgLocal + 1), true, "    pg2-local is set");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptPgHost + 1), false, "    pg2-host is not set (pg2-local override)");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptPgPath + 1), "/path/to/db2", "    pg2-path is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptPgPath), cfgSourceConfig, "    pg1-path is source config");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptLockPath), "/", "    lock-path is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptLockPath), cfgSourceConfig, "    lock-path is source config");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptPgSocketPath), "/path/to/socket", "    pg1-socket-path is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptPgSocketPath), cfgSourceConfig, "    pg1-socket-path is config param");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptOnline), false, "    online not is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptOnline), cfgSourceParam, "    online is source param");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptStartFast), false, "    start-fast not is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptStartFast), cfgSourceConfig, "    start-fast is config param");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptDelta), true, "    delta not set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptDelta), cfgSourceConfig, "    delta is source config");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptArchiveCheck), false, "    archive-check is not set");
        TEST_RESULT_BOOL(cfgOptionTest(cfgOptArchiveCopy), false, "    archive-copy is not set");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptRepoHardlink), true, "    repo-hardlink is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptRepoHardlink), cfgSourceConfig, "    repo-hardlink is source config");
        TEST_RESULT_INT(cfgOptionInt(cfgOptCompressLevel), 3, "    compress-level is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptCompressLevel), cfgSourceConfig, "    compress-level is source config");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptBackupStandby), false, "    backup-standby not is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBackupStandby), cfgSourceDefault, "    backup-standby is source default");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptDelta), true, "    delta is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptDelta), cfgSourceConfig, "    delta is source config");
        TEST_RESULT_INT(cfgOptionInt64(cfgOptBufferSize), 65536, "    buffer-size is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBufferSize), cfgSourceConfig, "    backup-standby is source config");

        unsetenv("PGBACKREST_BOGUS");
        unsetenv("PGBACKREST_NO_DELTA");
        unsetenv("PGBACKREST_RESET_REPO1_HOST");
        unsetenv("PGBACKREST_TARGET");
        unsetenv("PGBACKREST_ONLINE");
        unsetenv("PGBACKREST_START_FAST");
        unsetenv("PGBACKREST_PG1_SOCKET_PATH");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNewFmt("--config=%s", strZ(configFile)));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew("--archive-push-queue-max=4503599627370496"));
        strLstAdd(argList, strNew("--buffer-size=2MB"));
        strLstAdd(argList, strNew("archive-push"));

        storagePutP(
            storageNewWriteP(storageLocalWrite(), configFile),
            BUFSTRDEF(
                "[global]\n"
                "spool-path=/path/to/spool\n"));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), "archive-push command");

        TEST_RESULT_INT(cfgOptionInt64(cfgOptArchivePushQueueMax), 4503599627370496, "archive-push-queue-max is set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptArchivePushQueueMax), cfgSourceParam, "    archive-push-queue-max is source config");
        TEST_RESULT_INT(cfgOptionInt64(cfgOptBufferSize), 2097152, "buffer-size is set to bytes from MB");
        TEST_RESULT_INT(cfgOptionSource(cfgOptBufferSize), cfgSourceParam, "    buffer-size is source config");
        TEST_RESULT_PTR(cfgOption(cfgOptSpoolPath), NULL, "    spool-path is not set");
        TEST_RESULT_INT(cfgOptionSource(cfgOptSpoolPath), cfgSourceDefault, "    spool-path is source default");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--recovery-option=a"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "key/value 'a' not valid for 'recovery-option' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--recovery-option=a"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_ERROR(
            configParse(strLstSize(argList), strLstPtr(argList), false), OptionInvalidError,
            "key/value 'a' not valid for 'recovery-option' option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--db-include=abc"));
        strLstAdd(argList, strNew("--db-include=def"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), TEST_COMMAND_RESTORE " command");

        const VariantList *includeList = NULL;
        TEST_ASSIGN(includeList, cfgOptionLst(cfgOptDbInclude), "get db include options");
        TEST_RESULT_STR_Z(varStr(varLstGet(includeList, 0)), "abc", "check db include option");
        TEST_RESULT_STR_Z(varStr(varLstGet(includeList, 1)), "def", "check db include option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew("--pg1-path=/path/to/db"));
        strLstAdd(argList, strNew("--recovery-option=a=b"));
        strLstAdd(argList, strNew("--recovery-option=c=de=fg hi"));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));
        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), TEST_COMMAND_RESTORE " command");

        const KeyValue *recoveryKv = NULL;
        TEST_ASSIGN(recoveryKv, cfgOptionKv(cfgOptRecoveryOption), "get recovery options");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, varNewStr(strNew("a")))), "b", "check recovery option");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, varNewStr(strNew("c")))), "de=fg hi", "check recovery option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNewFmt("--config=%s", strZ(configFile)));
        strLstAdd(argList, strNew("--stanza=db"));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));

        storagePutP(
            storageNewWriteP(storageLocalWrite(), configFile),
            BUFSTRDEF(
                "[global:restore]\n"
                "recovery-option=f=g\n"
                "recovery-option=hijk=l\n"
                "\n"
                "[db]\n"
                "pg1-path=/path/to/db\n"));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), TEST_COMMAND_RESTORE " command");

        TEST_ASSIGN(recoveryKv, cfgOptionKv(cfgOptRecoveryOption), "get recovery options");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, varNewStr(strNew("f")))), "g", "check recovery option");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, varNewStr(strNew("hijk")))), "l", "check recovery option");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNew(TEST_COMMAND_RESTORE));

        setenv("PGBACKREST_STANZA", "db", true);
        setenv("PGBACKREST_PG1_PATH", "/path/to/db", true);
        setenv("PGBACKREST_RECOVERY_OPTION", "f=g:hijk=l", true);
        setenv("PGBACKREST_DB_INCLUDE", "77", true);

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), TEST_COMMAND_RESTORE " command");

        TEST_ASSIGN(recoveryKv, cfgOptionKv(cfgOptRecoveryOption), "get recovery options");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, varNewStr(strNew("f")))), "g", "check recovery option");
        TEST_RESULT_STR_Z(varStr(kvGet(recoveryKv, varNewStr(strNew("hijk")))), "l", "check recovery option");
        TEST_RESULT_STR_Z(varStr(varLstGet(cfgOptionLst(cfgOptDbInclude), 0)), "77", "check db include option");
        TEST_RESULT_UINT(varLstSize(cfgOptionLst(cfgOptDbInclude)), 1, "check db include option size");

        unsetenv("PGBACKREST_STANZA");
        unsetenv("PGBACKREST_PG1_PATH");
        unsetenv("PGBACKREST_RECOVERY_OPTION");
        unsetenv("PGBACKREST_DB_INCLUDE");

        // Stanza options should not be loaded for commands that don't take a stanza
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNew(TEST_BACKREST_EXE));
        strLstAdd(argList, strNewFmt("--config=%s", strZ(configFile)));
        strLstAdd(argList, strNew("info"));

        storagePutP(
            storageNewWriteP(storageLocalWrite(), configFile),
            BUFSTRDEF(
                "[global]\n"
                "repo1-path=/path/to/repo\n"
                "\n"
                "[db]\n"
                "repo1-path=/not/the/path\n"));

        TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), "info command");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoPath), "/path/to/repo", "check repo1-path option");
    }

    // *****************************************************************************************************************************
    if (testBegin("deprecated option names"))
    {
        // Repository options
        // -------------------------------------------------------------------------------------------------------------------------
        testOptionFind("hardlink", PARSE_DEPRECATE_FLAG | cfgOptRepoHardlink);
        testOptionFind("no-hardlink", PARSE_DEPRECATE_FLAG | PARSE_NEGATE_FLAG | cfgOptRepoHardlink);

        testOptionFind("archive-queue-max", PARSE_DEPRECATE_FLAG | cfgOptArchivePushQueueMax);
        testOptionFind("reset-archive-queue-max", PARSE_DEPRECATE_FLAG | PARSE_RESET_FLAG | cfgOptArchivePushQueueMax);

        testOptionFind("backup-cmd", PARSE_DEPRECATE_FLAG | cfgOptRepoHostCmd);
        testOptionFind("backup-config", PARSE_DEPRECATE_FLAG | cfgOptRepoHostConfig);
        testOptionFind("backup-host", PARSE_DEPRECATE_FLAG | cfgOptRepoHost);
        testOptionFind("backup-ssh-port", PARSE_DEPRECATE_FLAG | cfgOptRepoHostPort);
        testOptionFind("backup-user", PARSE_DEPRECATE_FLAG | cfgOptRepoHostUser);

        testOptionFind("repo-cipher-pass", PARSE_DEPRECATE_FLAG | cfgOptRepoCipherPass);
        testOptionFind("repo-cipher-type", PARSE_DEPRECATE_FLAG | cfgOptRepoCipherType);
        testOptionFind("repo-path", PARSE_DEPRECATE_FLAG | cfgOptRepoPath);
        testOptionFind("repo-type", PARSE_DEPRECATE_FLAG | cfgOptRepoType);

        testOptionFind("repo-s3-bucket", PARSE_DEPRECATE_FLAG | cfgOptRepoS3Bucket);
        testOptionFind("repo-s3-ca-file", PARSE_DEPRECATE_FLAG | cfgOptRepoS3CaFile);
        testOptionFind("repo-s3-ca-path", PARSE_DEPRECATE_FLAG | cfgOptRepoS3CaPath);
        testOptionFind("repo-s3-endpoint", PARSE_DEPRECATE_FLAG | cfgOptRepoS3Endpoint);
        testOptionFind("repo-s3-host", PARSE_DEPRECATE_FLAG | cfgOptRepoS3Host);
        testOptionFind("repo-s3-key", PARSE_DEPRECATE_FLAG | cfgOptRepoS3Key);
        testOptionFind("repo-s3-key-secret", PARSE_DEPRECATE_FLAG | cfgOptRepoS3KeySecret);
        testOptionFind("repo-s3-region", PARSE_DEPRECATE_FLAG | cfgOptRepoS3Region);
        testOptionFind("repo-s3-verify-ssl", PARSE_DEPRECATE_FLAG | cfgOptRepoS3VerifyTls);
        testOptionFind("repo1-s3-verify-ssl", PARSE_DEPRECATE_FLAG | cfgOptRepoS3VerifyTls);
        testOptionFind("no-repo-s3-verify-ssl", PARSE_DEPRECATE_FLAG | PARSE_NEGATE_FLAG | cfgOptRepoS3VerifyTls);

        // PostreSQL options
        // -------------------------------------------------------------------------------------------------------------------------
        testOptionFind("db-cmd", PARSE_DEPRECATE_FLAG | cfgOptPgHostCmd);
        testOptionFind("db-config", PARSE_DEPRECATE_FLAG | cfgOptPgHostConfig);
        testOptionFind("db-host", PARSE_DEPRECATE_FLAG | cfgOptPgHost);
        testOptionFind("db-path", PARSE_DEPRECATE_FLAG | cfgOptPgPath);
        testOptionFind("db-port", PARSE_DEPRECATE_FLAG | cfgOptPgPort);
        testOptionFind("db-socket-path", PARSE_DEPRECATE_FLAG | cfgOptPgSocketPath);
        testOptionFind("db-ssh-port", PARSE_DEPRECATE_FLAG | cfgOptPgHostPort);
        testOptionFind("db-user", PARSE_DEPRECATE_FLAG | cfgOptPgHostUser);

        testOptionFind("no-db-user", 0);

        for (unsigned int optionIdx = 0; optionIdx < cfgDefOptionIndexTotal(cfgDefOptPgPath); optionIdx++)
        {
            testOptionFind(strZ(strNewFmt("db%u-cmd", optionIdx + 1)), PARSE_DEPRECATE_FLAG | (cfgOptPgHostCmd + optionIdx));
            testOptionFind(
                strZ(strNewFmt("db%u-config", optionIdx + 1)), PARSE_DEPRECATE_FLAG | (cfgOptPgHostConfig + optionIdx));
            testOptionFind(strZ(strNewFmt("db%u-host", optionIdx + 1)), PARSE_DEPRECATE_FLAG | (cfgOptPgHost + optionIdx));
            testOptionFind(strZ(strNewFmt("db%u-path", optionIdx + 1)), PARSE_DEPRECATE_FLAG | (cfgOptPgPath + optionIdx));
            testOptionFind(strZ(strNewFmt("db%u-port", optionIdx + 1)), PARSE_DEPRECATE_FLAG | (cfgOptPgPort + optionIdx));
            testOptionFind(
                strZ(strNewFmt("db%u-socket-path", optionIdx + 1)), PARSE_DEPRECATE_FLAG | (cfgOptPgSocketPath + optionIdx));
            testOptionFind(
                strZ(strNewFmt("db%u-ssh-port", optionIdx + 1)), PARSE_DEPRECATE_FLAG | (cfgOptPgHostPort + optionIdx));
            testOptionFind(strZ(strNewFmt("db%u-user", optionIdx + 1)), PARSE_DEPRECATE_FLAG | (cfgOptPgHostUser + optionIdx));
        }
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
