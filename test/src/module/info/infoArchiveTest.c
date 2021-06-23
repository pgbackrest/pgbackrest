/***********************************************************************************************************************************
Test Archive Info Handler
***********************************************************************************************************************************/
#include <stdio.h>

#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "storage/posix/storage.h"

#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("InfoArchive"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move infoArchive to new memory context");

        const Buffer *contentLoad = harnessInfoChecksumZ
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
        );

        InfoArchive *info = NULL;

        // Load and test move function
        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(info, infoArchiveNewLoad(ioBufferReadNew(contentLoad)), "load new archive info");
            TEST_RESULT_VOID(infoArchiveMove(info, memContextPrior()), "move info");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_STR_Z(infoArchiveId(info), "9.4-1", "archiveId set");
        TEST_RESULT_PTR(infoArchivePg(info), info->pub.infoPg, "infoPg set");
        TEST_RESULT_STR(infoArchiveCipherPass(info), NULL, "no cipher sub");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("infoArchive save");

        // Save info and verify
        Buffer *contentSave = bufNew(0);

        TEST_RESULT_VOID(infoArchiveSave(info, ioBufferWriteNew(contentSave)), "info archive save");
        TEST_RESULT_STR(strNewBuf(contentSave), strNewBuf(contentLoad), "check save");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create content via new object");

        // Create the same content by creating a new object
        TEST_ASSIGN(
            info, infoArchiveNew(PG_VERSION_94, 6569239123849665679, NULL), "infoArchiveNew() - no sub cipher");
        TEST_RESULT_STR_Z(infoArchiveId(info), "9.4-1", "archiveId set");
        TEST_RESULT_PTR(infoArchivePg(info), info->pub.infoPg, "infoPg set");
        TEST_RESULT_STR(infoArchiveCipherPass(info), NULL, "no cipher sub");
        TEST_RESULT_INT(infoPgDataTotal(infoArchivePg(info)), 1, "history set");

        Buffer *contentCompare = bufNew(0);

        TEST_RESULT_VOID(infoArchiveSave(info, ioBufferWriteNew(contentCompare)), "info archive save");
        TEST_RESULT_STR(strNewBuf(contentCompare), strNewBuf(contentSave), "check save");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create with cipher");

        // Recreate from scratch with cipher
        TEST_ASSIGN(
            info,
            infoArchiveNew(
                PG_VERSION_10, 6569239123849665999, STRDEF("zWa/6Xtp-IVZC5444yXB+cgFDFl7MxGlgkZSaoPvTGirhPygu4jOKOXf9LO4vjfO")),
            "infoArchiveNew() - cipher sub");

        contentSave = bufNew(0);

        TEST_RESULT_VOID(infoArchiveSave(info, ioBufferWriteNew(contentSave)), "save new with cipher");

        TEST_ASSIGN(info, infoArchiveNewLoad(ioBufferReadNew(contentSave)), "load encrypted archive info");
        TEST_RESULT_STR_Z(infoArchiveId(info), "10-1", "archiveId set");
        TEST_RESULT_PTR(infoArchivePg(info), infoArchivePg(info), "infoPg set");
        TEST_RESULT_STR_Z(infoArchiveCipherPass(info),
            "zWa/6Xtp-IVZC5444yXB+cgFDFl7MxGlgkZSaoPvTGirhPygu4jOKOXf9LO4vjfO", "cipher sub set");
        TEST_RESULT_INT(infoPgDataTotal(infoArchivePg(info)), 1, "history set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("increment history");

        InfoPgData infoPgData = {0};
        TEST_RESULT_VOID(infoArchivePgSet(info, PG_VERSION_94, 6569239123849665679), "add another infoPg");
        TEST_RESULT_INT(infoPgDataTotal(infoArchivePg(info)), 2, "history incremented");
        TEST_ASSIGN(infoPgData, infoPgDataCurrent(infoArchivePg(info)), "get current infoPgData");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_94, "version set");
        TEST_RESULT_UINT(infoPgData.systemId, 6569239123849665679, "systemId set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("object free");

        TEST_RESULT_VOID(infoArchiveFree(info), "infoArchiveFree() - free archive info");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("infoArchiveIdHistoryMatch()");

        contentLoad = harnessInfoChecksumZ
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

        TEST_ASSIGN(info, infoArchiveNewLoad(ioBufferReadNew(contentLoad)), "new archive info");
        TEST_RESULT_STR_Z(infoArchiveIdHistoryMatch(info, 2, 90500, 6626363367545678089), "9.5-2", "  full match found");

        TEST_RESULT_STR_Z(infoArchiveIdHistoryMatch(info, 2, 90400, 6625592122879095702), "9.4-1", "  partial match found");

        TEST_ERROR(infoArchiveIdHistoryMatch(info, 2, 90400, 6625592122879095799), ArchiveMismatchError,
            "unable to retrieve the archive id for database version '9.4' and system-id '6625592122879095799'");

        TEST_ERROR(infoArchiveIdHistoryMatch(info, 2, 90400, 6626363367545678089), ArchiveMismatchError,
            "unable to retrieve the archive id for database version '9.4' and system-id '6626363367545678089'");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoArchiveLoadFile() and infoArchiveSaveFile()"))
    {
        TEST_ERROR(
            infoArchiveLoadFile(storageTest, STRDEF(INFO_ARCHIVE_FILE), cipherTypeNone, NULL), FileMissingError,
            "unable to load info file '" TEST_PATH "/archive.info' or '" TEST_PATH "/archive.info.copy':\n"
            "FileMissingError: unable to open missing file '" TEST_PATH "/archive.info' for read\n"
            "FileMissingError: unable to open missing file '" TEST_PATH "/archive.info.copy' for read\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.");

        InfoArchive *infoArchive = infoArchiveNew(PG_VERSION_10, 6569239123849665999, NULL);
        TEST_RESULT_VOID(
            infoArchiveSaveFile(infoArchive, storageTest, STRDEF(INFO_ARCHIVE_FILE), cipherTypeNone, NULL), "save archive info");

        TEST_ASSIGN(infoArchive, infoArchiveLoadFile(storageTest, STRDEF(INFO_ARCHIVE_FILE), cipherTypeNone, NULL), "load main");
        TEST_RESULT_UINT(infoPgDataCurrent(infoArchivePg(infoArchive)).systemId, 6569239123849665999, "check file loaded");

        HRN_STORAGE_REMOVE(storageTest, INFO_ARCHIVE_FILE, .errorOnMissing = true, .comment = "remove main so only copy exists");
        TEST_ASSIGN(infoArchive, infoArchiveLoadFile(storageTest, STRDEF(INFO_ARCHIVE_FILE), cipherTypeNone, NULL), "load copy");
        TEST_RESULT_UINT(infoPgDataCurrent(infoArchivePg(infoArchive)).systemId, 6569239123849665999, "check file loaded");
    }
}
