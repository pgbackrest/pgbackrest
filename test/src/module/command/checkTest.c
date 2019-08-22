/***********************************************************************************************************************************
Test Check Command
***********************************************************************************************************************************/
#include "postgres/version.h"
#include "storage/helper.h"
#include "storage/storage.intern.h"

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

    String *pg1Path = strNewFmt("%s/pg1", testPath());
    String *pg1PathOpt = strNewFmt("--pg1-path=%s", strPtr(pg1Path));
    // *****************************************************************************************************************************
    if (testBegin("cmdCheck()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, pg1PathOpt);
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "--archive-timeout=.5");
        strLstAddZ(argList, "check");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(
            cmdCheck(), FileMissingError,
            "unable to load info file '%s/repo/archive/test1/archive.info' or '%s/repo/archive/test1/archive.info.copy':\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
            testPath(), testPath(), strPtr(strNewFmt("%s/repo/archive/test1/archive.info", testPath())),
            strPtr(strNewFmt("%s/repo/archive/test1/archive.info.copy", testPath())));

        // Create archive.info file
        storagePutNP(
            storageNewWriteNP(storageRepoWrite(), INFO_ARCHIVE_PATH_FILE_STR),
            harnessInfoChecksum(
                strNew(
                    "[db]\n"
                    "db-id=1\n"
                    "db-system-id=6569239123849665679\n"
                    "db-version=\"9.2\"\n"
                    "\n"
                    "[db:history]\n"
                    "1={\"db-id\":6569239123849665679,\"db-version\":\"9.2\"}\n")));

        // Single primary
        // -------------------------------------------------------------------------------------------------------------------------
        // Error when WAL segment not found
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", strPtr(pg1Path), false),
            HRNPQ_MACRO_CREATE_RESTORE_POINT(1, "1/1"),
            HRNPQ_MACRO_WAL_SWITCH(1, "xlog", "000000010000000100000001"),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(
            cmdCheck(), ArchiveTimeoutError,
            "WAL segment 000000010000000100000001 was not archived before the 500ms timeout\n"
            "HINT: Check the archive_command to ensure that all options are correct (especially --stanza).\n"
            "HINT: Check the PostgreSQL server log for errors.");

        // Create WAL segment
        Buffer *buffer = bufNew(16 * 1024 * 1024);
        memset(bufPtr(buffer), 0, bufSize(buffer));
        bufUsedSet(buffer, bufSize(buffer));

        // WAL segment is found
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", strPtr(pg1Path), false),
            HRNPQ_MACRO_CREATE_RESTORE_POINT(1, "1/1"),
            HRNPQ_MACRO_WAL_SWITCH(1, "xlog", "000000010000000100000001"),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        storagePutNP(
            storageNewWriteNP(
                storageRepoWrite(),
                strNew(STORAGE_REPO_ARCHIVE "/9.2-1/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
            buffer);

        TEST_RESULT_VOID(cmdCheck(), "check");
        harnessLogResult(
            strPtr(
                strNewFmt(
                    "P00   INFO: WAL segment 000000010000000100000001 successfully archived to '%s/repo/archive/test1/9.2-1/"
                        "0000000100000001/000000010000000100000001-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'",
                    testPath())));

        // Single standby
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, pg1PathOpt);
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "--archive-timeout=.5");
        strLstAddZ(argList, "check");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Set script
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", strPtr(pg1Path), true),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdCheck(), "check");
        harnessLogResult("P00   INFO: switch wal not performed because no primary was found");
    }

    // *****************************************************************************************************************************
    if (testBegin("checkDbConfig()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, pg1PathOpt);
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "check");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(checkDbConfig(PG_VERSION_92, 1, PG_VERSION_92, pg1Path), "valid db config");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            checkDbConfig(PG_VERSION_92, 1, PG_VERSION_94, pg1Path),
            DbMismatchError, "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s'"
            " read from '%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters",
            strPtr(pgVersionToStr(PG_VERSION_94)), strPtr(pg1Path), strPtr(pgVersionToStr(PG_VERSION_92)), strPtr(pg1Path),
            strPtr(pg1Path));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            checkDbConfig(PG_VERSION_92, 1, PG_VERSION_92, strNew("bogus/path")),
            DbMismatchError, "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s'"
            " read from '%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters",
            strPtr(pgVersionToStr(PG_VERSION_92)), "bogus/path", strPtr(pgVersionToStr(PG_VERSION_92)), strPtr(pg1Path),
            strPtr(pg1Path));
    }

    // *****************************************************************************************************************************
    if (testBegin("checkStanzaInfo()"))
    {
        InfoArchive *archiveInfo = infoArchiveNew(PG_VERSION_96, 6569239123849665679, cipherTypeNone, NULL);
        InfoPgData archivePg = infoPgData(infoArchivePg(archiveInfo), infoPgDataCurrentId(infoArchivePg(archiveInfo)));

        InfoBackup *backupInfo = infoBackupNew(PG_VERSION_96, 6569239123849665679, 123, 12345, cipherTypeNone, NULL);
        InfoPgData backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_RESULT_VOID(checkStanzaInfo(&archivePg, &backupPg), "stanza infor files match");

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a corrupted backup file - system id
        backupInfo = infoBackupNew(PG_VERSION_96, 6569239123849665999, 123, 12345, cipherTypeNone, NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR_FMT(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.6, system-id = 6569239123849665999\n"
            "HINT: this may be a symptom of repository corruption!");
        // -------------------------------------------------------------------------------------------------------------------------
        // Create a corrupted backup file - system id and version
        backupInfo = infoBackupNew(PG_VERSION_95, 6569239123849665999, 123, 12345, cipherTypeNone, NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR_FMT(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.5, system-id = 6569239123849665999\n"
            "HINT: this may be a symptom of repository corruption!");

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a corrupted backup file - version
        backupInfo = infoBackupNew(PG_VERSION_95, 6569239123849665679, 123, 12345, cipherTypeNone, NULL);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR_FMT(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.5, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a corrupted backup file - db id
        infoBackupPgSet(backupInfo, PG_VERSION_96, 6569239123849665679, 960, 201608131);
        backupPg = infoPgData(infoBackupPg(backupInfo), infoPgDataCurrentId(infoBackupPg(backupInfo)));

        TEST_ERROR_FMT(
            checkStanzaInfo(&archivePg, &backupPg), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 2, version = 9.6, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
