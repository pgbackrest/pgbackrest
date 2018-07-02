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
        // Initialize test variables
        //--------------------------------------------------------------------------------------------------------------------------
        String *content = NULL;
        String *fileName = strNewFmt("%s/test.ini", testPath());

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

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put archive info to file");

        InfoArchive *info = NULL;

        TEST_ASSIGN(info, infoArchiveNew(fileName, true), "    new archive info");
        TEST_RESULT_STR(strPtr(infoArchiveId(info)), "9.4-1", "    archiveId set");

        // Check PG version
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoArchiveCheckPg(info, 90400, 6569239123849665679), "check PG current");
        TEST_ERROR(infoArchiveCheckPg(info, 1, 6569239123849665679), ArchiveMismatchError,
            "WAL segment version 1 does not match archive version 90400"
            "\nHINT: are you archiving to the correct stanza?");
        TEST_ERROR(infoArchiveCheckPg(info, 90400, 1), ArchiveMismatchError,
            "WAL segment system-id 1 does not match archive system-id 6569239123849665679"
            "\nHINT: are you archiving to the correct stanza?");
        TEST_ERROR(infoArchiveCheckPg(info, 1, 1), ArchiveMismatchError,
            "WAL segment version 1 does not match archive version 90400"
            "\nWAL segment system-id 1 does not match archive system-id 6569239123849665679"
            "\nHINT: are you archiving to the correct stanza?");

        // Free
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoArchiveFree(info), "infoArchiveFree() - free archive info");
        TEST_RESULT_VOID(infoArchiveFree(NULL), "    NULL ptr");
    }
}
