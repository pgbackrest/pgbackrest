/***********************************************************************************************************************************
Test Archive Info Handler
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    // Initialize test variables
    //--------------------------------------------------------------------------------------------------------------------------
    String *content = NULL;
    String *fileName = strNewFmt("%s/test.ini", testPath());
    InfoArchive *info = NULL;

    // *****************************************************************************************************************************
    if (testBegin("infoArchiveNew(), infoArchiveCheckPg(), infoArchiveFree()"))
    {


        TEST_ERROR_FMT(
            infoArchiveNew(storageLocal(), fileName, true), FileMissingError,
            "unable to open %s/test.ini or %s/test.ini.copy\n"
            "HINT: archive.info does not exist but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
            testPath(), testPath());

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

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put archive info to file");

        TEST_ASSIGN(info, infoArchiveNew(storageLocal(), fileName, true), "    new archive info");
        TEST_RESULT_STR(strPtr(infoArchiveId(info)), "9.4-1", "    archiveId set");
        TEST_RESULT_PTR(infoArchivePg(info), info->infoPg, "    infoPg set");

        // Check PG version
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoArchiveCheckPg(info, 90400, 6569239123849665679), "check PG current");
        TEST_ERROR(infoArchiveCheckPg(info, 90500, 6569239123849665679), ArchiveMismatchError,
            "WAL segment version 9.5 does not match archive version 9.4"
            "\nHINT: are you archiving to the correct stanza?");
        TEST_ERROR(infoArchiveCheckPg(info, 90400, 1), ArchiveMismatchError,
            "WAL segment system-id 1 does not match archive system-id 6569239123849665679"
            "\nHINT: are you archiving to the correct stanza?");
        TEST_ERROR(infoArchiveCheckPg(info, 100000, 1), ArchiveMismatchError,
            "WAL segment version 10 does not match archive version 9.4"
            "\nWAL segment system-id 1 does not match archive system-id 6569239123849665679"
            "\nHINT: are you archiving to the correct stanza?");

        // Free
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoArchiveFree(info), "infoArchiveFree() - free archive info");
        TEST_RESULT_VOID(infoArchiveFree(NULL), "    NULL ptr");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoArchiveIdHistoryMatch()"))
    {
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"075a202d42c3b6a0257da5f73a68fa77b342f777\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.08dev\"\n"
            "\n"
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6626363367545678089,\"db-version\":\"9.5\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put archive info to file");

        TEST_ASSIGN(info, infoArchiveNew(storageLocal(), fileName, true), "new archive info");
        TEST_RESULT_STR(strPtr(infoArchiveIdHistoryMatch(info, 2, 90500, 6626363367545678089)), "9.5-2", "  full match found");

        TEST_RESULT_STR(strPtr(infoArchiveIdHistoryMatch(info, 2, 90400, 6625592122879095702)), "9.4-1", "  partial match found");

        TEST_ERROR(infoArchiveIdHistoryMatch(info, 2, 90400, 6625592122879095799), ArchiveMismatchError,
            "unable to retrieve the archive id for database version '9.4' and system-id '6625592122879095799'");

        TEST_ERROR(infoArchiveIdHistoryMatch(info, 2, 90400, 6626363367545678089), ArchiveMismatchError,
            "unable to retrieve the archive id for database version '9.4' and system-id '6626363367545678089'");
    }
}
