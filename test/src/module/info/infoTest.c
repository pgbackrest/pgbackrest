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
    if (testBegin("info"))
    {
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
        TEST_ASSIGN(info, infoNew(fileName, true), "new info - no files, ignore missing");
        TEST_RESULT_BOOL(infoExists(info), false, "    exists is false");
        TEST_RESULT_STR(strPtr(infoFileName(info)), strPtr(fileName), "    info filename is set");

        // Only copy exists and one is required
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileNameCopy), bufNewStr(content)), "put info.copy to file");

        TEST_ASSIGN(info, infoNew(fileName, false), "new info - load copy file");
        TEST_RESULT_STR(strPtr(infoFileName(info)), strPtr(fileName), "    info filename");
        TEST_RESULT_BOOL(infoExists(info), true, "    file exists");

        TEST_RESULT_PTR(infoIni(info), info->ini, "    infoIni returns pointer to info->ini");

        // Remove the copy and store only the main info file. One is required.
        //--------------------------------------------------------------------------------------------------------------------------
        storageMoveNP(storageNewReadNP(storageLocal(), fileNameCopy), storageNewWriteNP(storageLocalWrite(), fileName));

        // Only main info exists and is required
        TEST_ASSIGN(info, infoNew(fileName, false), "new info - load file");

        TEST_RESULT_STR(strPtr(infoFileName(info)), strPtr(fileName), "    info filename");
        TEST_RESULT_BOOL(infoExists(info), true, "    file exists");

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

        TEST_RESULT_VOID(infoFree(info), "free info");
    }
}
