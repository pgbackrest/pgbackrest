/***********************************************************************************************************************************
Test Stanza Create Command
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "postgres/version.h"
#include "command/stanza/stanzaCreate.h"

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
    String *backupStanzaPath = strNewFmt("repo/backup/%s", strPtr(stanza));
    String *backupInfoFileName = strNewFmt("%s/backup.info", strPtr(backupStanzaPath));
    String *archiveStanzaPath = strNewFmt("repo/archive/%s", strPtr(stanza));
    String *archiveInfoFileName = strNewFmt("%s/archive.info", strPtr(archiveStanzaPath));

    StringList *argListBase = strLstNew();
    strLstAddZ(argListBase, "pgbackrest");
    strLstAdd(argListBase,  strNewFmt("--stanza=%s", strPtr(stanza)));
    strLstAdd(argListBase, strNewFmt("--pg1-path=%s/%s", testPath(), strPtr(stanza)));
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));


    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaUpgrade()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create pg_control
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "stanza-upgrade");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - files already exist and both are valid");
        harnessLogResult("P00   INFO: stanza db is already up to date");

        // Remove the copy files
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName)));
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName)));

        //--------------------------------------------------------------------------------------------------------------------------
        // backup info up to date but archive info db-id mismatch
        String *contentArchive = strNew
        (
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");

        TEST_ERROR_FMT(cmdStanzaUpgrade(), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 2, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");

        //--------------------------------------------------------------------------------------------------------------------------
        // backup info up to date but archive info version is not
        String *contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - archive.info file upgraded - version");
        contentArchive = strNew
        (
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentArchive)),
                "    put archive info to test file");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza archive info files are equal");

        //--------------------------------------------------------------------------------------------------------------------------
        // archive info up to date but backup info version is not
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
                "put backup info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - backup.info file upgraded - version");
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentBackup)),
                "    put backup info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza backup info files are equal");

        //--------------------------------------------------------------------------------------------------------------------------
        // backup info up to date but archive info system-id is not
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - archive.info file upgraded - system-id");
        contentArchive = strNew
        (
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentArchive)),
                "    put archive info to test file");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza archive info files are equal");

        //--------------------------------------------------------------------------------------------------------------------------
        // archive info up to date but backup info system-id is not
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
                "put backup info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - backup.info file upgraded - system-id");
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.6\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(contentBackup)),
                "    put backup info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(backupInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, backupInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza backup info files are equal");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
