/***********************************************************************************************************************************
Test Archive Info Handler
***********************************************************************************************************************************/
#include "common/harnessInfo.h"

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
    if (testBegin("infoArchiveNew() and infoArchiveFree()"))
    {
        TEST_ERROR_FMT(
            infoArchiveNew(storageLocal(), fileName, true, cipherTypeNone, NULL), FileMissingError,
            "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
            "FileMissingError: unable to open '%s/test.ini' for read: [2] No such file or directory\n"
            "FileMissingError: unable to open '%s/test.ini.copy' for read: [2] No such file or directory\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
            testPath(), testPath(), testPath(), testPath());

        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), fileName), harnessInfoChecksum(content)), "put archive info to file");

        TEST_ASSIGN(info, infoArchiveNew(storageLocal(), fileName, true, cipherTypeNone, NULL), "    new archive info");
        TEST_RESULT_STR(strPtr(infoArchiveId(info)), "9.4-1", "    archiveId set");
        TEST_RESULT_PTR(infoArchivePg(info), info->infoPg, "    infoPg set");
        TEST_RESULT_PTR(infoArchiveCipherPass(info), NULL, "    no cipher passphrase");

        // Free
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoArchiveFree(info), "infoArchiveFree() - free archive info");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoArchiveIdHistoryMatch()"))
    {
        content = strNew
        (
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
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), fileName), harnessInfoChecksum(content)), "put archive info to file");

        TEST_ASSIGN(info, infoArchiveNew(storageLocal(), fileName, true, cipherTypeNone, NULL), "new archive info");
        TEST_RESULT_STR(strPtr(infoArchiveIdHistoryMatch(info, 2, 90500, 6626363367545678089)), "9.5-2", "  full match found");

        TEST_RESULT_STR(strPtr(infoArchiveIdHistoryMatch(info, 2, 90400, 6625592122879095702)), "9.4-1", "  partial match found");

        TEST_ERROR(infoArchiveIdHistoryMatch(info, 2, 90400, 6625592122879095799), ArchiveMismatchError,
            "unable to retrieve the archive id for database version '9.4' and system-id '6625592122879095799'");

        TEST_ERROR(infoArchiveIdHistoryMatch(info, 2, 90400, 6626363367545678089), ArchiveMismatchError,
            "unable to retrieve the archive id for database version '9.4' and system-id '6626363367545678089'");
    }
}
