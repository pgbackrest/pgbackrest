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
        // CSHANG I'm a little confused, in production, will these spit out the file name and function?
        TEST_ERROR(infoNew(NULL, false), AssertError, "function debug assertion 'fileName != NULL' failed");
        TEST_ERROR(loadInternal(NULL, true), AssertError, "function debug assertion 'this != NULL' failed");
        TEST_ERROR(infoIni(NULL), AssertError, "function debug assertion 'this != NULL' failed");
        TEST_ERROR(infoFileName(NULL), AssertError, "function test assertion 'this != NULL' failed");
        TEST_ERROR(infoExists(NULL), AssertError, "function test assertion 'this != NULL' failed");

        // Initialize test variables
        //--------------------------------------------------------------------------------------------------------------------------
        String *content = NULL;
        String *fileName = strNewFmt("%s/test.ini", testPath());
        String *fileNameCopy = strNewFmt("%s/test.ini.copy", testPath());
        Info *info = NULL;

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"18a65555903b0e2a3250d141825de809409eb1cf\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.02dev\"\n"
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

        // Invalid format number
        //--------------------------------------------------------------------------------------------------------------------------
        storageRemoveNP(storageLocalWrite(), fileName);

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"18a65555903b0e2a3250d141825de809409eb1cf\"\n"
            "backrest-format=999\n"
            "backrest-version=\"2.02dev\"\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put invalid info to file");

        // ??? If only the info main file exists but is invalid, currently "missing" error thrown since invalid main is ignored
        TEST_ERROR(infoNew(fileName, false), FileMissingError, strPtr(missingInfoError));

        storageCopyNP(storageNewReadNP(storageLocal(), fileName), storageNewWriteNP(storageLocalWrite(), fileNameCopy));

        TEST_ERROR(
            infoNew(fileName, false), FormatError,
            strPtr(strNewFmt("invalid format in '%s', expected %d but found %d", strPtr(fileName), PGBACKREST_FORMAT, 999)));

        TEST_RESULT_VOID(infoFree(info), "infoFree() - free info memory context");
        TEST_RESULT_VOID(infoFree(NULL), "    NULL ptr");
    }
}
