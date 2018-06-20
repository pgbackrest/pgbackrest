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
    if (testBegin("infoArchiveNew(), infoArchiveCheckPg(), infoArchiveFree()"))
    {
        // Test assertions
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(infoArchiveNew(NULL, false), AssertError, "function debug assertion 'fileName != NULL' failed");
        TEST_ERROR(infoArchiveCheckPg(NULL, 1, 1), AssertError, "function debug assertion 'this != NULL' failed");
        TEST_ERROR(infoArchiveId(NULL), AssertError, "function test assertion 'this != NULL' failed");

        // Initialize test variables
        //--------------------------------------------------------------------------------------------------------------------------
        String *content = NULL;
        String *fileName = strNewFmt("%s/test.ini", testPath());

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

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put archive info to file");

        InfoArchive *info = NULL;

        TEST_ASSIGN(info, infoArchiveNew(fileName, true), "    new archive info");
        TEST_RESULT_STR(strPtr(infoArchiveId(info)), "9.4-1", "    archiveId set");

        // Check PG version
        //--------------------------------------------------------------------------------------------------------------------------
        // !!! Can't use PG_VERSION_94 - get error: 'PG_VERSION_94' undeclared (first use in this function)
        TEST_RESULT_VOID(infoArchiveCheckPg(info, 90400, 6365925855997464783), "check PG current");
        TEST_ERROR(infoArchiveCheckPg(info, 1, 6365925855997464783), ArchiveMismatchError,
            "WAL segment version 1 does not match archive version 90400"
            "\nHINT: are you archiving to the correct stanza?");
        TEST_ERROR(infoArchiveCheckPg(info, 90400, 1), ArchiveMismatchError,
            "WAL segment system-id 1 does not match archive system-id 6365925855997464783"
            "\nHINT: are you archiving to the correct stanza?");
        TEST_ERROR(infoArchiveCheckPg(info, 1, 1), ArchiveMismatchError,
            "WAL segment version 1 does not match archive version 90400"
            "\nWAL segment system-id 1 does not match archive system-id 6365925855997464783"
            "\nHINT: are you archiving to the correct stanza?");

        // Free
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoArchiveFree(info), "infoArchiveFree() - free archive info");
        TEST_RESULT_VOID(infoArchiveFree(NULL), "    NULL ptr");
    }
}
