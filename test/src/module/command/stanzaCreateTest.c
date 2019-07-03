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

        // uint32_t pgControlVersion = 960;
        // uint32_t pgCatalogVersion = 201608131;

        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create");
        String *content = strNew
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
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(content)),
                "put archive info to test file");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strPtr(archiveInfoFileName))))) &&
            bufEq(
                storageGetNP(storageNewReadNP(storageTest, archiveInfoFileName)),
                storageGetNP(storageNewReadNP(storageTest, fileName)))),
            true, "    test and stanza archive info files are equal");

        content = strNew
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
                storageNewWriteNP(storageTest, fileName), harnessInfoChecksum(content)),
                "put backup info to test file");
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
