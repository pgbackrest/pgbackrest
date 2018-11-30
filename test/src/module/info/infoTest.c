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
        TEST_ERROR(
            infoNew(storageLocal(), fileName, cipherTypeNone, NULL), FileOpenError,
            strPtr(
                strNewFmt(
                    "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
                    "FileMissingError: unable to open '%s/test.ini' for read: [2] No such file or directory\n"
                    "FileMissingError: unable to open '%s/test.ini.copy' for read: [2] No such file or directory",
                testPath(), testPath(), testPath(), testPath())));

        // Only copy exists and one is required
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileNameCopy), bufNewStr(content)), "put info.copy to file");

        TEST_ASSIGN(info, infoNew(storageLocal(), fileName, cipherTypeNone, NULL), "infoNew() - load copy file");
        TEST_RESULT_STR(strPtr(infoFileName(info)), strPtr(fileName), "    infoFileName() is set");

        TEST_RESULT_PTR(infoIni(info), info->ini, "    infoIni() returns pointer to info->ini");
        TEST_RESULT_PTR(infoCipherPass(info), NULL, "    cipherPass is not set");

        // Remove the copy and store only the main info file and encrypt it. One is required.
        //--------------------------------------------------------------------------------------------------------------------------
        StorageFileWrite *infoWrite = storageNewWriteNP(storageLocalWrite(), fileName);

        ioWriteFilterGroupSet(
            storageFileWriteIo(infoWrite),
            ioFilterGroupAdd(
                ioFilterGroupNew(),
                cipherBlockFilter(cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, bufNewStr(strNew("12345678")), NULL))));

        storageRemoveNP(storageLocalWrite(), fileNameCopy);
        storagePutNP(
            infoWrite,
            bufNewStr(
                strNew(
                    "[backrest]\n"
                    "backrest-checksum=\"9d2f6dce339751e1a056187fad67d2834b3d4ab3\"\n"
                    "backrest-format=5\n"
                    "backrest-version=\"2.04\"\n"
                    "\n"
                    "[cipher]\n"
                    "cipher-pass=\"ABCDEFGH\"\n"
                    "\n"
                    "[db]\n"
                    "db-id=1\n"
                    "db-system-id=6569239123849665679\n"
                    "db-version=\"9.4\"\n"
                    "\n"
                    "[db:history]\n"
                    "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n")));

        // Only main info exists and is required
        TEST_ASSIGN(info, infoNew(storageLocal(), fileName, cipherTypeAes256Cbc, strNew("12345678")), "infoNew() - load file");

        TEST_RESULT_STR(strPtr(infoFileName(info)), strPtr(fileName), "    infoFileName() is set");
        TEST_RESULT_STR(strPtr(infoCipherPass(info)), "ABCDEFGH", "    cipherPass is set");

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

        TEST_ERROR(
            infoNew(storageLocal(), fileName, cipherTypeNone, NULL), FileOpenError,
            strPtr(
                strNewFmt(
                    "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
                    "FormatError: invalid format in '%s/test.ini', expected 5 but found 4\n"
                    "FileMissingError: unable to open '%s/test.ini.copy' for read: [2] No such file or directory",
                testPath(), testPath(), testPath(), testPath())));

        storageCopyNP(storageNewReadNP(storageLocal(), fileName), storageNewWriteNP(storageLocalWrite(), fileNameCopy));

        TEST_ERROR(
            infoNew(storageLocal(), fileName, cipherTypeNone, NULL), FileOpenError,
            strPtr(
                strNewFmt(
                    "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
                    "FormatError: invalid format in '%s/test.ini', expected 5 but found 4\n"
                    "FormatError: invalid format in '%s/test.ini.copy', expected 5 but found 4",
                testPath(), testPath(), testPath(), testPath())));

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
            infoNew(storageLocal(), fileName, cipherTypeNone, NULL), FileOpenError,
            strPtr(
                strNewFmt(
                    "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
                    "ChecksumError: invalid checksum in '%s/test.ini', expected '4306ec205f71417c301e403c4714090e61c8a736' but"
                        " no checksum found\n"
                    "ChecksumError: invalid checksum in '%s/test.ini.copy', expected '4306ec205f71417c301e403c4714090e61c8a736'"
                        " but found '4306ec205f71417c301e403c4714090e61c8a999'",
                testPath(), testPath(), testPath(), testPath())));

        storageRemoveNP(storageLocalWrite(), fileName);
        storageRemoveNP(storageLocalWrite(), fileNameCopy);

        // infoFree()
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoFree(info), "infoFree() - free info memory context");
        TEST_RESULT_VOID(infoFree(NULL), "    NULL ptr");
    }
}
