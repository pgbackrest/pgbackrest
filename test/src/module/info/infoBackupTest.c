/***********************************************************************************************************************************
Test Backup Info Handler
***********************************************************************************************************************************/
#include "storage/storage.intern.h"

#include "common/harnessInfo.h"
#include "command/backup/common.h"

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
    InfoBackup *infoBackup = NULL;

    // *****************************************************************************************************************************
    if (testBegin("infoBackupNewLoad(), infoBackupDataTotal(), infoBackupCheckPg(), infoBackupFree()"))
    {
        // File missing
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            infoBackupNewLoad(storageLocal(), fileName, cipherTypeNone, NULL), FileMissingError,
            "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "HINT: has a stanza-create been performed?",
            testPath(), testPath(),  strPtr(strNewFmt("%s/test.ini", testPath())),
            strPtr(strNewFmt("%s/test.ini.copy", testPath())));

        // File exists, no backup:current section
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), fileName), harnessInfoChecksum(content)), "put backup info to file");

        TEST_ASSIGN(infoBackup, infoBackupNewLoad(storageLocal(), fileName, cipherTypeNone, NULL), "    new backup info");
        TEST_RESULT_PTR(infoBackupPg(infoBackup), infoBackup->infoPg, "    infoPg set");
        TEST_RESULT_PTR(infoBackup->backup, NULL, "    backupCurrent NULL");
        TEST_RESULT_INT(infoBackupDataTotal(infoBackup),  0, "    infoBackupDataTotal returns 0");

        // infoBackupCheckPg
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(infoBackupCheckPg(infoBackup, 90400, 6569239123849665679, 201409291, 942), 1, "check PG data");

        TEST_ERROR_FMT(
            infoBackupCheckPg(infoBackup, 90500, 6569239123849665679, 201409291, 942), BackupMismatchError,
            "database version = 9.5, system-id 6569239123849665679 does not match "
            "backup version = 9.4, system-id = 6569239123849665679\n"
            "HINT: is this the correct stanza?");

        TEST_ERROR_FMT(
            infoBackupCheckPg(infoBackup, 90400, 6569239123849665999, 201409291, 942), BackupMismatchError,
            "database version = 9.4, system-id 6569239123849665999 does not match "
            "backup version = 9.4, system-id = 6569239123849665679\n"
            "HINT: is this the correct stanza?");

        TEST_ERROR_FMT(
            infoBackupCheckPg(infoBackup, 90400, 6569239123849665679, 201409291, 941), BackupMismatchError,
            "database control-version = 941, catalog-version 201409291"
            " does not match backup control-version = 942, catalog-version = 201409291\n"
            "HINT: this may be a symptom of database or repository corruption!");

        TEST_ERROR_FMT(
            infoBackupCheckPg(infoBackup, 90400, 6569239123849665679, 201509291, 942), BackupMismatchError,
            "database control-version = 942, catalog-version 201509291"
            " does not match backup control-version = 942, catalog-version = 201409291\n"
            "HINT: this may be a symptom of database or repository corruption!");

        // Free
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoBackupFree(infoBackup), "infoBackupFree() - free backup info");
    }
    // *****************************************************************************************************************************
    if (testBegin(
        "infoBackupData(), infoBackupDataTotal(), infoBackupDataToLog(), infoBackupDataLabelList(), infoBackupDataDelete()"))
    {
        // File exists, backup:current section exists
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
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
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), fileName), harnessInfoChecksum(content)), "put backup info current to file");
        TEST_ASSIGN(infoBackup, infoBackupNewLoad(storageLocal(), fileName, cipherTypeNone, NULL), "    new backup info");

        // Save the file and verify it
        TEST_RESULT_VOID(infoBackupSave(infoBackup, storageLocalWrite(), fileName2, cipherTypeNone, NULL), "save file");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetNP(storageNewReadNP(storageLocal(), fileName)),
                storageGetNP(storageNewReadNP(storageLocal(), fileName2))),
            true, "files are equal");

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

        // infoBackupDataLabelList and infoBackupDataDelete
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderAsc), ", ")),
            "20161219-212741F, 20161219-212741F_20161219-212803D, 20161219-212741F_20161219-212918I", "infoBackupDataLabelList without expression");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(infoBackupDataLabelList(
                infoBackup, backupRegExpP(.full=true, .differential=true, .incremental=true)), sortOrderAsc), ", ")),
            "20161219-212741F, 20161219-212741F_20161219-212803D, 20161219-212741F_20161219-212918I", "infoBackupDataLabelList with expression");
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
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(
            strPtr(infoBackupDataToLog(&backupData)), "{label: 20161219-212741F_20161219-212918I, pgId: 1}", "check log format");
    }
}
