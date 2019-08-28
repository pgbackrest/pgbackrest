/***********************************************************************************************************************************
Test PostgreSQL Info Handler
***********************************************************************************************************************************/
#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    // *****************************************************************************************************************************
    if (testBegin("infoPgNew(), infoPgNewInternal(), infoPgSet()"))
    {
        InfoPg *infoPg = NULL;

        TEST_ASSIGN(infoPg, infoPgNew(cipherTypeNone, NULL), "infoPgNew(cipherTypeNone, NULL)");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 0, "  0 history");
        TEST_RESULT_STR(strPtr(infoCipherPass(infoPgInfo(infoPg))), NULL, "  cipherPass NULL");
        TEST_RESULT_INT(infoPgDataCurrentId(infoPg), 0, "  0 historyCurrent");

        TEST_ASSIGN(infoPg, infoPgNew(cipherTypeAes256Cbc, strNew("123xyz")), "infoPgNew(cipherTypeAes256Cbc, 123xyz)");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 0, "  0 history");
        TEST_RESULT_STR(strPtr(infoCipherPass(infoPgInfo(infoPg))), "123xyz", "  cipherPass set");
        TEST_RESULT_INT(infoPgDataCurrentId(infoPg), 0, "  0 historyCurrent");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            infoPg, infoPgSet(infoPgNew(cipherTypeNone, NULL), infoPgArchive, PG_VERSION_94, 6569239123849665679, 0, 0),
            "infoPgSet - infoPgArchive");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 1, "  1 history");
        TEST_RESULT_INT(infoPgDataCurrentId(infoPg), 0, "  0 historyCurrent");
        InfoPgData pgData = infoPgData(infoPg, infoPgDataCurrentId(infoPg));
        TEST_RESULT_INT(pgData.id, 1, "  id set");
        TEST_RESULT_INT(pgData.systemId, 6569239123849665679, "  system-id set");
        TEST_RESULT_INT(pgData.version, PG_VERSION_94, "  version set");
        TEST_RESULT_INT(pgData.catalogVersion, 0, "  catalog-version not set");
        TEST_RESULT_INT(pgData.controlVersion, 0, "  control-version set");

        TEST_ASSIGN(
            infoPg, infoPgSet(infoPg, infoPgArchive, PG_VERSION_95, 6569239123849665999, 0, 0),
            "infoPgSet - infoPgArchive second db");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 2, "  2 history");
        TEST_RESULT_INT(infoPgDataCurrentId(infoPg), 0, "  0 historyCurrent");
        pgData = infoPgData(infoPg, infoPgDataCurrentId(infoPg));
        TEST_RESULT_INT(pgData.id, 2, "  current id updated");
        TEST_RESULT_INT(pgData.systemId, 6569239123849665999, "  system-id updated");
        TEST_RESULT_INT(pgData.version, PG_VERSION_95, "  version updated");
        TEST_RESULT_INT(pgData.catalogVersion, 0, "  catalog-version not set");
        TEST_RESULT_INT(pgData.controlVersion, 0, "  control-version not set");
        TEST_RESULT_STR(strPtr(infoCipherPass(infoPgInfo(infoPg))), NULL, "  cipherPass not set");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            infoPg, infoPgSet(infoPgNew(cipherTypeAes256Cbc, strNew("123xyz")), infoPgBackup, PG_VERSION_94, 6569239123849665679,
            201409291, 942), "infoPgSet - infoPgBackup");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 1, "  1 history");
        TEST_RESULT_INT(infoPgDataCurrentId(infoPg), 0, "  0 historyCurrent");
        pgData = infoPgData(infoPg, infoPgDataCurrentId(infoPg));
        TEST_RESULT_INT(pgData.id, 1, "  id set");
        TEST_RESULT_INT(pgData.systemId, 6569239123849665679, "  system-id set");
        TEST_RESULT_INT(pgData.version, PG_VERSION_94, "  version set");
        TEST_RESULT_INT(pgData.catalogVersion, 942, "  catalog-version set");
        TEST_RESULT_INT(pgData.controlVersion, 201409291, "  control-version set");
        TEST_RESULT_STR(strPtr(infoCipherPass(infoPgInfo(infoPg))), "123xyz", "  cipherPass set");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(infoPg, infoPgNewInternal(), "infoPgNewInternal()");
        TEST_RESULT_PTR(infoPgInfo(infoPg), NULL, "  info not set");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            infoPgSet(infoPgNew(cipherTypeNone, NULL), 1000, PG_VERSION_94, 6569239123849665679, 201409291, 942),
            AssertError, "invalid InfoPg type 1000");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoPgNewLoad(), infoPgFree(), infoPgDataCurrent(), infoPgDataToLog(), infoPgAdd(), infoPgIni(), infoPgSave()"))
    {
        String *content = NULL;
        String *fileName = strNewFmt("%s/test.ini", testPath());
        String *fileNameCopy = strNewFmt("%s/test.ini.copy", testPath());
        String *fileName2 = strNewFmt("%s/test2.ini", testPath());
        String *fileName3 = strNewFmt("%s/test3.ini", testPath());

        // Archive info
        //--------------------------------------------------------------------------------------------------------------------------
        const char *contentDb =
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n";

        const char *contentDbHistory =
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n";

        content = strNew(contentDb);
        strCat(content, contentDbHistory);

        String *contentExtra = strNew(contentDb);
        strCat(
            contentExtra,
            "\n"
            "[backup:current]\n"
            "20161219-212741F={}\n");
        strCat(contentExtra, contentDbHistory);

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileNameCopy), harnessInfoChecksum(contentExtra)),
            "put info to file");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName2), harnessInfoChecksum(content)),
            "put check info to file");

        String *callbackContent = strNew("");
        InfoPg *infoPg = NULL;

        TEST_ASSIGN(
            infoPg,
            infoPgNewLoad(storageLocal(), fileName, infoPgArchive, cipherTypeNone, NULL, harnessInfoLoadCallback, callbackContent),
            "load file");
        TEST_RESULT_STR(
            strPtr(callbackContent),
            "BEGIN\n"
            "RESET\n"
            "BEGIN\n"
                "[backup:current] 20161219-212741F={}\n"
                "END\n",
            "    check callback content");
        TEST_RESULT_INT(lstSize(infoPg->history), 1, "    history record added");

        // Save the file and verify it
        Ini *ini = iniNew();
        TEST_RESULT_VOID(
            infoPgSave(infoPg, ini, storageLocalWrite(), fileName3, infoPgArchive, cipherTypeNone, NULL), "infoPgSave - archive");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetNP(storageNewReadNP(storageLocal(), fileName2)),
                storageGetNP(storageNewReadNP(storageLocal(), fileName3))),
            true, "    saved files are equal");

        InfoPgData pgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(pgData.id, 1, "    id set");
        TEST_RESULT_INT(pgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(pgData.systemId, 6569239123849665679, "    system-id set");
        TEST_RESULT_INT(pgData.catalogVersion, 0, "    catalog-version not set");
        TEST_RESULT_INT(pgData.controlVersion, 0, "    control-version not set");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 1, "    check pg data total");
        TEST_RESULT_STR(strPtr(infoPgArchiveId(infoPg, 0)), "9.4-1", "    check pg archive id");
        TEST_RESULT_PTR(infoPgCipherPass(infoPg), NULL, "    no cipher passphrase");

        // Backup info
        //--------------------------------------------------------------------------------------------------------------------------
        contentDb =
            "[db]\n"
            "db-catalog-version=201510051\n"
            "db-control-version=942\n"
            "db-id=2\n"
            "db-system-id=6365925855999999999\n"
            "db-version=\"9.5\"\n";

        contentDbHistory =
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
            "2={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6365925855999999999,"
                "\"db-version\":\"9.5\"}\n";

        content = strNew(contentDb);
        strCat(content, contentDbHistory);

        contentExtra = strNew(contentDb);
        strCat(
            contentExtra,
            "\n"
            "[backup:current]\n"
            "20161219-212741F={}\n");
        strCat(contentExtra, contentDbHistory);

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileNameCopy), harnessInfoChecksum(contentExtra)),
            "put info to file");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName2), harnessInfoChecksum(content)),
            "put check info to file");
        TEST_ASSIGN(
            infoPg, infoPgNewLoad(storageLocal(), fileName, infoPgBackup, cipherTypeNone, NULL, NULL, NULL), "load file");

        // Save the file and verify it
        ini = iniNew();
        TEST_RESULT_VOID(
            infoPgSave(infoPg, ini, storageLocalWrite(), fileName3, infoPgBackup, cipherTypeNone, NULL), "infoPgSave - backup");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetNP(storageNewReadNP(storageLocal(), fileName2)),
                storageGetNP(storageNewReadNP(storageLocal(), fileName3))),
            true, "    saved files are equal");

        TEST_RESULT_INT(infoPgDataTotal(infoPg), 2, "    check pg data total");

        pgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(pgData.id, 2, "    id set");
        TEST_RESULT_INT(pgData.version, PG_VERSION_95, "    version set");
        TEST_RESULT_INT(pgData.systemId, 6365925855999999999, "    system-id set");
        TEST_RESULT_INT(pgData.catalogVersion, 201510051, "    catalog-version set");
        TEST_RESULT_INT(pgData.controlVersion, 942, "    control-version set");

        pgData = infoPgData(infoPg, 1);
        TEST_RESULT_INT(pgData.id, 1, "    id set");
        TEST_RESULT_INT(pgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(pgData.systemId, 6569239123849665679, "    system-id set");
        TEST_RESULT_INT(pgData.catalogVersion, 201409291, "    catalog-version set");
        TEST_RESULT_INT(pgData.controlVersion, 942, "    control-version set");

        // infoPgAdd
        //--------------------------------------------------------------------------------------------------------------------------
        pgData.id = 3;
        pgData.version = PG_VERSION_96;
        pgData.systemId = 6399999999999999999;
        pgData.catalogVersion = 201608131;
        pgData.controlVersion = 960;
        TEST_RESULT_VOID(infoPgAdd(infoPg, &pgData), "infoPgAdd");

        InfoPgData pgDataTest = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(pgDataTest.id, 3, "    id set");
        TEST_RESULT_INT(pgDataTest.version, PG_VERSION_96, "    version set");
        TEST_RESULT_INT(pgDataTest.systemId, 6399999999999999999, "    system-id set");
        TEST_RESULT_INT(pgDataTest.catalogVersion, 201608131, "    catalog-version set");
        TEST_RESULT_INT(pgDataTest.controlVersion, 960, "    control-version set");

        // Errors
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            infoPgNewLoad(storageLocal(), fileName, 10, cipherTypeNone, NULL, NULL, NULL), AssertError,
            "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
            "FileMissingError: unable to open missing file '%s/test.ini' for read\n"
            "AssertError: invalid InfoPg type 10",
            testPath(), testPath(), testPath());
        TEST_ERROR(
            infoPgNewLoad(storageLocal(), NULL, infoPgBackup, cipherTypeNone, NULL, NULL, NULL), AssertError,
            "assertion 'fileName != NULL' failed");

        TEST_ERROR(infoPgDataCurrent(NULL), AssertError, "assertion 'this != NULL' failed");

        TEST_ERROR(infoPgAdd(NULL, &pgData), AssertError, "assertion 'this != NULL' failed");
        TEST_ERROR(infoPgAdd(infoPg, NULL), AssertError, "assertion 'infoPgData != NULL' failed");
        TEST_ERROR(
            infoPgSave(infoPg, ini, storageLocalWrite(), fileName2, 10000, cipherTypeNone, NULL),
            AssertError, "invalid InfoPg type 10000");

        // infoPgFree
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoPgFree(infoPg), "infoPgFree() - free infoPg");

        // infoPgDataToLog
        //--------------------------------------------------------------------------------------------------------------------------
        // test max values
        pgDataTest.id = (unsigned int)4294967295;
        pgDataTest.version = (unsigned int)4294967295;
        pgDataTest.systemId = 18446744073709551615U;
        pgDataTest.catalogVersion = (uint32_t)4294967295;
        pgDataTest.controlVersion = (uint32_t)4294967295;
        TEST_RESULT_STR(
            strPtr(infoPgDataToLog(&pgDataTest)),
            "{id: 4294967295, version: 4294967295, systemId: 18446744073709551615, catalogVersion: 4294967295, controlVersion:"
                " 4294967295}",
            "    check max format");
    }
}
