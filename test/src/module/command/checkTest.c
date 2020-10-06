/***********************************************************************************************************************************
Test Check Command
***********************************************************************************************************************************/
#include "postgres/version.h"
#include "storage/helper.h"
#include "storage/posix/storage.h"
#include "storage/storage.intern.h"

#include "command/stanza/create.h"
#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPq.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // PQfinish() is strictly checked
    harnessPqScriptStrictSet(true);

    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    String *pg1 = strNew("pg1");
    String *pg1Path = strNewFmt("%s/%s", testPath(), strZ(pg1));
    String *pg1PathOpt = strNewFmt("--pg1-path=%s", strZ(pg1Path));
    String *pg8 = strNew("pg8");
    String *pg8Path = strNewFmt("%s/%s", testPath(), strZ(pg8));
    String *pg8PathOpt = strNewFmt("--pg8-path=%s", strZ(pg8Path));
    String *stanza = strNew("test1");
    String *stanzaOpt = strNewFmt("--stanza=%s", strZ(stanza));
    StringList *argList = strLstNew();

    // *****************************************************************************************************************************
    if (testBegin("cmdCheck()"))
    {
        // Load Parameters
        argList = strLstNew();
        strLstAdd(argList, stanzaOpt);
        strLstAdd(argList, pg1PathOpt);
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "--archive-timeout=.5");
        harnessCfgLoad(cfgCmdCheck, argList);

        // Set up harness to expect a failure to connect to the database
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_BAD},
            {.function = HRNPQ_ERRORMESSAGE, .resultZ = "error"},
            {.function = HRNPQ_FINISH},
            {.function = NULL}
        });

        TEST_ERROR_FMT(cmdCheck(), ConfigError, "no database found\nHINT: check indexed pg-path/pg-host configurations");
        harnessLogResult(
            "P00   WARN: unable to check pg-1: [DbConnectError] unable to connect to 'dbname='postgres' port=5432': error");

        // Standby only, repo local
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(pg1Path), true, NULL, NULL),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(cmdCheck(), ConfigError, "primary database not found\nHINT: check indexed pg-path/pg-host configurations");

        // Standby only, repo remote but more than one pg-path configured
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, stanzaOpt);
        strLstAdd(argList, pg1PathOpt);
        strLstAddZ(argList, "--pg8-path=/path/to/standby2");
        strLstAddZ(argList, "--pg8-port=5433");
        strLstAddZ(argList, "--repo1-host=repo.domain.com");
        strLstAddZ(argList, "--archive-timeout=.5");
        harnessCfgLoad(cfgCmdCheck, argList);

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

        // Standby only, repo remote but only one pg-path configured
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, stanzaOpt);
        strLstAdd(argList, pg1PathOpt);
        strLstAddZ(argList, "--repo1-host=repo.domain.com");
        strLstAddZ(argList, "--archive-timeout=.5");
        harnessCfgLoad(cfgCmdCheck, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, "/pgdata", true, NULL, NULL),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        // Only confirming we get passed the check for isRepoLocal || more than one pg-path configured
        TEST_ERROR_FMT(cmdCheck(), FileMissingError, "unable to open missing file '%s/global/pg_control' for read", strZ(pg1Path));

        // backup-standby set without standby
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, stanzaOpt);
        strLstAdd(argList, pg1PathOpt);
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "--archive-timeout=.5");
        strLstAddZ(argList, "--backup-standby");
        harnessCfgLoad(cfgCmdCheck, argList);

        // Primary database connection ok
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(pg1Path), false, NULL, NULL),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR_FMT(
            cmdCheck(), FileMissingError, "unable to open missing file '%s' for read",
            strZ(strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(pg1Path))));
        harnessLogResult(
            strZ(strNewFmt("P00   WARN: option '%s' is enabled but standby is not properly configured",
            cfgOptionName(cfgOptBackupStandby))));

        // Standby and primary database
        // -------------------------------------------------------------------------------------------------------------------------
        // Create pg_control for standby
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(pg1))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 6569239123849665679}));

        argList = strLstNew();
        strLstAdd(argList, stanzaOpt);
        strLstAdd(argList, pg1PathOpt);
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "--archive-timeout=.5");
        strLstAdd(argList, pg8PathOpt);
        strLstAddZ(argList, "--pg8-port=5433");
        harnessCfgLoad(cfgCmdCheck, argList);

        // Standby database path doesn't match pg_control
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, testPath(), true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, strZ(pg8Path), false, NULL, NULL),

            HRNPQ_MACRO_CLOSE(8),
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR_FMT(
            cmdCheck(), DbMismatchError, "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s'"
            " read from '%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters.",
            strZ(pgVersionToStr(PG_VERSION_92)), testPath(), strZ(pgVersionToStr(PG_VERSION_92)), strZ(pg1Path), strZ(pg1Path));

        // Standby - Stanza has not yet been created
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(pg1Path), true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, strZ(pg8Path), false, NULL, NULL),

            HRNPQ_MACRO_CLOSE(8),
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR_FMT(
            cmdCheck(), FileMissingError,
            "unable to load info file '%s/repo/archive/test1/archive.info' or '%s/repo/archive/test1/archive.info.copy':\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
            testPath(), testPath(), strZ(strNewFmt("%s/repo/archive/test1/archive.info", testPath())),
            strZ(strNewFmt("%s/repo/archive/test1/archive.info.copy", testPath())));

        // Standby - Stanza created
        // -------------------------------------------------------------------------------------------------------------------------
        // Create pg_control for primary
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(pg8))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 6569239123849665679}));

        // Create info files
        storagePutP(
            storageNewWriteP(storageRepoWrite(), INFO_ARCHIVE_PATH_FILE_STR),
            harnessInfoChecksum(
                strNew(
                    "[db]\n"
                    "db-id=1\n"
                    "db-system-id=6569239123849665679\n"
                    "db-version=\"9.2\"\n"
                    "\n"
                    "[db:history]\n"
                    "1={\"db-id\":6569239123849665679,\"db-version\":\"9.2\"}\n")));

        storagePutP(
            storageNewWriteP(storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR),
            harnessInfoChecksum(
                strNew(
                    "[db]\n"
                    "db-catalog-version=201608131\n"
                    "db-control-version=920\n"
                    "db-id=1\n"
                    "db-system-id=6569239123849665679\n"
                    "db-version=\"9.2\"\n"
                    "\n"
                    "[db:history]\n"
                    "1={\"db-catalog-version\":201608131,\"db-control-version\":920,\"db-system-id\":6569239123849665679,"
                        "\"db-version\":\"9.2\"}\n")));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(pg1Path), true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, strZ(pg8Path), false, "off", NULL),

            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_CLOSE(8),

            HRNPQ_MACRO_DONE()
        });

        // Error on primary but standby check ok
        TEST_ERROR_FMT(cmdCheck(), ArchiveDisabledError, "archive_mode must be enabled");
        harnessLogResult("P00   INFO: switch wal not performed because this is a standby");

        // Single primary
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, stanzaOpt);
        strLstAdd(argList, pg1PathOpt);
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "--archive-timeout=.5");
        harnessCfgLoad(cfgCmdCheck, argList);

        // Error when WAL segment not found
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(pg1Path), false, NULL, NULL),
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

        // Create WAL segment
        Buffer *buffer = bufNew(16 * 1024 * 1024);
        memset(bufPtr(buffer), 0, bufSize(buffer));
        bufUsedSet(buffer, bufSize(buffer));

        // WAL segment is found
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(pg1Path), false, NULL, NULL),
            HRNPQ_MACRO_CREATE_RESTORE_POINT(1, "1/1"),
            HRNPQ_MACRO_WAL_SWITCH(1, "xlog", "000000010000000100000001"),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        storagePutP(
            storageNewWriteP(
                storageRepoWrite(),
                strNew(STORAGE_REPO_ARCHIVE "/9.2-1/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
            buffer);

        TEST_RESULT_VOID(cmdCheck(), "check primary, WAL archived");
        harnessLogResult(
            strZ(
                strNewFmt(
                    "P00   INFO: WAL segment 000000010000000100000001 successfully archived to '%s/repo/archive/test1/9.2-1/"
                        "0000000100000001/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'",
                    testPath())));

        // Primary == NULL (for test coverage)
        // -------------------------------------------------------------------------------------------------------------------------
        DbGetResult dbGroup = {0};
        TEST_RESULT_VOID(checkPrimary(dbGroup), "primary == NULL");
    }

    // *****************************************************************************************************************************
    if (testBegin("checkManifest()"))
    {
        argList = strLstNew();
        strLstAdd(argList, stanzaOpt);
        strLstAdd(argList, pg1PathOpt);
        strLstAddZ(argList, "--pg5-host=localhost");
        strLstAddZ(argList, "--" CFGOPT_PG5_HOST_CMD "=pgbackrest-bogus");
        strLstAddZ(argList, "--pg5-path=/path/to/pg5");
        strLstAdd(argList, strNewFmt("--pg5-host-user=%s", testUser()));
        harnessCfgLoad(cfgCmdCheck, argList);

        // Placeholder test for manifest
        TEST_ERROR(
            checkManifest(), UnknownError,
            "remote-0 process on 'localhost' terminated unexpectedly [127]: bash: pgbackrest-bogus: command not found");
    }

    // *****************************************************************************************************************************
    if (testBegin("checkDbConfig(), checkArchiveCommand()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            checkArchiveCommand(NULL), ArchiveCommandInvalidError, "archive_command '[null]' must contain " PROJECT_BIN);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            checkArchiveCommand(strNew("")), ArchiveCommandInvalidError, "archive_command '' must contain " PROJECT_BIN);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            checkArchiveCommand(strNew("backrest")), ArchiveCommandInvalidError, "archive_command 'backrest' must contain "
            PROJECT_BIN);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(checkArchiveCommand(strNew("pgbackrest --stanza=demo archive-push %p")), true, "archive_command valid");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, stanzaOpt);
        strLstAdd(argList, pg1PathOpt);
        strLstAdd(argList, pg8PathOpt);
        strLstAddZ(argList, "--pg8-port=5433");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        harnessCfgLoad(cfgCmdCheck, argList);

        DbGetResult db = {0};

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(pg1Path), false, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, "/badpath", true, NULL, NULL),

            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_CLOSE(8),
            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(db, dbGet(false, false, false), "get primary and standby");

        TEST_RESULT_VOID(checkDbConfig(PG_VERSION_92, db.primaryId, db.primary, false), "valid db config");

        // Version mismatch
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            checkDbConfig(PG_VERSION_94, db.primaryId, db.primary, false), DbMismatchError,
            "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s' read from '%s/"
            PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters.",
            strZ(pgVersionToStr(PG_VERSION_92)), strZ(pg1Path), strZ(pgVersionToStr(PG_VERSION_94)), strZ(pg1Path), strZ(pg1Path));

        // Path mismatch
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            checkDbConfig(PG_VERSION_92, db.standbyId, db.standby, true), DbMismatchError,
            "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s' read from '%s/"
            PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\n"
            "HINT: the pg8-path and pg8-port settings likely reference different clusters.",
            strZ(pgVersionToStr(PG_VERSION_92)), strZ(dbPgDataPath(db.standby)), strZ(pgVersionToStr(PG_VERSION_92)), strZ(pg8Path),
            strZ(pg8Path));

        // archive-check=false
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--no-archive-check");
        harnessCfgLoad(cfgCmdCheck, argList);

        TEST_RESULT_VOID(checkDbConfig(PG_VERSION_92, db.primaryId, db.primary, false), "valid db config --no-archive-check");

        // archive-check not valid for command
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, stanzaOpt);
        strLstAdd(argList, pg1PathOpt);
        strLstAdd(argList, pg8PathOpt);
        strLstAddZ(argList, "--pg8-port=5433");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        TEST_RESULT_VOID(
            checkDbConfig(PG_VERSION_92, db.primaryId, db.primary, false), "valid db config, archive-check not valid for command");

        TEST_RESULT_VOID(dbFree(db.primary), "free primary");
        TEST_RESULT_VOID(dbFree(db.standby), "free standby");

        // archive_mode=always not supported
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, stanzaOpt);
        strLstAdd(argList, pg1PathOpt);
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        harnessCfgLoad(cfgCmdCheck, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(pg1Path), false, "always", NULL),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(db, dbGet(true, true, false), "get primary");
        TEST_ERROR_FMT(
            checkDbConfig(PG_VERSION_92, db.primaryId, db.primary, false), FeatureNotSupportedError,
            "archive_mode=always not supported");

        TEST_RESULT_VOID(dbFree(db.primary), "free primary");
    }

    // *****************************************************************************************************************************
    if (testBegin("checkStanzaInfo(), checkStanzaInfoPg()"))
    {
        InfoArchive *archiveInfo = infoArchiveNew(PG_VERSION_96, 6569239123849665679, NULL);
        InfoPgData archivePg = infoPgData(infoArchivePg(archiveInfo), infoPgDataCurrentId(infoArchivePg(archiveInfo)));

        InfoBackup *backupInfo = infoBackupNew(PG_VERSION_96, 6569239123849665679, pgCatalogTestVersion(PG_VERSION_96), NULL);
        InfoPgData backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_RESULT_VOID(checkStanzaInfo(&archivePg, &backupPg), "stanza info files match");

        // Create a corrupted backup file - system id
        // -------------------------------------------------------------------------------------------------------------------------
        backupInfo = infoBackupNew(PG_VERSION_96, 6569239123849665999, pgCatalogTestVersion(PG_VERSION_96), NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR_FMT(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.6, system-id = 6569239123849665999\n"
            "HINT: this may be a symptom of repository corruption!");

        // Create a corrupted backup file - system id and version
        // -------------------------------------------------------------------------------------------------------------------------
        backupInfo = infoBackupNew(PG_VERSION_95, 6569239123849665999, pgCatalogTestVersion(PG_VERSION_95), NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR_FMT(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.5, system-id = 6569239123849665999\n"
            "HINT: this may be a symptom of repository corruption!");

        // Create a corrupted backup file - version
        // -------------------------------------------------------------------------------------------------------------------------
        backupInfo = infoBackupNew(PG_VERSION_95, 6569239123849665679, pgCatalogTestVersion(PG_VERSION_95), NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR_FMT(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.5, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");

        // Create a corrupted backup file - db id
        // -------------------------------------------------------------------------------------------------------------------------
        infoBackupPgSet(backupInfo, PG_VERSION_96, 6569239123849665679, pgCatalogTestVersion(PG_VERSION_96));
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR_FMT(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 2, version = 9.6, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");

        // checkStanzaInfoPg
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--no-online");
        strLstAdd(argList, stanzaOpt);
        strLstAdd(argList, strNewFmt("--pg1-path=%s/%s", testPath(), strZ(stanza)));
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "--repo1-cipher-type=aes-256-cbc");
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "12345678", true);
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        // Create info files
        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - encryption");

        // Version mismatch
        TEST_ERROR_FMT(
            checkStanzaInfoPg(storageRepo(), PG_VERSION_94, 6569239123849665679, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass)), FileInvalidError,
            "backup and archive info files exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");

        // SystemId mismatch
        TEST_ERROR_FMT(
            checkStanzaInfoPg(storageRepo(), PG_VERSION_96, 6569239123849665699, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass)), FileInvalidError,
            "backup and archive info files exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
