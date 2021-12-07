/***********************************************************************************************************************************
Test Check Command
***********************************************************************************************************************************/
#include "command/stanza/create.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "postgres/version.h"
#include "storage/helper.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPostgres.h"
#include "common/harnessPq.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // PQfinish() is strictly checked
    harnessPqScriptStrictSet(true);

    StringList *argList = strLstNew();

    // *****************************************************************************************************************************
    if (testBegin("cmdCheck()"))
    {
        // Load Parameters
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, ".5");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fail to connect to database");

        // Set up harness to expect a failure to connect to the database
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_BAD},
            {.function = HRNPQ_ERRORMESSAGE, .resultZ = "error"},
            {.function = HRNPQ_FINISH},
            {.function = NULL}
        });

        TEST_ERROR(cmdCheck(), ConfigError, "no database found\nHINT: check indexed pg-path/pg-host configurations");
        TEST_RESULT_LOG(
            "P00   WARN: unable to check pg-1: [DbConnectError] unable to connect to 'dbname='postgres' port=5432': error");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby only, repo local - fail to find primary database");

        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_92);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", true, NULL, NULL),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(cmdCheck(), ConfigError, "primary database not found\nHINT: check indexed pg-path/pg-host configurations");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby only, multiple pg databases and remote repos - fail to find primary database");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 8, TEST_PATH "/pg8");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 8, "5433");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 2, "repo.domain.com");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, ".5");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_92);

        // Two standbys found but no primary
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, "/pgdata", true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, "/pgdata", true, NULL, NULL),

            HRNPQ_MACRO_CLOSE(8),
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(cmdCheck(), ConfigError, "primary database not found\nHINT: check indexed pg-path/pg-host configurations");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby only, one pg database and remote repo - code coverage");

        // Standby only, repo remote but only one pg-path configured
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoHost, 1, "repo.domain.com");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, ".5");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, "/pgdata", true, NULL, NULL),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        // Only confirming we get passed the check for repoIsLocal || more than one pg-path configured
        TEST_ERROR(
            cmdCheck(), DbMismatchError,
            "version '9.2' and path '/pgdata' queried from cluster do not match version '9.2' and '" TEST_PATH "/pg' read from"
                " '" TEST_PATH "/pg/global/pg_control'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup-standby set without standby");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, ".5");
        hrnCfgArgRawBool(argList, cfgOptBackupStandby, true);
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        // Primary database connection ok
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", false, NULL, NULL),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

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

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby and primary database - standby path doesn't match pg_control");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, ".5");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 8, TEST_PATH "/pg8");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 8, "5433");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        // Create pg_control for standby
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_92);

        // Standby database path doesn't match pg_control
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH, true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, TEST_PATH "/pg8", false, NULL, NULL),

            HRNPQ_MACRO_CLOSE(8),
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(
            cmdCheck(), DbMismatchError,
            "version '" PG_VERSION_92_STR "' and path '" TEST_PATH "' queried from cluster do not match version '" PG_VERSION_92_STR
                "' and '" TEST_PATH "/pg' read from '" TEST_PATH "/pg/global/pg_control'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby and primary database - error on primary but standby check ok");

        // Create pg_control for primary
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_92);

        // Create info files
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_92_Z "\n"
            "db-version=\"9.2\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_92_Z ",\"db-version\":\"9.2\"}\n");
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=920\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_92_Z "\n"
            "db-version=\"9.2\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":920,\"db-system-id\":" HRN_PG_SYSTEMID_92_Z ","
                "\"db-version\":\"9.2\"}\n");

        // Single repo config - error when checking archive mode setting on database
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, TEST_PATH "/pg8", false, "off", NULL),

            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_CLOSE(8),

            HRNPQ_MACRO_DONE()
        });

        // Error on primary but standby check ok
        TEST_ERROR(cmdCheck(), ArchiveDisabledError, "archive_mode must be enabled");
        TEST_RESULT_LOG(
            "P00   INFO: check repo1 (standby)\n"
            "P00   INFO: switch wal not performed because this is a standby");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("mulit-repo - standby and primary database");

        // Multi-repo - add a second repo (repo2)
        StringList *argListRepo2 = strLstDup(argList);
        hrnCfgArgKeyRawZ(argListRepo2, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        HRN_CFG_LOAD(cfgCmdCheck, argListRepo2);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, TEST_PATH "/pg8", false, NULL, NULL),

            HRNPQ_MACRO_CLOSE(8),
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

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

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo - primary database only, WAL not found");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, ".5");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        // Create stanza files on repo2
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_92_Z "\n"
            "db-version=\"9.2\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_92_Z ",\"db-version\":\"9.2\"}\n");
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=920\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_92_Z "\n"
            "db-version=\"9.2\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":920,\"db-system-id\":" HRN_PG_SYSTEMID_92_Z ","
                "\"db-version\":\"9.2\"}\n");

        // Error when WAL segment not found
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", false, NULL, NULL),
            HRNPQ_MACRO_CREATE_RESTORE_POINT(1, "1/1"),
            HRNPQ_MACRO_WAL_SWITCH(1, "xlog", "000000010000000100000001"),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(
            cmdCheck(), ArchiveTimeoutError,
            "WAL segment 000000010000000100000001 was not archived before the 500ms timeout\n"
            "HINT: check the archive_command to ensure that all options are correct (especially --stanza).\n"
            "HINT: check the PostgreSQL server log for errors.\n"
            "HINT: run the 'start' command if the stanza was previously stopped.");
        TEST_RESULT_LOG(
            "P00   INFO: check repo1 configuration (primary)\n"
            "P00   INFO: check repo2 configuration (primary)\n"
            "P00   INFO: check repo1 archive for WAL (primary)");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo - WAL segment switch performed once for all repos");

        // Create WAL segment
        Buffer *buffer = bufNew(16 * 1024 * 1024);
        memset(bufPtr(buffer), 0, bufSize(buffer));
        bufUsedSet(buffer, bufSize(buffer));

        // WAL segment switch is performed once for all repos
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", false, NULL, NULL),
            HRNPQ_MACRO_CREATE_RESTORE_POINT(1, "1/1"),
            HRNPQ_MACRO_WAL_SWITCH(1, "xlog", "000000010000000100000001"),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/9.2-1/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            buffer);
        HRN_STORAGE_PUT(
            storageRepoIdxWrite(1), STORAGE_REPO_ARCHIVE "/9.2-1/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            buffer);

        TEST_RESULT_VOID(cmdCheck(), "check primary, WAL archived");
        TEST_RESULT_LOG(
            "P00   INFO: check repo1 configuration (primary)\n"
            "P00   INFO: check repo2 configuration (primary)\n"
            "P00   INFO: check repo1 archive for WAL (primary)\n"
            "P00   INFO: WAL segment 000000010000000100000001 successfully archived to '" TEST_PATH "/repo/archive/test1/9.2-1/"
                "0000000100000001/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' on repo1\n"
            "P00   INFO: check repo2 archive for WAL (primary)\n"
            "P00   INFO: WAL segment 000000010000000100000001 successfully archived to '" TEST_PATH "/repo2/archive/test1/9.2-1/"
                "0000000100000001/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' on repo2");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Primary == NULL (for test coverage)");

        DbGetResult dbGroup = {0};
        TEST_RESULT_VOID(checkPrimary(dbGroup), "primary == NULL");
    }

    // *****************************************************************************************************************************
    if (testBegin("checkManifest()"))
    {
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argList, cfgOptPgHost, 5, "localhost");
        hrnCfgArgKeyRawZ(argList, cfgOptPgHostCmd, 5, "pgbackrest-bogus");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 5, "/path/to/pg5");
        hrnCfgArgKeyRawZ(argList, cfgOptPgHostUser, 5, TEST_USER);
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        // Placeholder test for manifest
        TEST_ERROR(
            checkManifest(), UnknownError,
            "remote-0 process on 'localhost' terminated unexpectedly [127]: bash: pgbackrest-bogus: command not found");
    }

    // *****************************************************************************************************************************
    if (testBegin("checkDbConfig(), checkArchiveCommand()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkArchiveCommand() errors");

        TEST_ERROR(checkArchiveCommand(NULL), ArchiveCommandInvalidError, "archive_command '[null]' must contain " PROJECT_BIN);

        TEST_ERROR(checkArchiveCommand(strNew()), ArchiveCommandInvalidError, "archive_command '' must contain " PROJECT_BIN);

        TEST_ERROR(
            checkArchiveCommand(STRDEF("backrest")), ArchiveCommandInvalidError,
            "archive_command 'backrest' must contain " PROJECT_BIN);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkArchiveCommand() valid");

        TEST_RESULT_BOOL(checkArchiveCommand(STRDEF("pgbackrest --stanza=demo archive-push %p")), true, "archive_command valid");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() valid");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 8, TEST_PATH "/pg8");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 8, "5433");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_92);
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_92);

        DbGetResult db = {0};

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", false, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, "/badpath", true, NULL, NULL),

            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_CLOSE(8),
            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(db, dbGet(false, false, false), "get primary and standby");

        TEST_RESULT_VOID(checkDbConfig(PG_VERSION_92, db.primaryIdx, db.primary, false), "valid db config");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() version mismatch");

        TEST_ERROR(
            checkDbConfig(PG_VERSION_94, db.primaryIdx, db.primary, false), DbMismatchError,
            "version '" PG_VERSION_92_STR "' and path '" TEST_PATH "/pg' queried from cluster do not match version '"
            PG_VERSION_94_STR "' and '" TEST_PATH "/pg' read from '" TEST_PATH "/pg/global/pg_control'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() path mismatch");

        TEST_ERROR_FMT(
            checkDbConfig(PG_VERSION_92, db.standbyIdx, db.standby, true), DbMismatchError,
            "version '" PG_VERSION_92_STR "' and path '%s' queried from cluster do not match version '" PG_VERSION_92_STR "' and"
                " '" TEST_PATH "/pg8' read from '" TEST_PATH "/pg8/global/pg_control'\n"
            "HINT: the pg8-path and pg8-port settings likely reference different clusters.",
            strZ(dbPgDataPath(db.standby)));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() archive-check=false");

        hrnCfgArgRawBool(argList, cfgOptArchiveCheck, false);
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        TEST_RESULT_VOID(checkDbConfig(PG_VERSION_92, db.primaryIdx, db.primary, false), "valid db config --no-archive-check");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() archive-check not valid for command");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 8, TEST_PATH "/pg8");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 8, "5433");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        TEST_RESULT_VOID(
            checkDbConfig(PG_VERSION_92, db.primaryIdx, db.primary, false), "valid db config, archive-check not valid for command");

        TEST_RESULT_VOID(dbFree(db.primary), "free primary");
        TEST_RESULT_VOID(dbFree(db.standby), "free standby");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkDbConfig() archive_mode=always not supported");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", false, "always", NULL),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(db, dbGet(true, true, false), "get primary");
        TEST_ERROR(
            checkDbConfig(PG_VERSION_92, db.primaryIdx, db.primary, false), FeatureNotSupportedError,
            "archive_mode=always not supported");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("disable archive_mode=always check");

        hrnCfgArgRawBool(argList, cfgOptArchiveModeCheck, false);
        HRN_CFG_LOAD(cfgCmdCheck, argList);
        TEST_RESULT_VOID(checkDbConfig(PG_VERSION_92, db.primaryIdx, db.primary, false), "check");

        TEST_RESULT_VOID(dbFree(db.primary), "free primary");
    }

    // *****************************************************************************************************************************
    if (testBegin("checkStanzaInfo(), checkStanzaInfoPg()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfo() - files match");

        InfoArchive *archiveInfo = infoArchiveNew(PG_VERSION_96, 6569239123849665679, NULL);
        InfoPgData archivePg = infoPgData(infoArchivePg(archiveInfo), infoPgDataCurrentId(infoArchivePg(archiveInfo)));

        InfoBackup *backupInfo = infoBackupNew(PG_VERSION_96, 6569239123849665679, hrnPgCatalogVersion(PG_VERSION_96), NULL);
        InfoPgData backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_RESULT_VOID(checkStanzaInfo(&archivePg, &backupPg), "stanza info files match");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfo() - corrupted backup file: system id");

        backupInfo = infoBackupNew(PG_VERSION_96, 6569239123849665999, hrnPgCatalogVersion(PG_VERSION_96), NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.6, system-id = 6569239123849665999\n"
            "HINT: this may be a symptom of repository corruption!");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfo() - corrupted backup file: system id and version");

        backupInfo = infoBackupNew(PG_VERSION_95, 6569239123849665999, hrnPgCatalogVersion(PG_VERSION_95), NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.5, system-id = 6569239123849665999\n"
            "HINT: this may be a symptom of repository corruption!");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfo() - corrupted backup file: version");

        backupInfo = infoBackupNew(PG_VERSION_95, 6569239123849665679, hrnPgCatalogVersion(PG_VERSION_95), NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.5, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfo() - corrupted backup file: db id");

        infoBackupPgSet(backupInfo, PG_VERSION_96, 6569239123849665679, hrnPgCatalogVersion(PG_VERSION_96));
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 2, version = 9.6, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");

        //--------------------------------------------------------------------------------------------------------------------------
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
                storageRepoIdx(0), PG_VERSION_94, HRN_PG_SYSTEMID_94, cfgOptionIdxStrId(cfgOptRepoCipherType, 0),
                cfgOptionIdxStr(cfgOptRepoCipherPass, 0)),
            FileInvalidError,
            "backup and archive info files exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");

        //--------------------------------------------------------------------------------------------------------------------------
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
