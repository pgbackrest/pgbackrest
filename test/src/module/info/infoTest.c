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
    if (testBegin("infoNew(), infoExists(), infoFileName(), infoIni()"))
    {
        // Test assertions
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(infoNew(NULL, false), AssertError, "function debug assertion 'fileName != NULL' failed");
        TEST_ERROR(loadInternal(NULL, true), AssertError, "function debug assertion 'this != NULL' failed");
        TEST_ERROR(infoIni(NULL), AssertError, "function debug assertion 'this != NULL' failed");
        TEST_ERROR(infoFileName(NULL), AssertError, "function test assertion 'this != NULL' failed");
        TEST_ERROR(infoExists(NULL), AssertError, "function debug assertion 'this != NULL' failed");
        TEST_ERROR(infoValidInternal(NULL, true), AssertError, "function test assertion 'this != NULL' failed");

        // Initialize test variables
        //--------------------------------------------------------------------------------------------------------------------------
        String *content = NULL;
        String *fileName = strNewFmt("%s/test.ini", testPath());
        String *fileNameCopy = strNewFmt("%s/test.ini.copy", testPath());
        Info *info = NULL;

        // content = strNew
        // (
        //     "[backrest]\n"
        //     "backrest-checksum=\"4306ec205f71417c301e403c4714090e61c8a736\"\n"
        //     "backrest-format=5\n"
        //     "backrest-version=\"1.23\"\n"
        //     "\n"
        //     "[db]\n"
        //     "db-id=1\n"
        //     "db-system-id=6455618988686438683\n"
        //     "db-version=\"9.6\"\n"
        //     "\n"
        //     "[db:history]\n"
        //     "1={\"db-id\":6455618988686438683,\"db-version\":\"9.6\"}\n"
        //     "2={\"db-id\":6457457208275135411,\"db-version\":\"9.6\"}\n"
        // );

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

        // Info files missing and at least one is required
        //--------------------------------------------------------------------------------------------------------------------------
        String *missingInfoError = strNewFmt("unable to open %s or %s", strPtr(fileName), strPtr(fileNameCopy));

        TEST_ERROR(infoNew(fileName, false), FileMissingError, strPtr(missingInfoError));
        TEST_ASSIGN(info, infoNew(fileName, true), "infoNew() - no files, ignore missing");
        TEST_RESULT_BOOL(infoExists(info), false, "    infoExists() is false");
        TEST_RESULT_STR(strPtr(infoFileName(info)), strPtr(fileName), "    infoFileName() is set");

        // Only copy exists and one is required
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileNameCopy), bufNewStr(content)), "put info.copy to file");

        TEST_ASSIGN(info, infoNew(fileName, false), "infoNew() - load copy file");
        TEST_RESULT_STR(strPtr(infoFileName(info)), strPtr(fileName), "    infoFileName() is set");
        TEST_RESULT_BOOL(infoExists(info), true, "    infoExists() is true");

        TEST_RESULT_PTR(infoIni(info), info->ini, "    infoIni() returns pointer to info->ini");

        // Remove the copy and store only the main info file. One is required.
        //--------------------------------------------------------------------------------------------------------------------------
        storageMoveNP(storageNewReadNP(storageLocal(), fileNameCopy), storageNewWriteNP(storageLocalWrite(), fileName));

        // Only main info exists and is required
        TEST_ASSIGN(info, infoNew(fileName, false), "infoNew() - load file");

        TEST_RESULT_STR(strPtr(infoFileName(info)), strPtr(fileName), "    infoFileName() is set");
        TEST_RESULT_BOOL(infoExists(info), true, "    infoExists() is true");

        // Invalid format
        //--------------------------------------------------------------------------------------------------------------------------
        storageRemoveNP(storageLocalWrite(), fileName);

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"4306ec205f71417c301e403c4714090e61c8a736\"\n"
            "backrest-format=999\n"
            "backrest-version=\"1.23\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6455618988686438683\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6455618988686438683,\"db-version\":\"9.6\"}\n"
            "2={\"db-id\":6457457208275135411,\"db-version\":\"9.6\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put invalid br format to file");

        TEST_ERROR(infoNew(fileName, false), FileMissingError, strPtr(missingInfoError));

        storageCopyNP(storageNewReadNP(storageLocal(), fileName), storageNewWriteNP(storageLocalWrite(), fileNameCopy));

        TEST_ERROR(
            infoNew(fileName, false), FormatError,
            strPtr(strNewFmt("invalid format in '%s', expected %d but found %d", strPtr(fileName), PGBACKREST_FORMAT, 999)));

        // Invalid checksum
        //--------------------------------------------------------------------------------------------------------------------------
        storageRemoveNP(storageLocalWrite(), fileName);

        // change the checksum
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"4306ec205f71417c301e403c4714090e61c8a736\"\n"
            "backrest-format=5\n"
            "backrest-version=\"1.23\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6455618988686438683\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6455618988686438683,\"db-version\":\"9.6\"}\n"
            "2={\"db-id\":6457457208275135411,\"db-version\":\"9.6\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileNameCopy), bufNewStr(content)), "put invalid checksum to file");

        TEST_ERROR(
            infoNew(fileName, false), ChecksumError,
            strPtr(strNewFmt("invalid checksum in '%s', expected '%s' but found '%s'", strPtr(fileNameCopy),
            "4306ec205f71417c301e403c4714090e61c8a736", "4306ec205f71417c301e403c4714090e61c8a999")));

        // infoFree()
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoFree(info), "infoFree() - free info memory context");
        TEST_RESULT_VOID(infoFree(NULL), "    NULL ptr");
    }

    // // *****************************************************************************************************************************
    // if (testBegin("test hash"))
    // {
    //     String *content = NULL;
    //     String *hashName = strNewFmt("%s/hash.ini", testPath());
    //     Info *info = NULL;
    //
    //     content = strNew
    //     (
    //         [backrest]
    //         backrest-checksum="b34b238ce89d8e1365c9e392ce59e7b03342ceb9"
    //         backrest-format=5
    //         backrest-version="2.04dev"
    //
    //         [db]
    //         db-id=1
    //         db-system-id=6569239123849665679
    //         db-version="9.4"
    //
    //         [db:history]
    //         1={"db-id":6569239123849665679,"db-version":"9.4"}
    //     );
    //
    //     TEST_RESULT_VOID(
    //         storagePutNP(storageNewWriteNP(storageLocalWrite(), hashName), bufNewStr(content)), "put hashed contents to file");
    //
    //     TEST_ASSIGN(info, infoNew(hashName, false), "   load hash file");
    // }
}
