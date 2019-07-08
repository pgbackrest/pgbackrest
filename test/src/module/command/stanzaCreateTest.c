/***********************************************************************************************************************************
Test Stanza Create Command
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    String *stanza = strNew("db");
    String *fileName = strNew("test.info");
    // String *fileName2 = strNew("test2.info");
    //
    String *backupStanzaPath = strNewFmt("repo/backup/%s", strPtr(stanza));
    String *backupInfoFileName = strNewFmt("%s/backup.info", strPtr(backupStanzaPath));
    String *archiveStanzaPath = strNewFmt("repo/archive/%s", strPtr(stanza));
    String *archiveInfoFileName = strNewFmt("%s/archive.info", strPtr(archiveStanzaPath));

    StringList *argListBase = strLstNew();
    strLstAddZ(argListBase, "pgbackrest");
    strLstAdd(argListBase,  strNewFmt("--stanza=%s", strPtr(stanza)));
    strLstAdd(argListBase, strNewFmt("--pg1-path=%s/%s", testPath(), strPtr(stanza)));
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));
    strLstAddZ(argListBase, "stanza-create");

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaCreate()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - no files exist");
        String *contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentArchive)),
                "put archive info to test file");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza archive info files are equal");

        String *contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentBackup)),
                "put backup info to test file");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza backup info files are equal");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - files already exist and both are valid");

        // Remove backup.info
        storageRemoveP(storageTest, backupInfoFileName, .errorOnMissing = true);
        TEST_RESULT_BOOL(storageExistsNP(storageTest, backupInfoFileName), false, "backup.info removed");
        TEST_RESULT_VOID(cmdStanzaCreate(), "    stanza create - success with archive.info files and only backup.info.copy");

        // Remove archive.info
        storageRemoveP(storageTest, archiveInfoFileName, .errorOnMissing = true);
        TEST_RESULT_BOOL(storageExistsNP(storageTest, archiveInfoFileName), false, "archive.info removed");
        TEST_RESULT_VOID(cmdStanzaCreate(), "    stanza create - success with backup.info files and only archive.info.copy");

        // Remove info files
        storageRemoveP(storageTest, archiveInfoFileName, .errorOnMissing = true);
        storageRemoveP(storageTest, backupInfoFileName, .errorOnMissing = true);
        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - success with copy files only");

        // Remove copy files
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName)), .errorOnMissing = true);
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName)), .errorOnMissing = true);
        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - success with info files only");

        // Errors
        //--------------------------------------------------------------------------------------------------------------------------
        // Remove archive info files
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName)), .errorOnMissing = true);
        storageRemoveP(storageTest, archiveInfoFileName, .errorOnMissing = true);
        TEST_ERROR_FMT(cmdStanzaCreate(), FileMissingError,
            "backup.info exists but archive.info is missing - the repository is corrupted\n"
            "HINT: delete the stanza and run stanza-create again");

        // Add back an archive.info file and remove backup files
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName)), .errorOnMissing = true);
        storageRemoveP(storageTest, backupInfoFileName, .errorOnMissing = true);
        TEST_ERROR_FMT(cmdStanzaCreate(), FileMissingError,
            "archive.info exists but backup.info is missing - the repository is corrupted\n"
            "HINT: delete the stanza and run stanza-create again");

        // Create a corrupted backup file - system id
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.6\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put back info to file - bad system-id");
        TEST_ERROR_FMT(cmdStanzaCreate(), BackupMismatchError,
            "database version = 9.6, system-id 6569239123849665679 does not match backup version = 9.6, "
            "system-id = 6569239123849665999\nHINT: is this the correct stanza?");

        // Create a corrupted backup file - system id and version
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.5\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put back info to file - bad system-id and version");
        TEST_ERROR_FMT(cmdStanzaCreate(), BackupMismatchError,
            "database version = 9.6, system-id 6569239123849665679 does not match backup version = 9.5, "
            "system-id = 6569239123849665999\nHINT: is this the correct stanza?");

        // Create a corrupted backup file - version
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.5\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put back info to file - bad version");
        TEST_ERROR_FMT(cmdStanzaCreate(), BackupMismatchError,
            "database version = 9.6, system-id 6569239123849665679 does not match backup version = 9.5, "
            "system-id = 6569239123849665679\nHINT: is this the correct stanza?");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
