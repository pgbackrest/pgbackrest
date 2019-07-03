/***********************************************************************************************************************************
Test Archive Info Handler
***********************************************************************************************************************************/
#include "storage/storage.intern.h"
#include <stdio.h>
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
    String *fileName2 = strNewFmt("%s/test2.ini", testPath());
    InfoArchive *info = NULL;
    String *cipherPass = strNew("123xyz");

    // *****************************************************************************************************************************
    if (testBegin(
        "infoArchiveNewLoad(), infoArchiveNew(), infoArchiveNewInternal(), infoArchivePg(), infoArchiveCipherPass(), "
        "infoArchiveSave(), infoArchiveFree()"))
    {
        TEST_ERROR_FMT(
            infoArchiveNewLoad(storageLocal(), fileName, cipherTypeNone, NULL), FileMissingError,
            "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
            testPath(), testPath(), strPtr(strNewFmt("%s/test.ini", testPath())),
            strPtr(strNewFmt("%s/test.ini.copy", testPath())));

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

        TEST_ASSIGN(info, infoArchiveNewLoad(storageLocal(), fileName, cipherTypeNone, NULL), "    load new archive info");
        TEST_RESULT_STR(strPtr(infoArchiveId(info)), "9.4-1", "    archiveId set");
        TEST_RESULT_PTR(infoArchivePg(info), info->infoPg, "    infoPg set");
        TEST_RESULT_PTR(infoArchiveCipherPass(info), NULL, "    no cipher sub");

        TEST_RESULT_VOID(
            infoArchiveSave(info, storageLocalWrite(), fileName2, cipherTypeNone, NULL), "infoArchiveSave() - no cipher");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetNP(storageNewReadNP(storageLocal(), fileName)),
                storageGetNP(storageNewReadNP(storageLocal(), fileName2))),
            true, "    saved files are equal");

        // Remove the file just written and recreate it from scratch
        //--------------------------------------------------------------------------------------------------------------------------
        storageRemoveP(storageLocalWrite(), fileName2, .errorOnMissing = true);

        info = NULL;
        TEST_ASSIGN(
            info, infoArchiveNew(PG_VERSION_94, 6569239123849665679, cipherTypeNone, NULL), "infoArchiveNew() - no sub cipher");
        TEST_RESULT_STR(strPtr(infoArchiveId(info)), "9.4-1", "    archiveId set");
        TEST_RESULT_PTR(infoArchivePg(info), info->infoPg, "    infoPg set");
        TEST_RESULT_PTR(infoArchiveCipherPass(info), NULL, "    no cipher sub");
        TEST_RESULT_INT(infoPgDataTotal(info->infoPg), 1, "    history set");

        TEST_RESULT_VOID(
            infoArchiveSave(info, storageLocalWrite(), fileName2, cipherTypeNone, NULL), "    save new");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetNP(storageNewReadNP(storageLocal(), fileName)),
                storageGetNP(storageNewReadNP(storageLocal(), fileName2))),
            true, "    saved files are equal");

        // Remove both files and recreate from scratch with cipher
        //--------------------------------------------------------------------------------------------------------------------------
        storageRemoveP(storageLocalWrite(), fileName, .errorOnMissing = true);
        storageRemoveP(storageLocalWrite(), fileName2, .errorOnMissing = true);

        content = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"10\"\n"
            "\n"
            "[cipher]\n"
            "cipher-pass=\"zWa/6Xtp-IVZC5444yXB+cgFDFl7MxGlgkZSaoPvTGirhPygu4jOKOXf9LO4vjfO\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"10\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), fileName), harnessInfoChecksum(content)),
                "put archive info to file with cipher sub");

        TEST_ASSIGN(
            info, infoArchiveNew(PG_VERSION_10, 6569239123849665999, cipherTypeAes256Cbc,
                strNew("zWa/6Xtp-IVZC5444yXB+cgFDFl7MxGlgkZSaoPvTGirhPygu4jOKOXf9LO4vjfO")),
            "infoArchiveNew() - cipher sub");
        TEST_RESULT_VOID(
            infoArchiveSave(info, storageLocalWrite(), fileName, cipherTypeAes256Cbc, cipherPass), "    save new encrypted");

        info = NULL;
        TEST_ASSIGN(info, infoArchiveNewLoad(storageLocal(), fileName, cipherTypeAes256Cbc, cipherPass),
            "    load encryperd archive info");
        TEST_RESULT_STR(strPtr(infoArchiveId(info)), "10-1", "    archiveId set");
        TEST_RESULT_PTR(infoArchivePg(info), info->infoPg, "    infoPg set");
        TEST_RESULT_STR(strPtr(infoArchiveCipherPass(info)),
            "zWa/6Xtp-IVZC5444yXB+cgFDFl7MxGlgkZSaoPvTGirhPygu4jOKOXf9LO4vjfO", "    cipher sub set");
        TEST_RESULT_INT(infoPgDataTotal(info->infoPg), 1, "    history set");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(info, infoArchiveNewInternal(), "infoArchiveNewInternal()");
        TEST_RESULT_PTR(infoArchivePg(info), NULL, "    infoPg not set");

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

        TEST_ASSIGN(info, infoArchiveNewLoad(storageLocal(), fileName, cipherTypeNone, NULL), "new archive info");
        TEST_RESULT_STR(strPtr(infoArchiveIdHistoryMatch(info, 2, 90500, 6626363367545678089)), "9.5-2", "  full match found");

        TEST_RESULT_STR(strPtr(infoArchiveIdHistoryMatch(info, 2, 90400, 6625592122879095702)), "9.4-1", "  partial match found");

        TEST_ERROR(infoArchiveIdHistoryMatch(info, 2, 90400, 6625592122879095799), ArchiveMismatchError,
            "unable to retrieve the archive id for database version '9.4' and system-id '6625592122879095799'");

        TEST_ERROR(infoArchiveIdHistoryMatch(info, 2, 90400, 6626363367545678089), ArchiveMismatchError,
            "unable to retrieve the archive id for database version '9.4' and system-id '6626363367545678089'");
    }
}
