/***********************************************************************************************************************************
Test Check Command
***********************************************************************************************************************************/
#include "command/stanza/create.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "postgres/version.h"
#include "storage/helper.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPostgres.h"
#include "common/harnessPq.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    const Storage *const storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // PQfinish() is strictly checked
    hrnPqScriptStrictSet(true);

    StringList *argList = strLstNew();

    // *****************************************************************************************************************************
    if (testBegin("checkReport()"))
    {
        // Common config
        HRN_STORAGE_PUT_Z(
            storageTest, "pgbackrest.conf",
            "[global]\n"
            "repo1-path=" TEST_PATH "/repo1\n"
            "repo1-cipher-type=aes-256-cbc\n"
            "repo1-cipher-pass=ULmO7pKuimOzPEqHH9HUqQln\n"
            "repo1-block=y\n"
            "no-repo1-block=bogus\n"
            "bogus=y\n"
            "\n"
            "[global:backup]\n"
            "start-fast=y\n"
            "reset-start-fast=bogus\n"
            "\n"
            "[test2]\n"
            "pg1-path=" TEST_PATH "/test2-pg1\n"
            "recovery-option=key1=value1\n"
            "recovery-option=key2=value2\n"
            "\n"
            "[test1]\n"
            "pg1-path=" TEST_PATH "/test1-pg1\n");

        StringList *const argListCommon = strLstNew();
        hrnCfgArgRawZ(argListCommon, cfgOptConfig, TEST_PATH "/pgbackrest.conf");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no env, no config");
        {
            TEST_RESULT_STR_Z(
                checkReport(),
                // {uncrustify_off - indentation}
                "{"
                    "\"cfg\":{"
                        "\"env\":{},"
                        "\"file\":null"
                    "}"
                "}",
                // {uncrustify_on}
                "render");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("report on stdout");
        {
            argList = strLstNew();
            hrnCfgArgRawBool(argList, cfgOptReport, true);
            HRN_CFG_LOAD(cfgCmdCheck, argList);

            // Redirect stdout to a file
            int stdoutSave = dup(STDOUT_FILENO);
            const String *stdoutFile = STRDEF(TEST_PATH "/stdout.info");

            THROW_ON_SYS_ERROR(freopen(strZ(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

            // Not in a test wrapper to avoid writing to stdout
            cmdCheck();

            // Restore normal stdout
            dup2(stdoutSave, STDOUT_FILENO);

            // Check output of info command stored in file
            TEST_STORAGE_GET(
                storageTest, strZ(stdoutFile),
                // {uncrustify_off - indentation}
                "{"
                    "\"cfg\":{"
                        "\"env\":{},"
                        "\"file\":null"
                    "}"
                "}",
                // {uncrustify_on}
                .remove = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("env and config");
        {
            hrnCfgEnvRawZ(cfgOptBufferSize, "64KiB");
            setenv(PGBACKREST_ENV "BOGUS", "bogus", true);
            setenv(PGBACKREST_ENV "NO_ONLINE", "bogus", true);
            setenv(PGBACKREST_ENV "DB_INCLUDE", "db1:db2", true);
            setenv(PGBACKREST_ENV "RESET_COMPRESS_TYPE", "bogus", true);

            hrnCfgLoad(cfgCmdCheck, argListCommon, (HrnCfgLoadParam){.log = true});

            TEST_RESULT_STR_Z(
                checkReport(),
                // {uncrustify_off - indentation}
                "{"
                    "\"cfg\":{"
                        "\"env\":{"
                            "\"PGBACKREST_BOGUS\":{"
                                "\"warn\":\"invalid option\""
                            "},"
                            "\"PGBACKREST_BUFFER_SIZE\":{"
                                "\"val\":\"64KiB\""
                            "},"
                            "\"PGBACKREST_DB_INCLUDE\":{"
                                "\"val\":["
                                    "\"db1\","
                                    "\"db2\""
                                "]"
                            "},"
                            "\"PGBACKREST_NO_ONLINE\":{"
                                "\"val\":\"bogus\","
                                "\"warn\":\"invalid negate option\""
                            "},"
                            "\"PGBACKREST_RESET_COMPRESS_TYPE\":{"
                                "\"val\":\"bogus\","
                                "\"warn\":\"invalid reset option\""
                            "}"
                        "},"
                        "\"file\":{"
                            "\"global\":{"
                                "\"bogus\":{"
                                    "\"warn\":\"invalid option\""
                                "},"
                                "\"no-repo1-block\":{"
                                    "\"val\":\"bogus\","
                                    "\"warn\":\"invalid negate option\""
                                "},"
                                "\"repo1-block\":{"
                                    "\"val\":\"y\""
                                "},"
                                "\"repo1-cipher-pass\":{"
                                    "\"val\":\"<repo1-cipher-pass>\""
                                "},"
                                "\"repo1-cipher-type\":{"
                                "\"val\":\"aes-256-cbc\""
                                "},"
                                "\"repo1-path\":{"
                                    "\"val\":\"" TEST_PATH "/repo1\""
                                "}"
                            "},"
                            "\"global:backup\":{"
                                "\"reset-start-fast\":"
                                "{"
                                    "\"val\":\"bogus\","
                                    "\"warn\":\"invalid reset option\""
                                "},"
                                "\"start-fast\":{"
                                    "\"val\":\"y\""
                                "}"
                            "},"
                            "\"test1\":{"
                                "\"pg1-path\":{"
                                    "\"val\":\"" TEST_PATH "/test1-pg1\""
                                "}"
                            "},"
                            "\"test2\":{"
                                "\"pg1-path\":{"
                                    "\"val\":\"" TEST_PATH "/test2-pg1\""
                                "},"
                                "\"recovery-option\":{"
                                    "\"val\":["
                                        "\"key1=value1\","
                                        "\"key2=value2\""
                                    "]"
                                "}"
                            "}"
                        "}"
                    "}"
                "}",
                // {uncrustify_on}
                "render");

            TEST_RESULT_LOG(
                "P00   WARN: environment contains invalid option 'bogus'\n"
                "P00   WARN: environment contains invalid negate option 'no-online'\n"
                "P00   WARN: environment contains invalid reset option 'reset-compress-type'\n"
                "P00   WARN: configuration file contains negate option 'no-repo1-block'\n"
                "P00   WARN: configuration file contains invalid option 'bogus'");

            unsetenv(PGBACKREST_ENV "BOGUS");
            unsetenv(PGBACKREST_ENV "NO_ONLINE");
            unsetenv(PGBACKREST_ENV "DB_INCLUDE");
            unsetenv(PGBACKREST_ENV "RESET_COMPRESS_TYPE");
        }
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdCheck()"))
    {
        // Load Parameters
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "500ms");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fail to connect to database");

        // Set up harness to expect a failure to connect to the database
        HRN_PQ_SCRIPT_SET(
            {.function = HRN_PQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.function = HRN_PQ_STATUS, .resultInt = CONNECTION_BAD},
            {.function = HRN_PQ_ERRORMESSAGE, .resultZ = "error"},
            {.function = HRN_PQ_FINISH});

        TEST_ERROR(cmdCheck(), ConfigError, "no database found\nHINT: check indexed pg-path/pg-host configurations");
        TEST_RESULT_LOG(
            "P00   WARN: unable to check pg1: [DbConnectError] unable to connect to 'dbname='postgres' port=5432': error");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby only, repo local - fail to find primary database");

        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_96);

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_96, TEST_PATH "/pg", true, NULL, NULL),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ERROR(cmdCheck(), ConfigError, "primary database not found\nHINT: check indexed pg-path/pg-host configurations");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby only, multiple pg databases and remote repos - fail to find primary database");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 8, TEST_PATH "/pg8");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 8, "5433");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 2, "repo.domain.com");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "500ms");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_96);

        // Two standbys found but no primary
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_93(1, "dbname='postgres' port=5432", PG_VERSION_95, "/pgdata", true, NULL, NULL),
            HRN_PQ_SCRIPT_OPEN_GE_93(8, "dbname='postgres' port=5433", PG_VERSION_95, "/pgdata", true, NULL, NULL),

            HRN_PQ_SCRIPT_CLOSE(8),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ERROR(cmdCheck(), ConfigError, "primary database not found\nHINT: check indexed pg-path/pg-host configurations");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby only, one pg database and remote repo - code coverage");

        // Standby only, repo remote but only one pg-path configured
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 1, "repo.domain.com");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "500ms");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_96, "/pgdata", true, NULL, NULL),
            HRN_PQ_SCRIPT_CLOSE(1));

        // Only confirming we get passed the check for repoIsLocal || more than one pg-path configured
        TEST_ERROR(
            cmdCheck(), DbMismatchError,
            "version '9.6' and path '/pgdata' queried from cluster do not match version '9.6' and '" TEST_PATH "/pg' read from"
            " '" TEST_PATH "/pg/global/pg_control'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters.");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup-standby set without standby");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "500ms");
        hrnCfgArgRawBool(argList, cfgOptBackupStandby, true);
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        // Primary database connection ok
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_96, TEST_PATH "/pg", false, NULL, NULL),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ERROR(
            cmdCheck(), FileMissingError,
            "unable to load info file '" TEST_PATH "/repo/archive/test1/archive.info' or '" TEST_PATH
            "/repo/archive/test1/archive.info.copy':\n"
            "FileMissingError: unable to open missing file '" TEST_PATH "/repo/archive/test1/archive.info' for read\n"
            "FileMissingError: unable to open missing file '" TEST_PATH "/repo/archive/test1/archive.info.copy' for read\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.");
        TEST_RESULT_LOG(
            "P00   WARN: option 'backup-standby' is enabled but standby is not properly configured\n"
            "P00   INFO: check repo1 configuration (primary)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby and primary database - standby path doesn't match pg_control");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "500ms");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 8, TEST_PATH "/pg8");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 8, "5433");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        // Create pg_control for standby
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_15);

        // Standby database path doesn't match pg_control
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_15, TEST_PATH, true, NULL, NULL),
            HRN_PQ_SCRIPT_OPEN_GE_96(8, "dbname='postgres' port=5433", PG_VERSION_15, TEST_PATH "/pg8", false, NULL, NULL),

            HRN_PQ_SCRIPT_CLOSE(8),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ERROR(
            cmdCheck(), DbMismatchError,
            "version '" PG_VERSION_15_Z "' and path '" TEST_PATH "' queried from cluster do not match version '" PG_VERSION_15_Z
            "' and '" TEST_PATH "/pg' read from '" TEST_PATH "/pg/global/pg_control'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters.");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby and primary database - error on primary but standby check ok");

        // Create pg_control for primary
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_15);

        // Create info files
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_15_Z "\n"
            "db-version=\"15\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_15_Z ",\"db-version\":\"15\"}\n");
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=202209061\n"
            "db-control-version=1300\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_15_Z "\n"
            "db-version=\"15\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":202209061,\"db-control-version\":1300,\"db-system-id\":" HRN_PG_SYSTEMID_15_Z
            ",\"db-version\":\"15\"}\n");

        // Single repo config - error when checking archive mode setting on database
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_15, TEST_PATH "/pg", true, NULL, NULL),
            HRN_PQ_SCRIPT_OPEN_GE_96(8, "dbname='postgres' port=5433", PG_VERSION_15, TEST_PATH "/pg8", false, "off", NULL),

            HRN_PQ_SCRIPT_CLOSE(1),
            HRN_PQ_SCRIPT_CLOSE(8));

        // Error on primary but standby check ok
        TEST_ERROR(cmdCheck(), ArchiveDisabledError, "archive_mode must be enabled");
        TEST_RESULT_LOG(
            "P00   INFO: check repo1 (standby)\n"
            "P00   INFO: switch wal not performed because this is a standby");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("mulit-repo - standby and primary database");

        // Multi-repo - add a second repo (repo2)
        StringList *argListRepo2 = strLstDup(argList);
        hrnCfgArgKeyRawZ(argListRepo2, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        HRN_CFG_LOAD(cfgCmdCheck, argListRepo2);

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_15, TEST_PATH "/pg", true, NULL, NULL),
            HRN_PQ_SCRIPT_OPEN_GE_96(8, "dbname='postgres' port=5433", PG_VERSION_15, TEST_PATH "/pg8", false, NULL, NULL),

            HRN_PQ_SCRIPT_CLOSE(8),
            HRN_PQ_SCRIPT_CLOSE(1));

        // Stanza has not yet been created on repo2 but is created (and checked) on repo1
        TEST_ERROR_FMT(
            cmdCheck(), FileMissingError,
            "unable to load info file '" TEST_PATH "/repo2/archive/test1/archive.info' or"
            " '" TEST_PATH "/repo2/archive/test1/archive.info.copy':\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
            TEST_PATH "/repo2/archive/test1/archive.info", TEST_PATH "/repo2/archive/test1/archive.info.copy");
        TEST_RESULT_LOG(
            "P00   INFO: check repo1 (standby)\n"
            "P00   INFO: check repo2 (standby)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo - primary database only, WAL not found");

        HRN_STORAGE_PUT_Z(
            storageTest, "pgbackrest.conf",
            "[test1]\n"
            "pg1-path=" TEST_PATH "/pg\n");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptConfig, TEST_PATH "/pgbackrest.conf");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "500ms");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        // Create stanza files on repo2
        HRN_INFO_PUT(
            storageTest, "repo2/archive/test1/" INFO_ARCHIVE_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_15_Z "\n"
            "db-version=\"15\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_15_Z ",\"db-version\":\"15\"}\n");
        HRN_INFO_PUT(
            storageTest, "repo2/backup/test1/" INFO_BACKUP_FILE,
            "[db]\n"
            "db-catalog-version=202209061\n"
            "db-control-version=1300\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_15_Z "\n"
            "db-version=\"15\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":202209061,\"db-control-version\":1300,\"db-system-id\":" HRN_PG_SYSTEMID_15_Z
            ",\"db-version\":\"15\"}\n");

        // Error when WAL segment not found
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_15, TEST_PATH "/pg", false, NULL, NULL),
            HRN_PQ_SCRIPT_CREATE_RESTORE_POINT(1, "1/1"),
            HRN_PQ_SCRIPT_WAL_SWITCH(1, "wal", "000000010000000100000001"),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ERROR(
            cmdCheck(), ArchiveTimeoutError,
            "WAL segment 000000010000000100000001 was not archived before the 500ms timeout\n"
            "HINT: check the archive_command to ensure that all options are correct (especially --stanza).\n"
            "HINT: check the PostgreSQL server log for errors.\n"
            "HINT: run the 'start' command if the stanza was previously stopped.");
        TEST_RESULT_LOG(
            "P00   INFO: check stanza 'test1'\n"
            "P00   INFO: check repo1 configuration (primary)\n"
            "P00   INFO: check repo2 configuration (primary)\n"
            "P00   INFO: check repo1 archive for WAL (primary)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no stanzas in config file");

        HRN_STORAGE_PUT_Z(
            storageTest, "pgbackrest.conf",
            "[test1]\n");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        TEST_RESULT_VOID(cmdCheck(), "check");
        TEST_RESULT_LOG(
            "P00   WARN: no stanzas found to check\n"
            "            HINT: are there non-empty stanza sections in the configuration?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo - WAL segment switch performed once for all repos");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "500ms");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        // Create WAL segment
        Buffer *buffer = bufNew(16 * 1024 * 1024);
        memset(bufPtr(buffer), 0, bufSize(buffer));
        bufUsedSet(buffer, bufSize(buffer));

        // WAL segment switch is performed once for all repos
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_15, TEST_PATH "/pg", false, NULL, NULL),
            HRN_PQ_SCRIPT_CREATE_RESTORE_POINT(1, "1/1"),
            HRN_PQ_SCRIPT_WAL_SWITCH(1, "wal", "000000010000000100000001"),
            HRN_PQ_SCRIPT_CLOSE(1));

        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/15-1/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            buffer);
        HRN_STORAGE_PUT(
            storageRepoIdxWrite(1), STORAGE_REPO_ARCHIVE "/15-1/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            buffer);

        TEST_RESULT_VOID(cmdCheck(), "check primary, WAL archived");
        TEST_RESULT_LOG(
            "P00   INFO: check repo1 configuration (primary)\n"
            "P00   INFO: check repo2 configuration (primary)\n"
            "P00   INFO: check repo1 archive for WAL (primary)\n"
            "P00   INFO: WAL segment 000000010000000100000001 successfully archived to '" TEST_PATH "/repo/archive/test1/15-1"
            "/0000000100000001/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' on repo1\n"
            "P00   INFO: check repo2 archive for WAL (primary)\n"
            "P00   INFO: WAL segment 000000010000000100000001 successfully archived to '" TEST_PATH "/repo2/archive/test1/15-1"
            "/0000000100000001/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' on repo2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Primary == NULL (for test coverage)");

        DbGetResult dbGroup = {0};
        TEST_RESULT_VOID(checkPrimary(dbGroup), "primary == NULL");
    }

    // *****************************************************************************************************************************
    if (testBegin("checkDbConfig(), checkArchiveCommand()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkArchiveCommand() errors");

        TEST_ERROR(checkArchiveCommand(NULL), ArchiveCommandInvalidError, "archive_command '[null]' must contain " PROJECT_BIN);

        TEST_ERROR(checkArchiveCommand(strNew()), ArchiveCommandInvalidError, "archive_command '' must contain " PROJECT_BIN);

        TEST_ERROR(
            checkArchiveCommand(STRDEF("backrest")), ArchiveCommandInvalidError,
            "archive_command 'backrest' must contain " PROJECT_BIN);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkArchiveCommand() valid");

        TEST_RESULT_BOOL(checkArchiveCommand(STRDEF("pgbackrest --stanza=demo archive-push %p")), true, "archive_command valid");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() valid");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 8, TEST_PATH "/pg8");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 8, "5433");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_11);
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_11);

        DbGetResult db = {0};

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_11, TEST_PATH "/pg", false, NULL, NULL),
            HRN_PQ_SCRIPT_OPEN_GE_96(8, "dbname='postgres' port=5433", PG_VERSION_11, "/badpath", true, NULL, NULL),

            HRN_PQ_SCRIPT_CLOSE(1),
            HRN_PQ_SCRIPT_CLOSE(8));

        TEST_ASSIGN(db, dbGet(false, false, CFGOPTVAL_BACKUP_STANDBY_N), "get primary and standby");

        TEST_RESULT_VOID(checkDbConfig(PG_VERSION_11, db.primaryIdx, db.primary, false), "valid db config");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() version mismatch");

        TEST_ERROR(
            checkDbConfig(PG_VERSION_95, db.primaryIdx, db.primary, false), DbMismatchError,
            "version '" PG_VERSION_11_Z "' and path '" TEST_PATH "/pg' queried from cluster do not match version '"
            PG_VERSION_95_Z "' and '" TEST_PATH "/pg' read from '" TEST_PATH "/pg/global/pg_control'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters.");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() path mismatch");

        TEST_ERROR_FMT(
            checkDbConfig(PG_VERSION_11, db.standbyIdx, db.standby, true), DbMismatchError,
            "version '" PG_VERSION_11_Z "' and path '%s' queried from cluster do not match version '" PG_VERSION_11_Z "' and"
            " '" TEST_PATH "/pg8' read from '" TEST_PATH "/pg8/global/pg_control'\n"
            "HINT: the pg8-path and pg8-port settings likely reference different clusters.",
            strZ(dbPgDataPath(db.standby)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() archive-check=false");

        hrnCfgArgRawBool(argList, cfgOptArchiveCheck, false);
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        TEST_RESULT_VOID(checkDbConfig(PG_VERSION_11, db.primaryIdx, db.primary, false), "valid db config --no-archive-check");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() archive-check not valid for command");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 8, TEST_PATH "/pg8");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 8, "5433");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        TEST_RESULT_VOID(
            checkDbConfig(PG_VERSION_11, db.primaryIdx, db.primary, false), "valid db config, archive-check not valid for command");

        TEST_RESULT_VOID(dbFree(db.primary), "free primary");
        TEST_RESULT_VOID(dbFree(db.standby), "free standby");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() archive_mode=always not supported");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_11, TEST_PATH "/pg", false, "always", NULL),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ASSIGN(db, dbGet(true, true, CFGOPTVAL_BACKUP_STANDBY_N), "get primary");
        TEST_ERROR(
            checkDbConfig(PG_VERSION_11, db.primaryIdx, db.primary, false), FeatureNotSupportedError,
            "archive_mode=always not supported");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("disable archive_mode=always check");

        hrnCfgArgRawBool(argList, cfgOptArchiveModeCheck, false);
        HRN_CFG_LOAD(cfgCmdCheck, argList);
        TEST_RESULT_VOID(checkDbConfig(PG_VERSION_11, db.primaryIdx, db.primary, false), "check");

        TEST_RESULT_VOID(dbFree(db.primary), "free primary");
    }

    // *****************************************************************************************************************************
    if (testBegin("checkStanzaInfo(), checkStanzaInfoPg()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfo() - files match");

        InfoArchive *archiveInfo = infoArchiveNew(PG_VERSION_96, 6569239123849665679, NULL);
        InfoPgData archivePg = infoPgData(infoArchivePg(archiveInfo), infoPgDataCurrentId(infoArchivePg(archiveInfo)));

        InfoBackup *backupInfo = infoBackupNew(PG_VERSION_96, 6569239123849665679, hrnPgCatalogVersion(PG_VERSION_96), NULL);
        InfoPgData backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_RESULT_VOID(checkStanzaInfo(&archivePg, &backupPg), "stanza info files match");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfo() - corrupted backup file: system id");

        backupInfo = infoBackupNew(PG_VERSION_96, 6569239123849665999, hrnPgCatalogVersion(PG_VERSION_96), NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.6, system-id = 6569239123849665999\n"
            "HINT: this may be a symptom of repository corruption!");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfo() - corrupted backup file: system id and version");

        backupInfo = infoBackupNew(PG_VERSION_95, 6569239123849665999, hrnPgCatalogVersion(PG_VERSION_95), NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.5, system-id = 6569239123849665999\n"
            "HINT: this may be a symptom of repository corruption!");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfo() - corrupted backup file: version");

        backupInfo = infoBackupNew(PG_VERSION_95, 6569239123849665679, hrnPgCatalogVersion(PG_VERSION_95), NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.5, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfo() - corrupted backup file: db id");

        infoBackupPgSet(backupInfo, PG_VERSION_96, 6569239123849665679, hrnPgCatalogVersion(PG_VERSION_96));
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 2, version = 9.6, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfoPg() - version mismatch");

        argList = strLstNew();
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 1, TEST_CIPHER_PASS);
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        // Create pg_control
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_96);

        // Create info files
        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - encryption");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'test1' on repo1");

        // Version mismatch
        TEST_ERROR(
            checkStanzaInfoPg(
                storageRepoIdx(0), PG_VERSION_95, HRN_PG_SYSTEMID_95, cfgOptionIdxStrId(cfgOptRepoCipherType, 0),
                cfgOptionIdxStr(cfgOptRepoCipherPass, 0)),
            FileInvalidError,
            "backup and archive info files exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfoPg() - systemId mismatch");

        // SystemId mismatch
        TEST_ERROR(
            checkStanzaInfoPg(
                storageRepoIdx(0), PG_VERSION_96, 6569239123849665699, cfgOptionIdxStrId(cfgOptRepoCipherType, 0),
                cfgOptionIdxStr(cfgOptRepoCipherPass, 0)),
            FileInvalidError,
            "backup and archive info files exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
