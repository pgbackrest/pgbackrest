/***********************************************************************************************************************************
Test PostgreSQL Info Handler
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    // *****************************************************************************************************************************
    if (testBegin("infoPgNew(), infoPgFree(), infoPgDataCurrent(), infoPgDataToLog(), infoPgAdd(), infoPgIni()"))
    {
        String *content = NULL;
        String *fileName = strNewFmt("%s/test.ini", testPath());

        // Archive info
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"1efa53e0611604ad7d833c5547eb60ff716e758c\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), BUFSTR(content)), "put info to file");

        InfoPg *infoPg = NULL;

        TEST_ASSIGN(
            infoPg, infoPgNew(storageLocal(), fileName, infoPgArchive, cipherTypeNone, NULL), "new infoPg archive - load file");

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
            "[backrest]\n"
            "backrest-checksum=\"5c17df9523543f5283efdc3c5aa7eb933c63ea0b\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04\"\n"
            "\n"
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

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), BUFSTR(content)), "put info to file");

        TEST_ASSIGN(
            infoPg, infoPgNew(storageLocal(), fileName, infoPgBackup, cipherTypeNone, NULL), "new infoPg backup - load file");

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
            "[backrest]\n"
            "backrest-checksum=\"62e4d16add4b111bed37c9e9ab0b6f60897e27da\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04\"\n"
            "\n"
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

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), BUFSTR(content)), "put info to file");

        TEST_ASSIGN(
            infoPg, infoPgNew(storageLocal(), fileName, infoPgManifest, cipherTypeNone, NULL), "new infoPg manifest - load file");

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

        // infoPgIni
        //--------------------------------------------------------------------------------------------------------------------------
        Ini *infoIni = NULL;
        TEST_ASSIGN(infoIni, infoPgIni(infoPg), "get ini from infoPg");
        TEST_RESULT_BOOL(strLstExists(iniSectionList(infoIni), strNew("backrest")), true, "    section exists in ini");

        // Errors
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(infoPgNew(storageLocal(), fileName, 10, cipherTypeNone, NULL), AssertError, "invalid InfoPg type 10");
        TEST_ERROR(
            infoPgNew(storageLocal(), NULL, infoPgManifest, cipherTypeNone, NULL), AssertError,
            "assertion 'fileName != NULL' failed");

        TEST_ERROR(infoPgDataCurrent(NULL), AssertError, "assertion 'this != NULL' failed");

        TEST_ERROR(infoPgAdd(NULL, &infoPgData), AssertError, "assertion 'this != NULL' failed");
        TEST_ERROR(infoPgAdd(infoPg, NULL), AssertError, "assertion 'infoPgData != NULL' failed");

        // infoPgFree
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoPgFree(infoPg), "infoPgFree() - free infoPg");
        TEST_RESULT_VOID(infoPgFree(NULL), "    NULL ptr");

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
