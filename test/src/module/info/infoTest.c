/***********************************************************************************************************************************
Test Info Handler
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    // *****************************************************************************************************************************
    if (testBegin("infoNew(), infoExists(), infoFileName(), infoIni()"))
    {
        // Initialize test variables
        //--------------------------------------------------------------------------------------------------------------------------
        String *content = NULL;
        String *fileName = strNewFmt("%s/test.ini", testPath());
        String *fileNameCopy = strNewFmt("%s/test.ini.copy", testPath());
        Info *info = NULL;

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

        // Info files missing and at least one is required
        //--------------------------------------------------------------------------------------------------------------------------
        String *missingInfoError = strNewFmt("unable to open %s or %s", strPtr(fileName), strPtr(fileNameCopy));

        TEST_ERROR(infoNew(storageLocal(), fileName), FileMissingError, strPtr(missingInfoError));

        // Only copy exists and one is required
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileNameCopy), bufNewStr(content)), "put info.copy to file");

        TEST_ASSIGN(info, infoNew(storageLocal(), fileName), "infoNew() - load copy file");
        TEST_RESULT_STR(strPtr(infoFileName(info)), strPtr(fileName), "    infoFileName() is set");

        TEST_RESULT_PTR(infoIni(info), info->ini, "    infoIni() returns pointer to info->ini");

        // Remove the copy and store only the main info file. One is required.
        //--------------------------------------------------------------------------------------------------------------------------
        storageMoveNP(
            storageLocal(), storageNewReadNP(storageLocal(), fileNameCopy), storageNewWriteNP(storageLocalWrite(), fileName));

        // Only main info exists and is required
        TEST_ASSIGN(info, infoNew(storageLocal(), fileName), "infoNew() - load file");

        TEST_RESULT_STR(strPtr(infoFileName(info)), strPtr(fileName), "    infoFileName() is set");

        // Invalid format
        //--------------------------------------------------------------------------------------------------------------------------
        storageRemoveNP(storageLocalWrite(), fileName);

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"14617b089cb5c9b3224e739bb794e865b9bcdf4b\"\n"
            "backrest-format=4\n"
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

        // Only main file exists but the backrest-format is invalid
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put invalid br format to file");

        TEST_ERROR(infoNew(storageLocal(), fileName), FileMissingError, strPtr(missingInfoError));
        harnessLogResult(
            strPtr(
                strNewFmt("P00   WARN: invalid format in '%s', expected %d but found %d", strPtr(fileName), REPOSITORY_FORMAT, 4)));

        storageCopyNP(storageNewReadNP(storageLocal(), fileName), storageNewWriteNP(storageLocalWrite(), fileNameCopy));

        TEST_ERROR(
            infoNew(storageLocal(), fileName), FormatError,
            strPtr(strNewFmt("invalid format in '%s', expected %d but found %d", strPtr(fileName), REPOSITORY_FORMAT, 4)));
        harnessLogResult(
            strPtr(
                strNewFmt("P00   WARN: invalid format in '%s', expected %d but found %d", strPtr(fileName), REPOSITORY_FORMAT, 4)));

        // Invalid checksum
        //--------------------------------------------------------------------------------------------------------------------------
        storageRemoveNP(storageLocalWrite(), fileName);
        storageRemoveNP(storageLocalWrite(), fileNameCopy);

        // change the checksum
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"4306ec205f71417c301e403c4714090e61c8a999\"\n"
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
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileNameCopy), bufNewStr(content)), "put invalid checksum to copy");

        // Empty checksum for main file
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\n"
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
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put empty checksum to file");

        // Copy file error
        TEST_ERROR(
            infoNew(storageLocal(), fileName), ChecksumError,
            strPtr(strNewFmt("invalid checksum in '%s', expected '%s' but found '%s'", strPtr(fileName),
            "4306ec205f71417c301e403c4714090e61c8a736", "4306ec205f71417c301e403c4714090e61c8a999")));

        // Main file warning
        harnessLogResult(strPtr(strNewFmt("P00   WARN: invalid checksum in '%s', expected '%s' but found '%s'", strPtr(fileName),
            "4306ec205f71417c301e403c4714090e61c8a736", "[undef]")));

        storageRemoveNP(storageLocalWrite(), fileName);
        storageRemoveNP(storageLocalWrite(), fileNameCopy);

        // infoFree()
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoFree(info), "infoFree() - free info memory context");
        TEST_RESULT_VOID(infoFree(NULL), "    NULL ptr");
    }
}
