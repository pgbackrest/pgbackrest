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
            infoBackup, infoBackupNew(PG_VERSION_94, 6569239123849665679, NULL),
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
                PG_VERSION_10, 6569239123849665999, strNew("zWa/6Xtp-IVZC5444yXB+cgFDFl7MxGlgkZSaoPvTGirhPygu4jOKOXf9LO4vjfO")),
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
        TEST_RESULT_VOID(infoBackupPgSet(infoBackup, PG_VERSION_94, 6569239123849665679), "add another infoPg");
        TEST_RESULT_INT(infoPgDataTotal(infoBackup->infoPg), 2, "    history incremented");
        TEST_ASSIGN(infoPgData, infoPgDataCurrent(infoBackup->infoPg), "    get current infoPgData");
        TEST_RESULT_INT(infoPgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_INT(infoPgData.systemId, 6569239123849665679, "    systemId set");

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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("infoBackupDataAdd");

        Manifest *manifest = NULL;

        contentLoad = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-label=\"20190808-163540F\"\n"
            "backup-timestamp-copy-start=1565282141\n"
            "backup-timestamp-start=1565282140\n"
            "backup-timestamp-stop=1565282142\n"
            "backup-type=\"full\"\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=1000000000000000094\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[backup:option]\n"
            "option-archive-check=true\n"
            "option-archive-copy=true\n"
            "option-compress=false\n"
            "option-hardlink=false\n"
            "option-online=false\n"
            "\n"
            "[backup:target]\n"
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"
            "\n"
            "[cipher]\n"
            "cipher-pass=\"somepass\"\n"
            "\n"
            "[target:file]\n"
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"size\":4,\"timestamp\":1565282114}\n"
            "pg_data/postgresql.conf={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"repo-size\":24,\"size\":7,"
            "\"timestamp\":1565282214}\n"
            "\n"
            "[target:file:default]\n"
            "group=\"group1\"\n"
            "master=true\n"
            "mode=\"0600\"\n"
            "user=\"user1\"\n"
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            "\n"
            "[target:path:default]\n"
            "group=\"group1\"\n"
            "mode=\"0700\"\n"
            "user=\"user1\"\n"
        );

        TEST_ASSIGN(manifest, manifestNewLoad(ioBufferReadNew(contentLoad)), "load manifest");
        TEST_RESULT_VOID(infoBackupDataAdd(infoBackup, manifest), "add a backup");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 1, "backup added to current");
        TEST_ASSIGN(backupData, infoBackupData(infoBackup, 0), "get added backup");
        TEST_RESULT_STR(strPtr(backupData.backupLabel), "20190808-163540F", "backup label set");
        TEST_RESULT_UINT(backupData.backrestFormat, REPOSITORY_FORMAT, "backrest format");
        TEST_RESULT_STR(strPtr(backupData.backrestVersion), PROJECT_VERSION, "backuprest version");
        TEST_RESULT_PTR(backupData.backupArchiveStart, NULL, "archive start NULL");
        TEST_RESULT_PTR(backupData.backupArchiveStop, NULL, "archive stop NULL");
        TEST_RESULT_STR(strPtr(backupData.backupType), "full", "backup type set");
        TEST_RESULT_PTR(strPtr(backupData.backupPrior), NULL, "no backup prior");
        TEST_RESULT_UINT(strLstSize(backupData.backupReference), 0, "no backup reference");
        TEST_RESULT_BOOL(backupData.optionArchiveCheck, true, "option archive check");
        TEST_RESULT_BOOL(backupData.optionArchiveCopy, true, " option archive copy");
        TEST_RESULT_BOOL(backupData.optionBackupStandby, false, "no option backup standby");
        TEST_RESULT_BOOL(backupData.optionChecksumPage, false, "no option checksum page");
        TEST_RESULT_BOOL(backupData.optionCompress, false, "option compress");
        TEST_RESULT_BOOL(backupData.optionHardlink, false, "option hardlink");
        TEST_RESULT_BOOL(backupData.optionOnline, false, "option online");
        TEST_RESULT_UINT(backupData.backupInfoSize, 11, "database size");
        TEST_RESULT_UINT(backupData.backupInfoSizeDelta, 11, "backup size");
        TEST_RESULT_UINT(backupData.backupInfoRepoSize, 28, "repo size");
        TEST_RESULT_UINT(backupData.backupInfoRepoSizeDelta, 28, "repo backup size");

        // infoBackupDataToLog
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("infoBackupDataToLog");

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

        InfoBackup *infoBackup = infoBackupNew(PG_VERSION_10, 6569239123849665999, NULL);
        TEST_RESULT_VOID(
            infoBackupSaveFile(infoBackup, storageTest, STRDEF(INFO_BACKUP_FILE), cipherTypeNone, NULL), "save backup info");

        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, STRDEF(INFO_BACKUP_FILE), cipherTypeNone, NULL), "load main");
        TEST_RESULT_UINT(infoPgDataCurrent(infoBackup->infoPg).systemId, 6569239123849665999, "    check file loaded");

        storageRemoveP(storageTest, STRDEF(INFO_BACKUP_FILE), .errorOnMissing = true);
        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, STRDEF(INFO_BACKUP_FILE), cipherTypeNone, NULL), "load copy");
        TEST_RESULT_UINT(infoPgDataCurrent(infoBackup->infoPg).systemId, 6569239123849665999, "    check file loaded");
    }
}
