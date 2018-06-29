/***********************************************************************************************************************************
Test Lists
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("infoPgNew(), infoPgFree(), infoPgDataCurrent(), infoPgDataToLog(), infoPgAdd()"))
    {
        String *content = NULL;
        String *fileName = strNewFmt("%s/test.ini", testPath());

        // Archive info
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"b34b238ce89d8e1365c9e392ce59e7b03342ceb9\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04dev\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put info to file");

        InfoPg *infoPg = NULL;

        TEST_ASSIGN(infoPg, infoPgNew(fileName, false, infoPgArchive), "new infoPg archive - load file");

        TEST_RESULT_INT(lstSize(infoPg->history), 1, "    history record added");
        TEST_RESULT_INT(infoPg->indexCurrent, 0, "    current index set");

        InfoPgData infoPgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(infoPgData.id, 1, "    id set");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(infoPgData.systemId, 6569239123849665679, "    system-id set");
        TEST_RESULT_INT(infoPgData.catalogVersion, 0, "    catalog-version not set");
        TEST_RESULT_INT(infoPgData.controlVersion, 0, "    control-version not set");

        TEST_RESULT_STR(strPtr(infoPgVersionToString(infoPgData.version)), "9.4", "    version conversion back to string");

        // Backup info
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"fc1d9ca71ebf1f5562f1fd21c4959233c9d04b18\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04dev\"\n"
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

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put info to file");

        TEST_ASSIGN(infoPg, infoPgNew(fileName, infoPgBackup), "new infoPg backup - load file");

        TEST_RESULT_INT(lstSize(infoPg->history), 1, "    history record added");
        TEST_RESULT_INT(infoPg->indexCurrent, 0, "    current index set");

        infoPgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(infoPgData.id, 1, "    id set");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(infoPgData.systemId, 6569239123849665679, "    system-id set");
        TEST_RESULT_INT(infoPgData.catalogVersion, 201409291, "    catalog-version set");
        TEST_RESULT_INT(infoPgData.controlVersion, 942, "    control-version set");

        // Manifest info
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(infoPg, infoPgNew(fileName, infoPgManifest), "new infoPg manifest - load file");

        TEST_RESULT_INT(lstSize(infoPg->history), 1, "history record added");
        TEST_RESULT_INT(infoPg->indexCurrent, 0, "current index set");

        infoPgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(infoPgData.id, 1, "    id set");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(infoPgData.systemId, 6569239123849665679, "    system-id set");
        TEST_RESULT_INT(infoPgData.catalogVersion, 201409291, "    catalog-version set");
        TEST_RESULT_INT(infoPgData.controlVersion, 942, "    control-version set");

        // infoPgAdd
        //--------------------------------------------------------------------------------------------------------------------------
        infoPgData.id = 2;
        infoPgData.version = PG_VERSION_96;
        infoPgData.systemId = 6365925855999999999;
        infoPgData.catalogVersion = 201510051;
        infoPgData.controlVersion = 942;
        TEST_RESULT_INT(infoPgAdd(infoPg, &infoPgData), 1, "infoPgAdd - currentIndex incremented");

        InfoPgData infoPgDataTest = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(infoPgDataTest.id, 2, "    id set");
        TEST_RESULT_INT(infoPgDataTest.version, PG_VERSION_96, "    version set");
        TEST_RESULT_INT(infoPgDataTest.systemId, 6365925855999999999, "    system-id set");
        TEST_RESULT_INT(infoPgDataTest.catalogVersion, 201510051, "    catalog-version set");
        TEST_RESULT_INT(infoPgDataTest.controlVersion, 942, "    control-version set");

        // Errors
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(infoPgNew(fileName, 10), AssertError, "invalid InfoPg type 10");
        TEST_ERROR(infoPgNew(NULL, infoPgManifest), AssertError, "function debug assertion 'fileName != NULL' failed");

        TEST_ERROR(infoPgDataCurrent(NULL), AssertError, "function debug assertion 'this != NULL' failed");

        TEST_ERROR(infoPgAdd(NULL, &infoPgData), AssertError, "function debug assertion 'this != NULL' failed");
        TEST_ERROR(infoPgAdd(infoPg, NULL), AssertError, "function debug assertion 'infoPgData != NULL' failed");

        // infoPgFree
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoPgFree(infoPg), "infoPgFree() - free infoPg");
        TEST_RESULT_VOID(infoPgFree(NULL), "    NULL ptr");

        // infoPgDataToLog
        //--------------------------------------------------------------------------------------------------------------------------
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_RESULT_INT(infoPgDataToLog(NULL, buffer, 4), 4, "infoPgDataToLog - format null string with too small buffer");

        TEST_RESULT_INT(strToLog(NULL, buffer, STACK_TRACE_PARAM_MAX), 4, "infoPgDataToLog - format null string");
        TEST_RESULT_STR(buffer, "null", "    check format");

        // test max values
        infoPgDataTest.id = (unsigned int)4294967295;
        infoPgDataTest.version = (unsigned int)4294967295;
        infoPgDataTest.systemId = 18446744073709551615U;
        infoPgDataTest.catalogVersion = (uint32_t)4294967295;
        infoPgDataTest.controlVersion = (uint32_t)4294967295;
        TEST_RESULT_INT(infoPgDataToLog(&infoPgDataTest, buffer, STACK_TRACE_PARAM_MAX), 110, "infoPgDataToLog - format string");
        TEST_RESULT_STR(buffer,
            "{\"id: 4294967295, version: 4294967295, systemId 18446744073709551615, catalog 4294967295, control 4294967295\"}",
            "    check max format");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoPgVersionToUIntInternal(), infoPgVersionToString()"))
    {
        String *version = NULL;

        // infoPgVersionToUIntInternal
        //--------------------------------------------------------------------------------------------------------------------------
        version = strNew("10");
        TEST_RESULT_INT(infoPgVersionToUIntInternal(version), PG_VERSION_10, "Valid pg version 10 integer identifier");

        version = strNew("15");
        TEST_ERROR(infoPgVersionToUIntInternal(version), AssertError, "version 15 is not a valid PostgreSQl version");

        version = strNew("9.3.4");
        TEST_ERROR(infoPgVersionToUIntInternal(version), AssertError, "version 9.3.4 format is invalid");

        version = strNew("abc");
        TEST_ERROR(infoPgVersionToUIntInternal(version), AssertError, "version abc format is invalid");
        TEST_ERROR(infoPgVersionToUIntInternal(NULL), AssertError, "function debug assertion 'version != NULL' failed");

        // infoPgVersionToString
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(infoPgVersionToString(PG_VERSION_11), strNew("11.0"), "infoPgVersionToString 11.0");
        TEST_RESULT_STR(infoPgVersionToString(PG_VERSION_96), strNew("9.6"), "infoPgVersionToString 9.6");
    }
}
