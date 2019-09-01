/***********************************************************************************************************************************
Test Backup Info Handler
***********************************************************************************************************************************/
#include "command/backup/common.h"
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
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("InfoBackup"))
    {
        // File with section to ignore
        // -------------------------------------------------------------------------------------------------------------------------
        const Buffer *contentLoad = harnessInfoChecksumZ
        (
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[ignore-section]\n"
            "key1=value1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        // Load to make sure ignore-section is ignored
        InfoBackup *infoBackup;
        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(contentLoad)), "    new backup info");

        // Save to verify with new created info backup
        Buffer *contentSave = bufNew(0);

        TEST_RESULT_VOID(infoBackupSave(infoBackup, ioBufferWriteNew(contentSave)), "info backup save");

        // Create new info backup
        Buffer *contentCompare = bufNew(0);

        TEST_ASSIGN(
            infoBackup, infoBackupNew(PG_VERSION_94, 6569239123849665679, 942, 201409291, NULL),
            "infoBackupNew() - no cipher sub");
        TEST_RESULT_VOID(infoBackupSave(infoBackup, ioBufferWriteNew(contentCompare)), "    save backup info from new");
        TEST_RESULT_STR(strPtr(strNewBuf(contentCompare)), strPtr(strNewBuf(contentSave)), "   check save");

        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(contentCompare)), "load backup info");
        TEST_RESULT_PTR(infoBackupPg(infoBackup), infoBackup->infoPg, "    infoPg set");
        TEST_RESULT_PTR(infoBackupCipherPass(infoBackup), NULL, "    cipher sub not set");
        TEST_RESULT_INT(infoBackupDataTotal(infoBackup),  0, "    infoBackupDataTotal returns 0");

        // Check cipher pass
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(
                PG_VERSION_10, 6569239123849665999, 1002, 201707211,
                strNew("zWa/6Xtp-IVZC5444yXB+cgFDFl7MxGlgkZSaoPvTGirhPygu4jOKOXf9LO4vjfO")),
            "infoBackupNew() - cipher sub");

        contentSave = bufNew(0);

        TEST_RESULT_VOID(infoBackupSave(infoBackup, ioBufferWriteNew(contentSave)), "    save new with cipher sub");

        infoBackup = NULL;
        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(contentSave)), "    load backup info with cipher sub");
        TEST_RESULT_PTR(infoBackupPg(infoBackup), infoBackup->infoPg, "    infoPg set");
        TEST_RESULT_STR(strPtr(infoBackupCipherPass(infoBackup)),
            "zWa/6Xtp-IVZC5444yXB+cgFDFl7MxGlgkZSaoPvTGirhPygu4jOKOXf9LO4vjfO", "    cipher sub set");
        TEST_RESULT_INT(infoPgDataTotal(infoBackup->infoPg), 1, "    history set");

        // Add pg info
        // -------------------------------------------------------------------------------------------------------------------------
        InfoPgData infoPgData = {0};
        TEST_RESULT_VOID(infoBackupPgSet(infoBackup, PG_VERSION_94, 6569239123849665679, 12345, 54321), "add another infoPg");
        TEST_RESULT_INT(infoPgDataTotal(infoBackup->infoPg), 2, "    history incremented");
        TEST_ASSIGN(infoPgData, infoPgDataCurrent(infoBackup->infoPg), "    get current infoPgData");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(infoPgData.systemId, 6569239123849665679, "    systemId set");
        TEST_RESULT_INT(infoPgData.controlVersion, 12345, "    catalog set");
        TEST_RESULT_INT(infoPgData.catalogVersion, 54321, "    catalog set");

        // Free
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoBackupFree(infoBackup), "infoBackupFree() - free backup info");

        // backup:current section exists
        // -------------------------------------------------------------------------------------------------------------------------
        contentLoad = harnessInfoChecksumZ(
            "[backup:current]\n"
            "20161219-212741F={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"00000007000000000000001C\",\"backup-archive-stop\":\"00000007000000000000001C\","
            "\"backup-info-repo-size\":3159776,\"backup-info-repo-size-delta\":3159776,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":26897030,\"backup-timestamp-start\":1482182846,\"backup-timestamp-stop\":1482182861,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "20161219-212741F_20161219-212803D={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"00000008000000000000001E\",\"backup-archive-stop\":\"00000008000000000000001E\","
            "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":163866,\"backup-prior\":\"20161219-212741F\",\"backup-reference\":[\"20161219-212741F\"],"
            "\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,\"backup-type\":\"diff\",\"db-id\":1,"
            "\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20161219-212741F_20161219-212918I={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":null,\"backup-archive-stop\":null,"
            "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":163866,\"backup-prior\":\"20161219-212741F\",\"backup-reference\":[\"20161219-212741F\","
            "\"20161219-212741F_20161219-212803D\"],"
            "\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,\"backup-type\":\"incr\",\"db-id\":1,"
            "\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
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
                "\"db-version\":\"9.4\"}\n");

        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(contentLoad)), "    new backup info");

        TEST_RESULT_INT(infoBackupDataTotal(infoBackup), 3, "    backup list contains backups");

        InfoBackupData backupData = infoBackupData(infoBackup, 0);

        TEST_RESULT_STR(strPtr(backupData.backupLabel), "20161219-212741F", "full backup label");
        TEST_RESULT_STR(strPtr(backupData.backupType), "full", "    backup type full");
        TEST_RESULT_INT(backupData.backrestFormat, 5, "    backrest format");
        TEST_RESULT_STR(strPtr(backupData.backrestVersion), "2.04", "    backrest version");
        TEST_RESULT_STR(strPtr(backupData.backupArchiveStart), "00000007000000000000001C", "    archive start");
        TEST_RESULT_STR(strPtr(backupData.backupArchiveStop), "00000007000000000000001C", "    archive stop");
        TEST_RESULT_INT(backupData.backupInfoRepoSize, 3159776, "    repo size");
        TEST_RESULT_INT(backupData.backupInfoRepoSizeDelta, 3159776, "    repo delta");
        TEST_RESULT_INT(backupData.backupInfoSize, 26897030, "    backup size");
        TEST_RESULT_INT(backupData.backupInfoSizeDelta, 26897030, "    backup delta");
        TEST_RESULT_INT(backupData.backupPgId, 1, "    pg id");
        TEST_RESULT_PTR(backupData.backupPrior, NULL, "    backup prior NULL");
        TEST_RESULT_PTR(backupData.backupReference, NULL, "    backup reference NULL");
        TEST_RESULT_INT(backupData.backupTimestampStart, 1482182846, "    timestamp start");
        TEST_RESULT_INT(backupData.backupTimestampStop, 1482182861, "    timestamp stop");

        backupData = infoBackupData(infoBackup, 1);
        TEST_RESULT_STR(strPtr(backupData.backupLabel), "20161219-212741F_20161219-212803D", "diff backup label");
        TEST_RESULT_STR(strPtr(backupData.backupType), "diff", "    backup type diff");
        TEST_RESULT_INT(backupData.backupInfoRepoSize, 3159811, "    repo size");
        TEST_RESULT_INT(backupData.backupInfoRepoSizeDelta, 15765, "    repo delta");
        TEST_RESULT_INT(backupData.backupInfoSize, 26897030, "    backup size");
        TEST_RESULT_INT(backupData.backupInfoSizeDelta, 163866, "    backup delta");
        TEST_RESULT_STR(strPtr(backupData.backupPrior), "20161219-212741F", "    backup prior exists");
        TEST_RESULT_BOOL(
            (strLstSize(backupData.backupReference) == 1 && strLstExistsZ(backupData.backupReference, "20161219-212741F")), true,
            "    backup reference exists");

        backupData = infoBackupData(infoBackup, 2);
        TEST_RESULT_STR(strPtr(backupData.backupLabel), "20161219-212741F_20161219-212918I", "incr backup label");
        TEST_RESULT_PTR(backupData.backupArchiveStart, NULL, "    archive start NULL");
        TEST_RESULT_PTR(backupData.backupArchiveStop, NULL, "    archive stop NULL");
        TEST_RESULT_STR(strPtr(backupData.backupType), "incr", "    backup type incr");
        TEST_RESULT_STR(strPtr(backupData.backupPrior), "20161219-212741F", "    backup prior exists");
        TEST_RESULT_BOOL(
            (strLstSize(backupData.backupReference) == 2 && strLstExistsZ(backupData.backupReference, "20161219-212741F") &&
            strLstExistsZ(backupData.backupReference, "20161219-212741F_20161219-212803D")), true, "    backup reference exists");
        TEST_RESULT_BOOL(backupData.optionArchiveCheck, true, "    option archive check");
        TEST_RESULT_BOOL(backupData.optionArchiveCopy, false, "    option archive copy");
        TEST_RESULT_BOOL(backupData.optionBackupStandby, false, "    option backup standby");
        TEST_RESULT_BOOL(backupData.optionChecksumPage, false, "    option checksum page");
        TEST_RESULT_BOOL(backupData.optionCompress, true, "    option compress");
        TEST_RESULT_BOOL(backupData.optionHardlink, false, "    option hardlink");
        TEST_RESULT_BOOL(backupData.optionOnline, true, "    option online");

        // Save info and verify
        contentSave = bufNew(0);

        TEST_RESULT_VOID(infoBackupSave(infoBackup, ioBufferWriteNew(contentSave)), "info backup save");
        TEST_RESULT_STR(strPtr(strNewBuf(contentSave)), strPtr(strNewBuf(contentLoad)), "   check save");

        // infoBackupDataLabelList and infoBackupDataDelete
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderAsc), ", ")),
            "20161219-212741F, 20161219-212741F_20161219-212803D, 20161219-212741F_20161219-212918I",
            "infoBackupDataLabelList without expression");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(infoBackupDataLabelList(
                infoBackup, backupRegExpP(.full=true, .differential=true, .incremental=true)), sortOrderAsc), ", ")),
            "20161219-212741F, 20161219-212741F_20161219-212803D, 20161219-212741F_20161219-212918I",
            "infoBackupDataLabelList with expression");
        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelList(infoBackup, backupRegExpP(.full=true)), ", ")),
            "20161219-212741F", "  full=true");
        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelList(infoBackup, backupRegExpP(.differential=true)), ", ")),
            "20161219-212741F_20161219-212803D", "differential=true");
        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelList(infoBackup, backupRegExpP(.incremental=true)), ", ")),
            "20161219-212741F_20161219-212918I", "incremental=true");

        TEST_RESULT_VOID(infoBackupDataDelete(infoBackup, strNew("20161219-212741F_20161219-212918I")), "delete a backup");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderAsc), ", ")),
            "20161219-212741F, 20161219-212741F_20161219-212803D", "  backup deleted");

        TEST_RESULT_VOID(infoBackupDataDelete(infoBackup, strNew("20161219-212741F_20161219-212803D")), "delete all backups");
        TEST_RESULT_VOID(infoBackupDataDelete(infoBackup, strNew("20161219-212741F")), "  deleted");
        TEST_RESULT_UINT(strLstSize(infoBackupDataLabelList(infoBackup, NULL)), 0, "  no backups remain");

        // infoBackupDataToLog
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(
            strPtr(infoBackupDataToLog(&backupData)), "{label: 20161219-212741F_20161219-212918I, pgId: 1}", "check log format");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoBackupLoadFile() and infoBackupSaveFile()"))
    {
        TEST_ERROR_FMT(
            infoBackupLoadFile(storageTest, STRDEF(INFO_BACKUP_FILE), cipherTypeNone, NULL), FileMissingError,
            "unable to load info file '%s/backup.info' or '%s/backup.info.copy':\n"
            "FileMissingError: unable to open missing file '%s/backup.info' for read\n"
            "FileMissingError: unable to open missing file '%s/backup.info.copy' for read\n"
            "HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "HINT: has a stanza-create been performed?",
            testPath(), testPath(), testPath(), testPath());

        InfoBackup *infoBackup = infoBackupNew(PG_VERSION_10, 6569239123849665999, 1002, 201707211, NULL);
        TEST_RESULT_VOID(
            infoBackupSaveFile(infoBackup, storageTest, STRDEF(INFO_BACKUP_FILE), cipherTypeNone, NULL), "save backup info");

        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, STRDEF(INFO_BACKUP_FILE), cipherTypeNone, NULL), "load main");
        TEST_RESULT_UINT(infoPgDataCurrent(infoBackup->infoPg).systemId, 6569239123849665999, "    check file loaded");

        storageRemoveP(storageTest, STRDEF(INFO_BACKUP_FILE), .errorOnMissing = true);
        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, STRDEF(INFO_BACKUP_FILE), cipherTypeNone, NULL), "load copy");
        TEST_RESULT_UINT(infoPgDataCurrent(infoBackup->infoPg).systemId, 6569239123849665999, "    check file loaded");
    }
}
