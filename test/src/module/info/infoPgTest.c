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
    if (testBegin("infoPgNew(), infoPgFree()"))
    {
        String *content = NULL;
        String *fileName = strNewFmt("%s/test.ini", testPath());

        // Archive info
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"18a65555903b0e2a3250d141825de809409eb1cf\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.02dev\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6365925855997464783\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6365925855997464783,\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put info to file");

        InfoPg *infoPg = NULL;

        TEST_ASSIGN(infoPg, infoPgNew(fileName, false, infoPgArchive), "new infoPg archive - load file");

        TEST_RESULT_INT(lstSize(infoPg->history), 1, "    history record added");
        TEST_RESULT_INT(infoPg->indexCurrent, 0, "    current index set");

        InfoPgData infoPgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(infoPgData.id, 1, "    id set");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(infoPgData.systemId, 6365925855997464783, "    system-id set");
        TEST_RESULT_INT(infoPgData.catalogVersion, 0, "    catalog-version not set");
        TEST_RESULT_INT(infoPgData.controlVersion, 0, "    control-version not set");

        TEST_RESULT_STR(strPtr(infoPgVersionToString(infoPgData.version)), "9.4", "    version conversion back to string");

        // Backup info
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"18a65555903b0e2a3250d141825de809409eb1cf\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.02dev\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6365925855997464783\n"
            "db-version=\"9.4\"\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,"
            "\"db-system-id\":6365925855997464783,\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put info to file");

        TEST_ASSIGN(infoPg, infoPgNew(fileName, false, infoPgBackup), "new infoPg backup - load file");

        TEST_RESULT_INT(lstSize(infoPg->history), 1, "    history record added");
        TEST_RESULT_INT(infoPg->indexCurrent, 0, "    current index set");

        infoPgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(infoPgData.id, 1, "    id set");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(infoPgData.systemId, 6365925855997464783, "    system-id set");
        TEST_RESULT_INT(infoPgData.catalogVersion, 201409291, "    catalog-version set");
        TEST_RESULT_INT(infoPgData.controlVersion, 942, "    control-version set");

        // Manifest info
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(infoPg, infoPgNew(fileName, false, infoPgBackup), "new infoPg manifest - load file");

        TEST_RESULT_INT(lstSize(infoPg->history), 1, "history record added");
        TEST_RESULT_INT(infoPg->indexCurrent, 0, "current index set");

        infoPgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(infoPgData.id, 1, "    id set");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(infoPgData.systemId, 6365925855997464783, "    system-id set");
        TEST_RESULT_INT(infoPgData.catalogVersion, 201409291, "    catalog-version set");
        TEST_RESULT_INT(infoPgData.controlVersion, 942, "    control-version set");

        // Invalid type
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(infoPgNew(fileName, false, 10), AssertError, "invalid InfoPg type 10");

        // Free
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoPgFree(infoPg), "free infoPg");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoPgVersionToUIntInternal()"))
    {
        String *version = NULL;

        version = strNew("10");
        TEST_RESULT_INT(infoPgVersionToUIntInternal(version), PG_VERSION_10, "Valid pg version 10 integer identifier");

        version = strNew("15");
        TEST_ERROR(infoPgVersionToUIntInternal(version), AssertError, "version 15 is not a valid PostgreSQl version");

        version = strNew("9.3.4");
        TEST_ERROR(infoPgVersionToUIntInternal(version), AssertError, "version 9.3.4 format is invalid");

        version = strNew("abc");
        TEST_ERROR(infoPgVersionToUIntInternal(version), AssertError, "version abc format is invalid");
    }
}
