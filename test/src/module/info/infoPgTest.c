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
    if (testBegin("infoPgNew(), infoPgSet()"))
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
        TEST_ERROR(
            infoPgSet(infoPgNew(cipherTypeAes256Cbc, strNew("123xyz")), infoPgBackup, PG_VERSION_94, 6569239123849665679, 0, 942),
            AssertError, "assertion 'type == infoPgArchive || (pgControlVersion != 0 && pgCatalogVersion != 0)' failed");
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
        TEST_ASSIGN(
            infoPg, infoPgSet(infoPgNew(cipherTypeNone, NULL), infoPgManifest, PG_VERSION_95, 6569239123849665699,
            201510051, 950), "infoPgSet - infoPgManifest");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 1, "  1 history");
        TEST_RESULT_INT(infoPgDataCurrentId(infoPg), 0, "  0 historyCurrent");
        pgData = infoPgData(infoPg, infoPgDataCurrentId(infoPg));
        TEST_RESULT_INT(pgData.id, 1, "  id set");
        TEST_RESULT_INT(pgData.systemId, 6569239123849665699, "  system-id set");
        TEST_RESULT_INT(pgData.version, PG_VERSION_95, "  version set");
        TEST_RESULT_INT(pgData.catalogVersion, 950, "  catalog-version set");
        TEST_RESULT_INT(pgData.controlVersion, 201510051, "  control-version set");

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
        String *fileName2 = strNewFmt("%s/test2.ini", testPath());
        String *fileName3 = strNewFmt("%s/test3.ini", testPath());

        // Archive info
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), harnessInfoChecksum(content)), "put info to file");

        InfoPg *infoPg = NULL;
        Ini *ini = NULL;

        TEST_ASSIGN(
            infoPg, infoPgNewLoad(storageLocal(), fileName, infoPgArchive, cipherTypeNone, NULL, &ini), "load file");
        TEST_RESULT_STR(strPtr(iniGet(ini, strNew("db"), strNew("db-id"))), "1", "    check ini");

        // Save the file and verify it
        ini = iniNew();
        TEST_RESULT_VOID(
            infoPgSave(infoPg, ini, storageLocalWrite(), fileName3, infoPgArchive, cipherTypeNone, NULL), "infoPgSave - archive");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetNP(storageNewReadNP(storageLocal(), fileName)),
                storageGetNP(storageNewReadNP(storageLocal(), fileName3))),
            true, "    saved files are equal");

        TEST_RESULT_INT(lstSize(infoPg->history), 1, "    history record added");

        InfoPgData infoPgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(infoPgData.id, 1, "    id set");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(infoPgData.systemId, 6569239123849665679, "    system-id set");
        TEST_RESULT_INT(infoPgData.catalogVersion, 0, "    catalog-version not set");
        TEST_RESULT_INT(infoPgData.controlVersion, 0, "    control-version not set");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 1, "    check pg data total");
        TEST_RESULT_STR(strPtr(infoPgArchiveId(infoPg, 0)), "9.4-1", "    check pg archive id");
        TEST_RESULT_PTR(infoPgCipherPass(infoPg), NULL, "    no cipher passphrase");

        // Backup info
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), harnessInfoChecksum(content)), "put info to file");
        TEST_ASSIGN(
            infoPg, infoPgNewLoad(storageLocal(), fileName, infoPgBackup, cipherTypeNone, NULL, NULL), "load file");

        // Save the file and verify it
        ini = iniNew();
        TEST_RESULT_VOID(
            infoPgSave(infoPg, ini, storageLocalWrite(), fileName3, infoPgBackup, cipherTypeNone, NULL), "infoPgSave - backup");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetNP(storageNewReadNP(storageLocal(), fileName)),
                storageGetNP(storageNewReadNP(storageLocal(), fileName3))),
            true, "    saved files are equal");
        TEST_RESULT_INT(lstSize(infoPg->history), 1, "    history record added");

        infoPgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(infoPgData.id, 1, "    id set");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(infoPgData.systemId, 6569239123849665679, "    system-id set");
        TEST_RESULT_INT(infoPgData.catalogVersion, 201409291, "    catalog-version set");
        TEST_RESULT_INT(infoPgData.controlVersion, 942, "    control-version set");

        // Manifest info
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[db]\n"
            "db-catalog-version=201510051\n"
            "db-control-version=942\n"
            "db-id=2\n"
            "db-system-id=6365925855999999999\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
            "2={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6365925855999999999,"
                "\"db-version\":\"9.5\"}\n"
        );

        // Put the file and load it
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), harnessInfoChecksum(content)), "put info to file");
        TEST_ASSIGN(
            infoPg, infoPgNewLoad(storageLocal(), fileName, infoPgManifest, cipherTypeNone, NULL, NULL), "load file");

        // Save the file and verify it
        ini = iniNew();
        TEST_RESULT_VOID(
            infoPgSave(infoPg, ini, storageLocalWrite(), fileName2, infoPgManifest, cipherTypeNone, NULL), "infoPgSave - manifest");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetNP(storageNewReadNP(storageLocal(), fileName)),
                storageGetNP(storageNewReadNP(storageLocal(), fileName2))),
            true, "    saved files are equal");

        TEST_RESULT_INT(lstSize(infoPg->history), 2, "history record added");

        infoPgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(infoPgData.id, 2, "    id set");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_95, "    version set");
        TEST_RESULT_INT(infoPgData.systemId, 6365925855999999999, "    system-id set");
        TEST_RESULT_INT(infoPgData.catalogVersion, 201510051, "    catalog-version set");
        TEST_RESULT_INT(infoPgData.controlVersion, 942, "    control-version set");

        // infoPgAdd
        //--------------------------------------------------------------------------------------------------------------------------
        infoPgData.id = 3;
        infoPgData.version = PG_VERSION_96;
        infoPgData.systemId = 6399999999999999999;
        infoPgData.catalogVersion = 201608131;
        infoPgData.controlVersion = 960;
        TEST_RESULT_VOID(infoPgAdd(infoPg, &infoPgData), "infoPgAdd");

        InfoPgData infoPgDataTest = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(infoPgDataTest.id, 3, "    id set");
        TEST_RESULT_INT(infoPgDataTest.version, PG_VERSION_96, "    version set");
        TEST_RESULT_INT(infoPgDataTest.systemId, 6399999999999999999, "    system-id set");
        TEST_RESULT_INT(infoPgDataTest.catalogVersion, 201608131, "    catalog-version set");
        TEST_RESULT_INT(infoPgDataTest.controlVersion, 960, "    control-version set");

        // Errors
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(infoPgNewLoad(storageLocal(), fileName, 10, cipherTypeNone, NULL, NULL), AssertError, "invalid InfoPg type 10");
        TEST_ERROR(
            infoPgNewLoad(storageLocal(), NULL, infoPgManifest, cipherTypeNone, NULL, NULL), AssertError,
            "assertion 'fileName != NULL' failed");

        TEST_ERROR(infoPgDataCurrent(NULL), AssertError, "assertion 'this != NULL' failed");

        TEST_ERROR(infoPgAdd(NULL, &infoPgData), AssertError, "assertion 'this != NULL' failed");
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
        infoPgDataTest.id = (unsigned int)4294967295;
        infoPgDataTest.version = (unsigned int)4294967295;
        infoPgDataTest.systemId = 18446744073709551615U;
        infoPgDataTest.catalogVersion = (uint32_t)4294967295;
        infoPgDataTest.controlVersion = (uint32_t)4294967295;
        TEST_RESULT_STR(
            strPtr(infoPgDataToLog(&infoPgDataTest)),
            "{id: 4294967295, version: 4294967295, systemId: 18446744073709551615, catalogVersion: 4294967295, controlVersion:"
                " 4294967295}",
            "    check max format");
    }
}
