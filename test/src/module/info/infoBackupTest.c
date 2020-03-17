/***********************************************************************************************************************************
Test Backup Info Handler
***********************************************************************************************************************************/
#include "command/backup/common.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
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
        TEST_RESULT_STR(strNewBuf(contentCompare), strNewBuf(contentSave), "   check save");

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
        TEST_RESULT_STR_Z(infoBackupCipherPass(infoBackup),
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

        TEST_RESULT_STR_Z(backupData.backupLabel, "20161219-212741F", "full backup label");
        TEST_RESULT_STR_Z(backupData.backupType, "full", "    backup type full");
        TEST_RESULT_INT(backupData.backrestFormat, 5, "    backrest format");
        TEST_RESULT_STR_Z(backupData.backrestVersion, "2.04", "    backrest version");
        TEST_RESULT_STR_Z(backupData.backupArchiveStart, "00000007000000000000001C", "    archive start");
        TEST_RESULT_STR_Z(backupData.backupArchiveStop, "00000007000000000000001C", "    archive stop");
        TEST_RESULT_INT(backupData.backupInfoRepoSize, 3159776, "    repo size");
        TEST_RESULT_INT(backupData.backupInfoRepoSizeDelta, 3159776, "    repo delta");
        TEST_RESULT_INT(backupData.backupInfoSize, 26897030, "    backup size");
        TEST_RESULT_INT(backupData.backupInfoSizeDelta, 26897030, "    backup delta");
        TEST_RESULT_INT(backupData.backupPgId, 1, "    pg id");
        TEST_RESULT_PTR(backupData.backupPrior, NULL, "    backup prior NULL");
        TEST_RESULT_PTR(backupData.backupReference, NULL, "    backup reference NULL");
        TEST_RESULT_INT(backupData.backupTimestampStart, 1482182846, "    timestamp start");
        TEST_RESULT_INT(backupData.backupTimestampStop, 1482182861, "    timestamp stop");

        InfoBackupData *backupDataPtr = infoBackupDataByLabel(infoBackup, STRDEF("20161219-212741F_20161219-212803D"));
        TEST_RESULT_STR_Z(backupDataPtr->backupLabel, "20161219-212741F_20161219-212803D", "diff backup label");
        TEST_RESULT_STR_Z(backupDataPtr->backupType, "diff", "    backup type diff");
        TEST_RESULT_INT(backupDataPtr->backupInfoRepoSize, 3159811, "    repo size");
        TEST_RESULT_INT(backupDataPtr->backupInfoRepoSizeDelta, 15765, "    repo delta");
        TEST_RESULT_INT(backupDataPtr->backupInfoSize, 26897030, "    backup size");
        TEST_RESULT_INT(backupDataPtr->backupInfoSizeDelta, 163866, "    backup delta");
        TEST_RESULT_STR_Z(backupDataPtr->backupPrior, "20161219-212741F", "    backup prior exists");
        TEST_RESULT_BOOL(
            (strLstSize(backupDataPtr->backupReference) == 1 && strLstExistsZ(backupDataPtr->backupReference, "20161219-212741F")), true,
            "    backup reference exists");
        TEST_RESULT_PTR(infoBackupDataByLabel(infoBackup, STRDEF("20161219-12345")), NULL, "    backup label does not exist");

        backupData = infoBackupData(infoBackup, 2);
        TEST_RESULT_STR_Z(backupData.backupLabel, "20161219-212741F_20161219-212918I", "incr backup label");
        TEST_RESULT_PTR(backupData.backupArchiveStart, NULL, "    archive start NULL");
        TEST_RESULT_PTR(backupData.backupArchiveStop, NULL, "    archive stop NULL");
        TEST_RESULT_STR_Z(backupData.backupType, "incr", "    backup type incr");
        TEST_RESULT_STR_Z(backupData.backupPrior, "20161219-212741F", "    backup prior exists");
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
        TEST_RESULT_STR(strNewBuf(contentSave), strNewBuf(contentLoad), "   check save");

        // infoBackupDataLabelList and infoBackupDataDelete
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderAsc), ", "),
            "20161219-212741F, 20161219-212741F_20161219-212803D, 20161219-212741F_20161219-212918I",
            "infoBackupDataLabelList without expression");
        TEST_RESULT_STR_Z(
            strLstJoin(
                strLstSort(
                    infoBackupDataLabelList(
                        infoBackup, backupRegExpP(.full = true, .differential = true, .incremental = true)), sortOrderAsc), ", "),
            "20161219-212741F, 20161219-212741F_20161219-212803D, 20161219-212741F_20161219-212918I",
            "infoBackupDataLabelList with expression");
        TEST_RESULT_STR_Z(
            strLstJoin(infoBackupDataLabelList(infoBackup, backupRegExpP(.full=true)), ", "), "20161219-212741F", "  full=true");
        TEST_RESULT_STR_Z(
            strLstJoin(infoBackupDataLabelList(infoBackup, backupRegExpP(.differential=true)), ", "),
            "20161219-212741F_20161219-212803D", "differential=true");
        TEST_RESULT_STR_Z(
            strLstJoin(infoBackupDataLabelList(infoBackup, backupRegExpP(.incremental=true)), ", "),
            "20161219-212741F_20161219-212918I", "incremental=true");

        TEST_RESULT_VOID(infoBackupDataDelete(infoBackup, strNew("20161219-212741F_20161219-212918I")), "delete a backup");
        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderAsc), ", "),
            "20161219-212741F, 20161219-212741F_20161219-212803D", "  backup deleted");

        TEST_RESULT_VOID(infoBackupDataDelete(infoBackup, strNew("20161219-212741F_20161219-212803D")), "delete all backups");
        TEST_RESULT_VOID(infoBackupDataDelete(infoBackup, strNew("20161219-212741F")), "  deleted");
        TEST_RESULT_UINT(strLstSize(infoBackupDataLabelList(infoBackup, NULL)), 0, "  no backups remain");

        // infoBackupDataToLog
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(
            infoBackupDataToLog(&backupData), "{label: 20161219-212741F_20161219-212918I, pgId: 1}", "check log format");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("infoBackupDataAdd - full backup");

        #define TEST_MANIFEST_BACKUPDB                                                                                             \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=201409291\n"                                                                                       \
            "db-control-version=942\n"                                                                                             \
            "db-id=1\n"                                                                                                            \
            "db-system-id=6569239123849665679\n"                                                                                   \
            "db-version=\"9.4\"\n"

        #define TEST_MANIFEST_FILE_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:file:default]\n"                                                                                              \
            "group=\"group1\"\n"                                                                                                   \
            "master=false\n"                                                                                                       \
            "mode=\"0600\"\n"                                                                                                      \
            "user=\"user1\"\n"

        #define TEST_MANIFEST_LINK_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:link:default]\n"                                                                                              \
            "group=\"group1\"\n"                                                                                                   \
            "user=false\n"

        #define TEST_MANIFEST_PATH_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:path:default]\n"                                                                                              \
            "group=false\n"                                                                                                        \
            "mode=\"0700\"\n"                                                                                                      \
            "user=\"user1\"\n"

        Manifest *manifest = NULL;

        const Buffer *manifestContent = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-label=\"20190818-084502F\"\n"
            "backup-timestamp-copy-start=1565282141\n"
            "backup-timestamp-start=1565282140\n"
            "backup-timestamp-stop=1565282142\n"
            "backup-type=\"full\"\n"
            TEST_MANIFEST_BACKUPDB
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
            TEST_MANIFEST_FILE_DEFAULT
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_ASSIGN(manifest, manifestNewLoad(ioBufferReadNew(manifestContent)), "load manifest");
        TEST_RESULT_VOID(infoBackupDataAdd(infoBackup, manifest), "add a backup");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 1, "backup added to current");
        TEST_ASSIGN(backupData, infoBackupData(infoBackup, 0), "get added backup");
        TEST_RESULT_STR_Z(backupData.backupLabel, "20190818-084502F", "backup label set");
        TEST_RESULT_UINT(backupData.backrestFormat, REPOSITORY_FORMAT, "backrest format");
        TEST_RESULT_STR_Z(backupData.backrestVersion, PROJECT_VERSION, "backuprest version");
        TEST_RESULT_INT(backupData.backupPgId, 1, "pg id");
        TEST_RESULT_PTR(backupData.backupArchiveStart, NULL, "archive start NULL");
        TEST_RESULT_PTR(backupData.backupArchiveStop, NULL, "archive stop NULL");
        TEST_RESULT_STR_Z(backupData.backupType, "full", "backup type set");
        TEST_RESULT_PTR(strPtr(backupData.backupPrior), NULL, "no backup prior");
        TEST_RESULT_PTR(backupData.backupReference, NULL, "no backup reference");
        TEST_RESULT_INT(backupData.backupTimestampStart, 1565282140, "timestamp start");
        TEST_RESULT_INT(backupData.backupTimestampStop, 1565282142, "timestamp stop");
        TEST_RESULT_BOOL(backupData.optionArchiveCheck, true, "option archive check");
        TEST_RESULT_BOOL(backupData.optionArchiveCopy, true, "option archive copy");
        TEST_RESULT_BOOL(backupData.optionBackupStandby, false, "no option backup standby");
        TEST_RESULT_BOOL(backupData.optionChecksumPage, false, "no option checksum page");
        TEST_RESULT_BOOL(backupData.optionCompress, false, "option compress");
        TEST_RESULT_BOOL(backupData.optionHardlink, false, "option hardlink");
        TEST_RESULT_BOOL(backupData.optionOnline, false, "option online");
        TEST_RESULT_UINT(backupData.backupInfoSize, 11, "database size");
        TEST_RESULT_UINT(backupData.backupInfoSizeDelta, 11, "backup size");
        TEST_RESULT_UINT(backupData.backupInfoRepoSize, 28, "repo size");
        TEST_RESULT_UINT(backupData.backupInfoRepoSizeDelta, 28, "repo backup size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("infoBackupDataAdd - incr backup");

        #define TEST_MANIFEST_DB                                                                                                   \
            "\n"                                                                                                                   \
            "[db]\n"                                                                                                               \
            "mail={\"db-id\":16456,\"db-last-system-id\":12168}\n"                                                                 \
            "postgres={\"db-id\":12173,\"db-last-system-id\":12168}\n"                                                             \
            "template0={\"db-id\":12168,\"db-last-system-id\":12168}\n"                                                            \
            "template1={\"db-id\":1,\"db-last-system-id\":12168}\n"                                                                \

        manifestContent = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-archive-start=\"000000030000028500000089\"\n"
            "backup-archive-stop=\"000000030000028500000090\"\n"
            "backup-label=\"20190818-084502F_20190820-084502I\"\n"
            "backup-lsn-start=\"285/89000028\"\n"
            "backup-lsn-stop=\"285/89001F88\"\n"
            "backup-prior=\"20190818-084502F\"\n"
            "backup-timestamp-copy-start=1565282141\n"
            "backup-timestamp-start=1565282140\n"
            "backup-timestamp-stop=1565282142\n"
            "backup-type=\"diff\"\n"
            TEST_MANIFEST_BACKUPDB
            "\n"
            "[backup:option]\n"
            "option-archive-check=true\n"
            "option-archive-copy=true\n"
            "option-backup-standby=true\n"
            "option-buffer-size=16384\n"
            "option-checksum-page=true\n"
            "option-compress=true\n"
            "option-compress-level=3\n"
            "option-compress-level-network=3\n"
            "option-delta=false\n"
            "option-hardlink=true\n"
            "option-online=true\n"
            "option-process-max=32\n"
            "\n"
            "[backup:target]\n"
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"
            "pg_data/base/1={\"path\":\"../../base-1\",\"type\":\"link\"}\n"
            "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../pg_config\",\"type\":\"link\"}\n"
            "pg_data/pg_stat={\"path\":\"../pg_stat\",\"type\":\"link\"}\n"
            "pg_data/postgresql.conf={\"file\":\"postgresql.conf\",\"path\":\"../pg_config\",\"type\":\"link\"}\n"
            "pg_tblspc/1={\"path\":\"/tblspc/ts1\",\"tablespace-id\":\"1\",\"tablespace-name\":\"ts1\",\"type\":\"link\"}\n"
            TEST_MANIFEST_DB
            "\n"
            "[target:file]\n"
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true"
                ",\"reference\":\"20190818-084502F_20190819-084506D\",\"size\":4,\"timestamp\":1565282114}\n"
            "pg_data/base/16384/17000={\"checksum\":\"e0101dd8ffb910c9c202ca35b5f828bcb9697bed\",\"checksum-page\":false"
                ",\"checksum-page-error\":[1],\"repo-size\":4096,\"size\":8192,\"timestamp\":1565282114}\n"
            "pg_data/base/16384/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"group\":false,\"size\":4"
                ",\"timestamp\":1565282115}\n"
            "pg_data/base/32768/33000={\"checksum\":\"7a16d165e4775f7c92e8cdf60c0af57313f0bf90\",\"checksum-page\":true"
                ",\"reference\":\"20190818-084502F\",\"size\":1073741824,\"timestamp\":1565282116}\n"
            "pg_data/base/32768/33000.32767={\"checksum\":\"6e99b589e550e68e934fd235ccba59fe5b592a9e\",\"checksum-page\":true"
                ",\"reference\":\"20190818-084502F_20190819-084506I\",\"size\":32768,\"timestamp\":1565282114}\n"
            "pg_data/postgresql.conf={\"checksum\":\"6721d92c9fcdf4248acff1f9a1377127d9064807\",\"master\":true,\"size\":4457"
                ",\"timestamp\":1565282114}\n"
            "pg_data/special={\"master\":true,\"mode\":\"0640\",\"size\":0,\"timestamp\":1565282120,\"user\":false}\n"
            "pg_data/dupref={\"master\":true,\"mode\":\"0640\",\"reference\":\"20190818-084502F\",\"size\":0"
                ",\"timestamp\":1565282120,\"user\":false}\n"
            TEST_MANIFEST_FILE_DEFAULT
            "\n"
            "[target:link]\n"
            "pg_data/pg_stat={\"destination\":\"../pg_stat\"}\n"
            "pg_data/postgresql.conf={\"destination\":\"../pg_config/postgresql.conf\",\"group\":false,\"user\":\"user1\"}\n"
            TEST_MANIFEST_LINK_DEFAULT
            "\n"
            "[target:path]\n"
            "pg_data={\"user\":\"user2\"}\n"
            "pg_data/base={\"group\":\"group2\"}\n"
            "pg_data/base/16384={\"mode\":\"0750\"}\n"
            "pg_data/base/32768={}\n"
            "pg_data/base/65536={\"user\":false}\n"
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_ASSIGN(manifest, manifestNewLoad(ioBufferReadNew(manifestContent)), "load manifest");
        TEST_RESULT_VOID(infoBackupDataAdd(infoBackup, manifest), "add a backup");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "backup added to current");
        TEST_ASSIGN(backupData, infoBackupData(infoBackup, 1), "get added backup");
        TEST_RESULT_STR_Z(backupData.backupLabel, "20190818-084502F_20190820-084502I", "backup label set");
        TEST_RESULT_UINT(backupData.backrestFormat, REPOSITORY_FORMAT, "backrest format");
        TEST_RESULT_STR_Z(backupData.backrestVersion, PROJECT_VERSION, "backuprest version");
        TEST_RESULT_STR_Z(backupData.backupArchiveStart, "000000030000028500000089", "archive start set");
        TEST_RESULT_STR_Z(backupData.backupArchiveStop, "000000030000028500000090", "archive stop set");
        TEST_RESULT_STR_Z(backupData.backupType, "diff", "backup type set");
        TEST_RESULT_STR_Z(backupData.backupPrior, "20190818-084502F", "backup prior set");
        TEST_RESULT_STR_Z(
            strLstJoin(backupData.backupReference, ", "),
            "20190818-084502F, 20190818-084502F_20190819-084506D, 20190818-084502F_20190819-084506I",
            "backup reference set and ordered");
        TEST_RESULT_BOOL(backupData.optionArchiveCheck, true, "option archive check");
        TEST_RESULT_BOOL(backupData.optionArchiveCopy, true, "option archive copy");
        TEST_RESULT_BOOL(backupData.optionBackupStandby, true, "option backup standby");
        TEST_RESULT_BOOL(backupData.optionChecksumPage, true, "no option checksum page");
        TEST_RESULT_BOOL(backupData.optionCompress, true, "option compress");
        TEST_RESULT_BOOL(backupData.optionHardlink, true, "option hardlink");
        TEST_RESULT_BOOL(backupData.optionOnline, true, "option online");
        TEST_RESULT_UINT(backupData.backupInfoSize, 1073787249, "database size");
        TEST_RESULT_UINT(backupData.backupInfoSizeDelta, 12653, "backup size");
        TEST_RESULT_UINT(backupData.backupInfoRepoSize, 1073783153, "repo size");
        TEST_RESULT_UINT(backupData.backupInfoRepoSizeDelta, 8557, "repo backup size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("infoBackupLoadFileReconstruct - skip/add backups");

        // Load configuration to set repo-path and stanza
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAdd(argList, strNewFmt("--repo-path=%s", testPath()));
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        // Create manifest for upgrade db (id=2), save to disk
        manifestContent = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-archive-start=\"000000030000028500000066\"\n"
            "backup-archive-stop=\"000000030000028500000070\"\n"
            "backup-label=\"20190923-164324F\"\n"
            "backup-lsn-start=\"0/66000028\"\n"
            "backup-lsn-stop=\"0/700000F8\"\n"
            "backup-timestamp-copy-start=1569257007\n"
            "backup-timestamp-start=1569257004\n"
            "backup-timestamp-stop=1569257014\n"
            "backup-type=\"full\"\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201809051\n"
            "db-control-version=1100\n"
            "db-id=2\n"
            "db-system-id=6739907367085689196\n"
            "db-version=\"11\"\n"
            "\n"
            "[backup:option]\n"
            "option-archive-check=true\n"
            "option-archive-copy=false\n"
            "option-backup-standby=false\n"
            "option-buffer-size=4194304\n"
            "option-checksum-page=false\n"
            "option-compress=true\n"
            "option-compress-level=6\n"
            "option-compress-level-network=3\n"
            "option-delta=false\n"
            "option-hardlink=false\n"
            "option-online=true\n"
            "option-process-max=3\n"
            "\n"
            "[backup:target]\n"
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"
            TEST_MANIFEST_DB
            "\n"
            "[target:file]\n"
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true"
                ",\"reference\":\"20190818-084502F_20190819-084506D\",\"size\":4,\"timestamp\":1569256970}\n"
            "pg_data/backup_label={\"checksum\":\"e0101dd8ffb910c9c202ca35b5f828bcb9697bed\",\"checksum-page\":false"
                ",\"checksum-page-error\":[1],\"size\":249,\"timestamp\":1569257013}\n"
            "pg_data/base/13050/PG_VERSION={\"checksum\":\"dd71038f3463f511ee7403dbcbc87195302d891c\",\"repo-size\":23,\"size\":3,\"timestamp\":1569256971}\n"
            TEST_MANIFEST_FILE_DEFAULT
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            "pg_data/base={}\n"
            "pg_data/base/13050={}\n"
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_VOID(
           storagePutP(storageNewWriteP(storageRepoWrite(),
           strNew(STORAGE_REPO_BACKUP "/20190923-164324F/" BACKUP_MANIFEST_FILE)), manifestContent),
           "write main manifest for pgId=2 - valid backup to add");

        manifestContent = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-label=\"20190818-084444F\"\n"
            "backup-timestamp-copy-start=1565282141\n"
            "backup-timestamp-start=1565282140\n"
            "backup-timestamp-stop=1565282142\n"
            "backup-type=\"full\"\n"
            TEST_MANIFEST_BACKUPDB
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
            TEST_MANIFEST_FILE_DEFAULT
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageRepoWrite(),
            strNew(STORAGE_REPO_BACKUP "/20190818-084444F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT)),
            manifestContent), "write manifest copy for pgId=1");

        manifestContent = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-label=\"20190818-084555F\"\n"
            "backup-timestamp-copy-start=1565282141\n"
            "backup-timestamp-start=1565282140\n"
            "backup-timestamp-stop=1565282142\n"
            "backup-type=\"full\"\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201809051\n"
            "db-control-version=1100\n"
            "db-id=1\n"
            "db-system-id=6739907367085689196\n"
            "db-version=\"11\"\n"
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
            TEST_MANIFEST_FILE_DEFAULT
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageRepoWrite(),
            strNew(STORAGE_REPO_BACKUP "/20190818-084555F/" BACKUP_MANIFEST_FILE)),
            manifestContent), "write manifest - invalid backup pgId mismatch");

        manifestContent = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-label=\"20190818-084666F\"\n"
            "backup-timestamp-copy-start=1565282141\n"
            "backup-timestamp-start=1565282140\n"
            "backup-timestamp-stop=1565282142\n"
            "backup-type=\"full\"\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201809051\n"
            "db-control-version=1100\n"
            "db-id=2\n"
            "db-system-id=6739907367085689666\n"
            "db-version=\"11\"\n"
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
            TEST_MANIFEST_FILE_DEFAULT
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageRepoWrite(),
            strNew(STORAGE_REPO_BACKUP "/20190818-084666F/" BACKUP_MANIFEST_FILE)),
            manifestContent), "write manifest - invalid backup system-id mismatch");

        manifestContent = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-label=\"20190818-084777F\"\n"
            "backup-timestamp-copy-start=1565282141\n"
            "backup-timestamp-start=1565282140\n"
            "backup-timestamp-stop=1565282142\n"
            "backup-type=\"full\"\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201809051\n"
            "db-control-version=1100\n"
            "db-id=2\n"
            "db-system-id=6739907367085689196\n"
            "db-version=\"10\"\n"
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
            TEST_MANIFEST_FILE_DEFAULT
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageRepoWrite(),
            strNew(STORAGE_REPO_BACKUP "/20190818-084777F/" BACKUP_MANIFEST_FILE)),
            manifestContent), "write manifest - invalid backup version mismatch");

        TEST_RESULT_VOID(
            storagePathCreateP(storageRepoWrite(), strNew(STORAGE_REPO_BACKUP "/20190818-084502F")),
            "create backup on disk that is in current but no manifest");

        TEST_RESULT_STR_Z(
            strLstJoin(
                strLstSort(
                    storageListP(
                        storageRepo(), STORAGE_REPO_BACKUP_STR,
                        .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
                    sortOrderAsc),
                ", "),
            "20190818-084444F, 20190818-084502F, 20190818-084555F, 20190818-084666F, 20190818-084777F, 20190923-164324F",
            "confirm backups on disk");

        // With the infoBackup from above, upgrade the DB so there a 2 histories then save to disk
        TEST_ASSIGN(infoBackup, infoBackupPgSet(infoBackup, PG_VERSION_11, 6739907367085689196), "upgrade db");
        TEST_RESULT_VOID(
            infoBackupSaveFile(infoBackup, storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL),
            "save backup info");
        infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL),
            "get saved backup info");
        TEST_RESULT_INT(infoBackupDataTotal(infoBackup), 2, "backup list contains backups to be removed");
        TEST_RESULT_INT(infoPgDataTotal(infoBackup->infoPg), 2, "multiple DB history");

        TEST_ASSIGN(
            infoBackup, infoBackupLoadFileReconstruct(storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL),
            "reconstruct");
        TEST_RESULT_INT(infoBackupDataTotal(infoBackup), 1, "backup list contains 1 backup");
        TEST_ASSIGN(backupData, infoBackupData(infoBackup, 0), "get the backup");
        TEST_RESULT_STR_Z(
            backupData.backupLabel, "20190923-164324F",
            "backups not on disk removed, valid backup on disk added, manifest copy-only ignored");
        harnessLogResult(
            "P00   WARN: invalid backup '20190818-084555F' cannot be added to current backups\n"
            "P00   WARN: invalid backup '20190818-084666F' cannot be added to current backups\n"
            "P00   WARN: invalid backup '20190818-084777F' cannot be added to current backups\n"
            "P00   WARN: backup '20190923-164324F' found in repository added to backup.info\n"
            "P00   WARN: backup '20190818-084502F' missing manifest removed from backup.info\n"
            "P00   WARN: backup '20190818-084502F_20190820-084502I' missing manifest removed from backup.info");

        TEST_RESULT_VOID(
            storageCopyP(
                storageNewReadP(storageRepo(), strNew(STORAGE_REPO_BACKUP "/20190818-084444F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT)),
                storageNewWriteP(storageRepoWrite(), strNew(STORAGE_REPO_BACKUP "/20190818-084444F/" BACKUP_MANIFEST_FILE))),
                "write manifest from copy-only for pgId=1");

        TEST_RESULT_VOID(
            infoBackupSaveFile(infoBackup, storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL),
            "save updated backup info");
        infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup, infoBackupLoadFileReconstruct(storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL),
            "reconstruct");
        TEST_RESULT_STR_Z(
            strLstJoin(infoBackupDataLabelList(infoBackup, NULL), ", "), "20190818-084444F, 20190923-164324F",
            "previously ignored pgId=1 manifest copy-only now added before existing");
        harnessLogResult(
            "P00   WARN: backup '20190818-084444F' found in repository added to backup.info\n"
            "P00   WARN: invalid backup '20190818-084555F' cannot be added to current backups\n"
            "P00   WARN: invalid backup '20190818-084666F' cannot be added to current backups\n"
            "P00   WARN: invalid backup '20190818-084777F' cannot be added to current backups");
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
