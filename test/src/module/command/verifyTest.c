/***********************************************************************************************************************************
Test Stanza Commands
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPostgres.h"
#include "common/harnessPq.h"

#include "common/harnessProtocol.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Install local command handler shim
    static const ProtocolServerHandler testLocalHandlerList[] = {PROTOCOL_SERVER_HANDLER_VERIFY_LIST};
    hrnProtocolLocalShimInstall(testLocalHandlerList, PROTOCOL_SERVER_HANDLER_LIST_SIZE(testLocalHandlerList));

    StringList *argListBase = strLstNew();
    hrnCfgArgRawZ(argListBase, cfgOptStanza, "db");
    hrnCfgArgRawZ(argListBase, cfgOptRepoPath, TEST_PATH "/repo");

    const char *fileContents = "acefile";
    uint64_t fileSize = 7;
    const String *fileChecksum = STRDEF("d1cd8a7d11daa26814b93eb604e1d49ab4b43770");

    #define TEST_BACKUP_DB1_94                                                                                                     \
        "db-catalog-version=201409291\n"                                                                                           \
        "db-control-version=942\n"                                                                                                 \
        "db-id=1\n"                                                                                                                \
        "db-system-id=" HRN_PG_SYSTEMID_94_Z "\n"                                                                                  \
        "db-version=\"9.4\"\n"

    #define TEST_BACKUP_DB2_11                                                                                                     \
        "db-catalog-version=201707211\n"                                                                                           \
        "db-control-version=1100\n"                                                                                                \
        "db-id=2\n"                                                                                                                \
        "db-system-id=" HRN_PG_SYSTEMID_11_Z "\n"                                                                                  \
        "db-version=\"11\"\n"

    #define TEST_BACKUP_DB1_CURRENT_FULL1                                                                                          \
        "20181119-152138F={"                                                                                                       \
        "\"backrest-format\":5,\"backrest-version\":\"2.28dev\","                                                                  \
        "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","              \
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"                                               \
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"                                                       \
        "\"backup-timestamp-start\":1482182846,\"backup-timestamp-stop\":1482182861,\"backup-type\":\"full\","                     \
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"                 \
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"

    #define TEST_BACKUP_DB1_CURRENT_FULL2                                                                                          \
        "20181119-152800F={"                                                                                                       \
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","                                                                  \
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"                                               \
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"                                                       \
        "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","                     \
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"                 \
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"

    #define TEST_BACKUP_DB1_CURRENT_FULL3                                                                                          \
        "20181119-152900F={"                                                                                                       \
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","                                                                  \
        "\"backup-archive-start\":\"000000010000000000000004\",\"backup-archive-stop\":\"000000010000000000000004\","              \
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"                                               \
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"                                                       \
        "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","                     \
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"                 \
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"

    #define TEST_BACKUP_DB1_HISTORY                                                                                                \
        "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":" HRN_PG_SYSTEMID_94_Z ","                \
            "\"db-version\":\"9.4\"}"

    #define TEST_BACKUP_DB2_HISTORY                                                                                                \
        "2={\"db-catalog-version\":201707211,\"db-control-version\":1100,\"db-system-id\":" HRN_PG_SYSTEMID_11_Z ","               \
            "\"db-version\":\"11\"}"

    #define TEST_BACKUP_INFO_MULTI_HISTORY_BASE                                                                                    \
        "[backup:current]\n"                                                                                                       \
        TEST_BACKUP_DB1_CURRENT_FULL1                                                                                              \
        TEST_BACKUP_DB1_CURRENT_FULL2                                                                                              \
        TEST_BACKUP_DB1_CURRENT_FULL3                                                                                              \
        "\n"                                                                                                                       \
        "[db]\n"                                                                                                                   \
        TEST_BACKUP_DB2_11                                                                                                         \
        "\n"                                                                                                                       \
        "[db:history]\n"                                                                                                           \
        TEST_BACKUP_DB1_HISTORY                                                                                                    \
        "\n"                                                                                                                       \
        TEST_BACKUP_DB2_HISTORY

    #define TEST_ARCHIVE_INFO_BASE                                                                                                 \
        "[db]\n"                                                                                                                   \
        "db-id=1\n"                                                                                                                \
        "db-system-id=" HRN_PG_SYSTEMID_94_Z "\n"                                                                                  \
        "db-version=\"9.4\"\n"                                                                                                     \
        "\n"                                                                                                                       \
        "[db:history]\n"                                                                                                           \
        "1={\"db-id\":" HRN_PG_SYSTEMID_94_Z ",\"db-version\":\"9.4\"}"

    #define TEST_ARCHIVE_INFO_MULTI_HISTORY_BASE                                                                                   \
        "[db]\n"                                                                                                                   \
        "db-id=2\n"                                                                                                                \
        "db-system-id=" HRN_PG_SYSTEMID_11_Z "\n"                                                                                  \
        "db-version=\"11\"\n"                                                                                                      \
        "\n"                                                                                                                       \
        "[db:history]\n"                                                                                                           \
        "1={\"db-id\":" HRN_PG_SYSTEMID_94_Z ",\"db-version\":\"9.4\"}\n"                                                          \
        "2={\"db-id\":" HRN_PG_SYSTEMID_11_Z ",\"db-version\":\"11\"}"

    #define TEST_MANIFEST_HEADER                                                                                                   \
        "[backup]\n"                                                                                                               \
        "backup-label=null\n"                                                                                                      \
        "backup-timestamp-copy-start=0\n"                                                                                          \
        "backup-timestamp-start=0\n"                                                                                               \
        "backup-timestamp-stop=0\n"                                                                                                \
        "backup-type=\"full\"\n"

    #define TEST_MANIFEST_DB_92                                                                                                    \
        "\n"                                                                                                                       \
        "[backup:db]\n"                                                                                                            \
        "db-catalog-version=201204301\n"                                                                                           \
        "db-control-version=922\n"                                                                                                 \
        "db-id=1\n"                                                                                                                \
        "db-system-id=" HRN_PG_SYSTEMID_94_Z "\n"                                                                                  \
        "db-version=\"9.2\"\n"

    #define TEST_MANIFEST_DB_94                                                                                                    \
        "\n"                                                                                                                       \
        "[backup:db]\n"                                                                                                            \
        "db-catalog-version=201409291\n"                                                                                           \
        "db-control-version=942\n"                                                                                                 \
        "db-id=1\n"                                                                                                                \
        "db-system-id=" HRN_PG_SYSTEMID_94_Z "\n"                                                                                  \
        "db-version=\"9.4\"\n"

    #define TEST_MANIFEST_OPTION_ALL                                                                                               \
        "\n"                                                                                                                       \
        "[backup:option]\n"                                                                                                        \
        "option-archive-check=false\n"                                                                                             \
        "option-archive-copy=false\n"                                                                                              \
        "option-checksum-page=false\n"                                                                                             \
        "option-compress=false\n"                                                                                                  \
        "option-compress-type=\"none\"\n"                                                                                          \
        "option-hardlink=false\n"                                                                                                  \
        "option-online=false\n"

    #define TEST_MANIFEST_OPTION_ARCHIVE_TRUE                                                                                      \
        "\n"                                                                                                                       \
        "[backup:option]\n"                                                                                                        \
        "option-archive-check=true\n"                                                                                              \
        "option-archive-copy=true\n"

    #define TEST_MANIFEST_TARGET                                                                                                   \
        "\n"                                                                                                                       \
        "[backup:target]\n"                                                                                                        \
        "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"

    #define TEST_MANIFEST_DB                                                                                                       \
        "\n"                                                                                                                       \
        "[db]\n"                                                                                                                   \
        "postgres={\"db-id\":12173,\"db-last-system-id\":12168}\n"

    #define TEST_MANIFEST_FILE                                                                                                     \
        "\n"                                                                                                                       \
        "[target:file]\n"                                                                                                          \
        "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"size\":4,\"timestamp\":1565282114}\n"

    #define TEST_MANIFEST_FILE_DEFAULT                                                                                             \
        "\n"                                                                                                                       \
        "[target:file:default]\n"                                                                                                  \
        "group=\"group1\"\n"                                                                                                       \
        "mode=\"0600\"\n"                                                                                                          \
        "user=\"user1\"\n"

    #define TEST_MANIFEST_LINK                                                                                                     \
        "\n"                                                                                                                       \
        "[target:link]\n"                                                                                                          \
        "pg_data/pg_stat={\"destination\":\"../pg_stat\"}\n"

    #define TEST_MANIFEST_LINK_DEFAULT                                                                                             \
        "\n"                                                                                                                       \
        "[target:link:default]\n"                                                                                                  \
        "group=\"group1\"\n"                                                                                                       \
        "user=false\n"

    #define TEST_MANIFEST_PATH                                                                                                     \
        "\n"                                                                                                                       \
        "[target:path]\n"                                                                                                          \
        "pg_data={\"user\":\"user1\"}\n"                                                                                           \

    #define TEST_MANIFEST_PATH_DEFAULT                                                                                             \
        "\n"                                                                                                                       \
        "[target:path:default]\n"                                                                                                  \
        "group=false\n"                                                                                                            \
        "mode=\"0700\"\n"                                                                                                          \
        "user=\"user1\"\n"

    #define TEST_INVALID_BACKREST_INFO                                                                                             \
        "[backrest]\n"                                                                                                             \
        "backrest-checksum=\"BOGUS\"\n"                                                                                            \
        "backrest-format=5\n"                                                                                                      \
        "backrest-version=\"2.28\"\n"

    // *****************************************************************************************************************************
    if (testBegin("verifyManifestFile()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        HRN_CFG_LOAD(cfgCmdVerify, argList);

        #define TEST_BACKUP_LABEL_FULL                              "20181119-152138F"

        Manifest *manifest = NULL;
        unsigned int jobErrorTotal = 0;
        VerifyBackupResult backupResult = {.backupLabel = strNewZ(TEST_BACKUP_LABEL_FULL)};

        InfoPg *infoPg = NULL;
        TEST_ASSIGN(
            infoPg, infoArchivePg(infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(TEST_ARCHIVE_INFO_BASE)))),
            "infoPg from archive.info");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("manifest.copy exists, no manifest main, manifest db version not in history, not current db");

        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/" TEST_BACKUP_LABEL_FULL "/" BACKUP_MANIFEST_FILE,
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_DB_92
            TEST_MANIFEST_OPTION_ALL
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT,
            .comment = "manifest db section mismatch");

        backupResult.status = backupValid;
        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, false, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_PTR(manifest, NULL, "manifest not set - pg version mismatch");
        TEST_RESULT_UINT(backupResult.status, backupInvalid, "manifest unusable - backup invalid");
        TEST_RESULT_LOG(
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/20181119-152138F/backup.manifest.copy'"
                " for read\n"
            "P00  ERROR: [028]: '20181119-152138F' may not be recoverable - PG data (id 1, version 9.2, system-id "
                HRN_PG_SYSTEMID_94_Z ") is not in the backup.info history, skipping");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("rerun test with db-system-id invalid and no main");

        HRN_STORAGE_REMOVE(storageRepoWrite(), STORAGE_REPO_BACKUP "/" TEST_BACKUP_LABEL_FULL "/" BACKUP_MANIFEST_FILE);
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/" TEST_BACKUP_LABEL_FULL "/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
            TEST_MANIFEST_HEADER
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=0\n"
            "db-version=\"9.4\"\n"
            TEST_MANIFEST_OPTION_ALL
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT,
            .comment = "manifest copy - invalid system-id");

        backupResult.status = backupValid;
        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, false, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_PTR(manifest, NULL, "manifest not set - pg system-id mismatch");
        TEST_RESULT_UINT(backupResult.status, backupInvalid, "manifest unusable - backup invalid");
        TEST_RESULT_LOG(
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/20181119-152138F/backup.manifest' for read\n"
            "P00   WARN: 20181119-152138F/backup.manifest is missing or unusable, using copy\n"
            "P00  ERROR: [028]: '20181119-152138F' may not be recoverable - PG data (id 1, version 9.4, system-id 0) is "
                "not in the backup.info history, skipping");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("rerun copy test with db-id invalid");

        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/" TEST_BACKUP_LABEL_FULL "/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
            TEST_MANIFEST_HEADER
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=0\n"
            "db-system-id=" HRN_PG_SYSTEMID_94_Z "\n"
            "db-version=\"9.4\"\n"
            TEST_MANIFEST_OPTION_ALL
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT,
            .comment = "manifest copy - invalid db-id");

        backupResult.status = backupValid;
        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, false, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_PTR(manifest, NULL, "manifest not set - pg db-id mismatch");
        TEST_RESULT_UINT(backupResult.status, backupInvalid, "manifest unusable - backup invalid");
        TEST_RESULT_LOG(
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/20181119-152138F/backup.manifest' for read\n"
            "P00   WARN: 20181119-152138F/backup.manifest is missing or unusable, using copy\n"
            "P00  ERROR: [028]: '20181119-152138F' may not be recoverable - PG data (id 0, version 9.4, system-id "
                HRN_PG_SYSTEMID_94_Z ") is not in the backup.info history, skipping");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing main manifest, errored copy");

        backupResult.status = backupValid;

        HRN_STORAGE_PUT_Z(
            storageRepoWrite(), TEST_PATH "/repo/" STORAGE_PATH_BACKUP "/db/" TEST_BACKUP_LABEL_FULL "/" BACKUP_MANIFEST_FILE
            INFO_COPY_EXT, TEST_INVALID_BACKREST_INFO, .comment = "invalid manifest copy");

        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, false, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_UINT(backupResult.status, backupInvalid, "manifest unusable - backup invalid");
        TEST_RESULT_LOG(
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/20181119-152138F/backup.manifest' for read\n"
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS' "
                "<REPO:BACKUP>/20181119-152138F/backup.manifest.copy");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("current backup true");

        HRN_STORAGE_PUT_Z(
            storageRepoWrite(), TEST_PATH "/repo/" STORAGE_PATH_BACKUP "/db/" TEST_BACKUP_LABEL_FULL "/" BACKUP_MANIFEST_FILE,
            TEST_INVALID_BACKREST_INFO, .comment = "invalid manifest");

        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, true, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_PTR(manifest, NULL, "manifest not set");
        TEST_RESULT_UINT(backupResult.status, backupInvalid, "manifest unusable - backup invalid");
        TEST_RESULT_LOG(
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS' "
                "<REPO:BACKUP>/20181119-152138F/backup.manifest\n"
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS' "
                "<REPO:BACKUP>/20181119-152138F/backup.manifest.copy");

        // Write a valid manifest with a manifest copy that is invalid
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/" TEST_BACKUP_LABEL_FULL "/" BACKUP_MANIFEST_FILE,
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_DB_94
            TEST_MANIFEST_OPTION_ALL
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT,
            .comment = "valid manifest");

        backupResult.status = backupValid;
        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, true, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_PTR_NE(manifest, NULL, "manifest set");
        TEST_RESULT_UINT(backupResult.status, backupValid, "manifest usable");
        TEST_RESULT_LOG("P00   WARN: backup '20181119-152138F' manifest.copy does not match manifest");
    }

    // *****************************************************************************************************************************
    if (testBegin("verifyCreateArchiveIdRange()"))
    {
        VerifyWalRange *walRangeResult = NULL;
        unsigned int errTotal = 0;
        StringList *walFileList = strLstNew();

        VerifyArchiveResult archiveResult =
        {
            .archiveId = strNewZ("9.4-1"),
            .walRangeList = lstNewP(sizeof(VerifyWalRange), .comparator = lstComparatorStr),
        };
        List *archiveIdResultList = lstNewP(sizeof(VerifyArchiveResult), .comparator = archiveIdComparator);
        lstAdd(archiveIdResultList, &archiveResult);
        VerifyArchiveResult *archiveIdResult = lstGetLast(archiveIdResultList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Single WAL");

        archiveIdResult->pgWalInfo.size = PG_WAL_SEGMENT_SIZE_DEFAULT;
        archiveIdResult->pgWalInfo.version = PG_VERSION_94;

        strLstAddZ(walFileList, "000000020000000200000000-daa497dba64008db824607940609ba1cd7c6c501.gz");

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 0, "no errors");
        TEST_RESULT_UINT(lstSize(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList), 1, "single range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 0),
            "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000000", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Duplicate WAL only - no range, all removed from list");

        lstClear(archiveIdResult->walRangeList);

        // Add a duplicate
        strLstAddZ(walFileList, "000000020000000200000000");

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 1, "duplicate WAL error");
        TEST_RESULT_UINT(strLstSize(walFileList), 0, "all WAL removed from WAL file list");
        TEST_RESULT_UINT(lstSize(archiveIdResult->walRangeList), 0, "no range");
        TEST_RESULT_LOG("P00  ERROR: [028]: duplicate WAL '000000020000000200000000' for '9.4-1' exists, skipping");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("FF Wal not skipped > 9.2, duplicates at beginning and end of list are removed");

        errTotal = 0;
        strLstAddZ(walFileList, "000000020000000100000000-daa497dba64008db824607940609ba1cd7c6c501.gz");
        strLstAddZ(walFileList, "000000020000000100000000");
        strLstAddZ(walFileList, "000000020000000100000000-aaaaaadba64008db824607940609ba1cd7c6c501");
        strLstAddZ(walFileList, "0000000200000001000000FD-daa497dba64008db824607940609ba1cd7c6c501.gz");
        strLstAddZ(walFileList, "0000000200000001000000FE-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz");
        strLstAddZ(walFileList, "0000000200000001000000FF-daa497dba64008db824607940609ba1cd7c6c501");
        strLstAddZ(walFileList, "000000020000000200000000");
        strLstAddZ(walFileList, "000000020000000200000001");
        strLstAddZ(walFileList, "000000020000000200000001");

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 2, "triplicate WAL error at beginning, duplicate WAL at end");
        TEST_RESULT_UINT(strLstSize(walFileList), 4, "only duplicate WAL removed from WAL list");
        TEST_RESULT_UINT(lstSize(archiveIdResultList), 1, "single archiveId result");
        TEST_RESULT_UINT(lstSize(archiveIdResult->walRangeList), 1, "single range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 0),
            "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
        TEST_RESULT_LOG(
            "P00  ERROR: [028]: duplicate WAL '000000020000000100000000' for '9.4-1' exists, skipping\n"
            "P00  ERROR: [028]: duplicate WAL '000000020000000200000001' for '9.4-1' exists, skipping");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("FF Wal skipped <= 9.2, duplicates in middle of list removed");

        // Clear the range lists and rerun the test with PG_VERSION_92 to ensure FF is reported as an error
        lstClear(archiveIdResult->walRangeList);
        errTotal = 0;
        archiveIdResult->archiveId = strNewZ("9.2-1");
        archiveIdResult->pgWalInfo.version = PG_VERSION_92;

        strLstAddZ(walFileList, "000000020000000200000001");
        strLstAddZ(walFileList, "000000020000000200000001");
        strLstAddZ(walFileList, "000000020000000200000002");

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 2, "error reported");
        TEST_RESULT_UINT(lstSize(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList), 2, "multiple ranges");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 0),
            "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 1),
            "get second range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000002", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000002", "stop range");

        TEST_RESULT_LOG(
            "P00  ERROR: [028]: invalid WAL '0000000200000001000000FF' for '9.2-1' exists, skipping\n"
            "P00  ERROR: [028]: duplicate WAL '000000020000000200000001' for '9.2-1' exists, skipping");

        TEST_RESULT_STRLST_Z(
            walFileList,
            "0000000200000001000000FD-daa497dba64008db824607940609ba1cd7c6c501.gz\n"
            "0000000200000001000000FE-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz\n"
            "000000020000000200000000\n000000020000000200000002\n",
            "skipped files removed");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Rerun <= 9.2, missing FF not a gap");

        // Clear the range lists, rerun the PG_VERSION_92 test to ensure the missing FF is not considered a gap
        lstClear(archiveIdResult->walRangeList);
        errTotal = 0;

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 0, "error reported");
        TEST_RESULT_UINT(lstSize(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList), 2, "multiple ranges");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 0),
            "get range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 1),
            "get second range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000002", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000002", "stop range");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("version > 9.2, missing FF is a gap");

        // Clear the range lists and update the version > 9.2 so missing FF is considered a gap in the WAL ranges
        lstClear(archiveIdResult->walRangeList);
        errTotal = 0;
        archiveIdResult->archiveId = strNewZ("9.6-1");
        archiveIdResult->pgWalInfo.version = PG_VERSION_96;

        strLstAddZ(walFileList, "000000020000000200000003-123456");
        strLstAddZ(walFileList, "000000020000000200000004-123456");

        TEST_RESULT_VOID(verifyCreateArchiveIdRange(archiveIdResult, walFileList, &errTotal), "create archiveId WAL range");
        TEST_RESULT_UINT(errTotal, 0, "no errors");
        TEST_RESULT_UINT(lstSize(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList), 3, "multiple ranges");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 0),
            "get first range");
        TEST_RESULT_STR_Z(walRangeResult->start, "0000000200000001000000FD", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "0000000200000001000000FE", "stop range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 1),
            "get second range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000000", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000000", "stop range");
        TEST_ASSIGN(
            walRangeResult, (VerifyWalRange *)lstGet(((VerifyArchiveResult *)lstGet(archiveIdResultList, 0))->walRangeList, 2),
            "get third range");
        TEST_RESULT_STR_Z(walRangeResult->start, "000000020000000200000002", "start range");
        TEST_RESULT_STR_Z(walRangeResult->stop, "000000020000000200000004", "stop range");
    }

    // *****************************************************************************************************************************
    if (testBegin("verifyPgHistory()"))
    {
        // Create backup.info
        InfoBackup *backupInfo = NULL;
        TEST_ASSIGN(
            backupInfo, infoBackupNewLoad(ioBufferReadNew(harnessInfoChecksumZ(TEST_BACKUP_INFO_MULTI_HISTORY_BASE))),
            "backup.info multi-history");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("history mismatch - missing history");

        // Create archive.info - history mismatch
        InfoArchive *archiveInfo = NULL;
        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=" HRN_PG_SYSTEMID_11_Z "\n"
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "2={\"db-id\":" HRN_PG_SYSTEMID_11_Z ",\"db-version\":\"11\"}"))), "archive.info missing history");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("history mismatch - system id");

        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=" HRN_PG_SYSTEMID_11_Z "\n"
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6625592122879095777,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":" HRN_PG_SYSTEMID_11_Z ",\"db-version\":\"11\"}"))), "archive.info history system id mismatch");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("history mismatch - version");

        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=" HRN_PG_SYSTEMID_11_Z "\n"
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":" HRN_PG_SYSTEMID_94_Z ",\"db-version\":\"9.5\"}\n"
                "2={\"db-id\":" HRN_PG_SYSTEMID_11_Z ",\"db-version\":\"11\"}"))), "archive.info history version mismatch");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("history mismatch - id");

        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=" HRN_PG_SYSTEMID_11_Z "\n"
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "3={\"db-id\":" HRN_PG_SYSTEMID_94_Z ",\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":" HRN_PG_SYSTEMID_11_Z ",\"db-version\":\"11\"}"))), "archive.info history id mismatch");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");
    }

    // *****************************************************************************************************************************
    if (testBegin("verifySetBackupCheckArchive(), verifyLogInvalidResult(), verifyRender()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifySetBackupCheckArchive()");

        InfoBackup *backupInfo = NULL;
        InfoArchive *archiveInfo = NULL;
        TEST_ASSIGN(
            backupInfo, infoBackupNewLoad(ioBufferReadNew(harnessInfoChecksumZ(TEST_BACKUP_INFO_MULTI_HISTORY_BASE))),
            "backup.info multi-history");
        TEST_ASSIGN(archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(TEST_ARCHIVE_INFO_MULTI_HISTORY_BASE))),
            "archive.info multi-history");
        InfoPg *pgHistory = infoArchivePg(archiveInfo);

        StringList *backupList= strLstNew();
        strLstAddZ(backupList, "20181119-152138F");
        strLstAddZ(backupList, "20181119-152900F");
        StringList *archiveIdList = strLstComparatorSet(strLstNew(), archiveIdComparator);
        strLstAddZ(archiveIdList, "9.4-1");
        strLstAddZ(archiveIdList, "11-2");

        unsigned int errTotal = 0;

        // Add backup to end of list
        strLstAddZ(backupList, "20181119-153000F");
        strLstAddZ(archiveIdList, "12-3");

        TEST_RESULT_STR_Z(
            verifySetBackupCheckArchive(backupList, backupInfo, archiveIdList, pgHistory, &errTotal),
            "20181119-153000F", "current backup, missing archive");
        TEST_RESULT_UINT(errTotal, 1, "error logged");
        TEST_RESULT_LOG("P00  ERROR: [044]: archiveIds '12-3' are not in the archive.info history list");

        errTotal = 0;
        strLstAddZ(archiveIdList, "13-4");
        TEST_RESULT_STR_Z(
            verifySetBackupCheckArchive(backupList, backupInfo, archiveIdList, pgHistory, &errTotal),
            "20181119-153000F", "test multiple archiveIds on disk not in archive.info");
        TEST_RESULT_UINT(errTotal, 1, "error logged");
        TEST_RESULT_LOG("P00  ERROR: [044]: archiveIds '12-3, 13-4' are not in the archive.info history list");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyLogInvalidResult() - missing file");

        TEST_RESULT_UINT(
            verifyLogInvalidResult(STORAGE_REPO_ARCHIVE_STR, verifyFileMissing, 0, STRDEF("missingfilename")),
            0, "file missing message");
        TEST_RESULT_LOG("P00   WARN: file missing 'missingfilename'");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyRender() - missing file, empty invalidList");

        List *archiveIdResultList = lstNewP(sizeof(VerifyArchiveResult), .comparator = archiveIdComparator);
        List *backupResultList = lstNewP(sizeof(VerifyBackupResult), .comparator = lstComparatorStr);

        VerifyArchiveResult archiveIdResult =
        {
            .archiveId = strNewZ("9.6-1"),
            .totalWalFile = 1,
            .walRangeList = lstNewP(sizeof(VerifyWalRange), .comparator = lstComparatorStr),
        };
        VerifyWalRange walRange =
        {
            .start = strNewZ("0"),
            .stop = strNewZ("2"),
            .invalidFileList = lstNewP(sizeof(VerifyInvalidFile), .comparator = lstComparatorStr),
        };

        lstAdd(archiveIdResult.walRangeList, &walRange);
        lstAdd(archiveIdResultList, &archiveIdResult);
        TEST_RESULT_STR_Z(
            verifyRender(archiveIdResultList, backupResultList),
            "Results:\n"
            "  archiveId: 9.6-1, total WAL checked: 1, total valid WAL: 0\n"
            "    missing: 0, checksum invalid: 0, size invalid: 0, other: 0\n"
            "  backup: none found", "archive: no invalid file list");

        VerifyInvalidFile invalidFile =
        {
            .fileName = strNewZ("file"),
            .reason = verifyFileMissing,
        };
        lstAdd(walRange.invalidFileList, &invalidFile);

        VerifyBackupResult backupResult =
        {
            .backupLabel = strNewZ("test-backup-label"),
            .status = backupInvalid,
            .totalFileVerify = 1,
            .invalidFileList = lstNewP(sizeof(VerifyInvalidFile), .comparator = lstComparatorStr),
        };
        lstAdd(backupResult.invalidFileList, &invalidFile);
        lstAdd(backupResultList, &backupResult);

        TEST_RESULT_STR_Z(
            verifyRender(archiveIdResultList, backupResultList),
            "Results:\n"
            "  archiveId: 9.6-1, total WAL checked: 1, total valid WAL: 0\n"
            "    missing: 1, checksum invalid: 0, size invalid: 0, other: 0\n"
            "  backup: test-backup-label, status: invalid, total files checked: 1, total valid files: 0\n"
            "    missing: 1, checksum invalid: 0, size invalid: 0, other: 0", "archive file missing, backup file missing");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyAddInvalidWalFile() - file missing (coverage test)");

        TEST_RESULT_VOID(
            verifyAddInvalidWalFile(archiveIdResult.walRangeList, verifyFileMissing, STRDEF("test"), STRDEF("3")), "coverage test");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify() - info files"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        HRN_CFG_LOAD(cfgCmdVerify, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info invalid checksum, neither backup copy nor archive infos exist");

        HRN_STORAGE_PUT_Z(storageRepoWrite(), INFO_BACKUP_PATH_FILE, TEST_INVALID_BACKREST_INFO, .comment = "invalid backup.info");
        TEST_ERROR(cmdVerify(), RuntimeError, "2 fatal errors encountered, see log for details");
        TEST_RESULT_LOG(
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS' "
                "<REPO:BACKUP>/backup.info\n"
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/backup.info.copy' for read\n"
            "P00  ERROR: [029]: No usable backup.info file\n"
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/archive/db/archive.info' for read\n"
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/archive/db/archive.info.copy' for read\n"
            "P00  ERROR: [029]: No usable archive.info file");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info invalid checksum, backup.info.copy valid, archive.info not exist, archive copy checksum invalid");

        HRN_STORAGE_PUT_Z(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT, TEST_INVALID_BACKREST_INFO,
            .comment = "invalid archive.info.copy");
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE INFO_COPY_EXT,
            "[backup:current]\n"
            TEST_BACKUP_DB1_CURRENT_FULL1
            "\n"
            "[db]\n"
            TEST_BACKUP_DB1_94
            "\n"
            "[db:history]\n"
            TEST_BACKUP_DB1_HISTORY,
            .comment = "valid backup.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        TEST_RESULT_LOG(
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS'"
                " <REPO:BACKUP>/backup.info\n"
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/archive/db/archive.info' for read\n"
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS'"
                " <REPO:ARCHIVE>/archive.info.copy\n"
            "P00  ERROR: [029]: No usable archive.info file");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy valid but checksum mismatch, archive.info checksum invalid, archive.info copy valid");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE, TEST_BACKUP_INFO_MULTI_HISTORY_BASE, .comment = "valid backup.info");
        HRN_STORAGE_PUT_Z(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE, TEST_INVALID_BACKREST_INFO, .comment = "invalid archive.info");
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT, TEST_ARCHIVE_INFO_BASE, .comment = "valid archive.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        TEST_RESULT_LOG(
            "P00   WARN: backup.info.copy does not match backup.info\n"
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS'"
                " <REPO:ARCHIVE>/archive.info\n"
            "P00  ERROR: [029]: backup info file and archive info file do not match\n"
            "            archive: id = 1, version = 9.4, system-id = " HRN_PG_SYSTEMID_94_Z "\n"
            "            backup : id = 2, version = 11, system-id = " HRN_PG_SYSTEMID_11_Z "\n"
            "            HINT: this may be a symptom of repository corruption!");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy valid and checksums match, archive.info and copy valid, but checksum mismatch");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE INFO_COPY_EXT, TEST_BACKUP_INFO_MULTI_HISTORY_BASE,
            .comment = "valid backup.info.copy");
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE, TEST_ARCHIVE_INFO_MULTI_HISTORY_BASE, .comment = "valid archive.info");
        TEST_RESULT_VOID(cmdVerify(), "usable backup and archive info files");
        TEST_RESULT_LOG(
            "P00   WARN: archive.info.copy does not match archive.info\n"
            "P00   WARN: no archives or backups exist in the repo");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info valid, copy invalid, archive.info valid, copy invalid");

        HRN_STORAGE_REMOVE(storageRepoWrite(), INFO_BACKUP_PATH_FILE INFO_COPY_EXT, .comment = "remove backup.info.copy");
        HRN_STORAGE_REMOVE(storageRepoWrite(), INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT, .comment = "remove archive.info.copy");
        TEST_RESULT_VOID(cmdVerify(), "usable backup and archive info files");
        TEST_RESULT_LOG(
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/backup.info.copy' for read\n"
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/archive/db/archive.info.copy' for read\n"
            "P00   WARN: no archives or backups exist in the repo");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy missing, archive.info and copy valid");

        HRN_STORAGE_REMOVE(storageRepoWrite(), INFO_BACKUP_PATH_FILE);
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT, TEST_ARCHIVE_INFO_MULTI_HISTORY_BASE,
            .comment = "valid and matching archive.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        TEST_RESULT_LOG(
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/backup.info' for read\n"
            "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/backup.info.copy' for read\n"
            "P00  ERROR: [029]: No usable backup.info file");
    }

    // *****************************************************************************************************************************
    if (testBegin("verifyFile()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        HRN_CFG_LOAD(cfgCmdVerify, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("zero-sized file in archive");

        String *filePathName = strNewZ(STORAGE_REPO_ARCHIVE "/testfile");
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), strZ(filePathName));
        TEST_RESULT_UINT(verifyFile(filePathName, 0, 0, 0, STRDEF(HASH_TYPE_SHA1_ZERO), 0, NULL), verifyOk, "file ok");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file size invalid in archive");

        HRN_STORAGE_PUT_Z(storageRepoWrite(), strZ(filePathName), fileContents);
        TEST_RESULT_UINT(verifyFile(filePathName, 0, 0, 0, fileChecksum, 0, NULL), verifySizeInvalid, "file size invalid");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing in archive");
        TEST_RESULT_UINT(
            verifyFile(
                strNewFmt(STORAGE_REPO_ARCHIVE "/missingFile"), 0, 0, 0, fileChecksum, 0, NULL), verifyFileMissing, "file missing");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("encrypted/compressed file in backup");

        // Create a compressed encrypted repo file in backup
        filePathName = strCatZ(strNew(), STORAGE_REPO_BACKUP "/testfile");
        HRN_STORAGE_PUT_Z(
            storageRepoWrite(), strZ(filePathName), fileContents, .compressType = compressTypeGz, .cipherType = cipherTypeAes256Cbc,
            .cipherPass = "pass");

        strCatZ(filePathName, ".gz");
        TEST_RESULT_UINT(
            verifyFile(filePathName, 0, 0, 0, fileChecksum, fileSize, STRDEF("pass")), verifyOk, "file encrypted compressed ok");
        TEST_RESULT_UINT(
            verifyFile(
                filePathName, 0, 0, 0, STRDEF("badchecksum"), fileSize, STRDEF("pass")), verifyChecksumMismatch,
                "file encrypted compressed checksum mismatch");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify(), verifyProcess() - errors"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        // Load Parameters with multi-repo
        StringList *argList = strLstDup(argListBase);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 4, TEST_PATH "/repo4");
        HRN_CFG_LOAD(cfgCmdVerify, argList);

        // Store valid archive/backup info files
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE, TEST_ARCHIVE_INFO_MULTI_HISTORY_BASE, .comment = "valid archive.info");
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT, TEST_ARCHIVE_INFO_MULTI_HISTORY_BASE,
            .comment = "valid archive.info.copy");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid info files, WAL files present, no backups");

        #define TEST_NO_CURRENT_BACKUP                                                                                             \
            "[db]\n"                                                                                                               \
            TEST_BACKUP_DB2_11                                                                                                     \
            "\n"                                                                                                                   \
            "[db:history]\n"                                                                                                       \
            TEST_BACKUP_DB1_HISTORY                                                                                                \
            "\n"                                                                                                                   \
            TEST_BACKUP_DB2_HISTORY

        HRN_INFO_PUT(storageRepoWrite(), INFO_BACKUP_PATH_FILE, TEST_NO_CURRENT_BACKUP, .comment = "no current backups");
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE INFO_COPY_EXT, TEST_NO_CURRENT_BACKUP, .comment = "no current backups copy");

        // Create WAL file with just header info and small WAL size
        Buffer *walBuffer = bufNew((size_t)(1024 * 1024));
        bufUsedSet(walBuffer, bufSize(walBuffer));
        memset(bufPtr(walBuffer), 0, bufSize(walBuffer));
        hrnPgWalToBuffer((PgWal){.version = PG_VERSION_11, .size = 1024 * 1024}, walBuffer);
        const char *walBufferSha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer)));

        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0),
            strZ(strNewFmt(STORAGE_REPO_ARCHIVE "/11-2/0000000200000007/000000020000000700000FFE-%s", walBufferSha1)), walBuffer,
            .comment = "valid WAL");
        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/11-2/0000000200000007/000000020000000700000FFE-bad817043007aa2100c44c712bcb456db705dab9",
            walBuffer, .comment = "duplicate WAL");

        // Set log detail level to capture ranges (there should be none)
        harnessLogLevelSet(logLevelDetail);

        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        TEST_RESULT_LOG(
            "P00   WARN: no backups exist in the repo\n"
            "P00  ERROR: [028]: duplicate WAL '000000020000000700000FFE' for '11-2' exists, skipping\n"
            "P00   WARN: path '11-2/0000000200000007' does not contain any valid WAL to be processed\n"
            "P00   INFO: Results:\n"
            "              archiveId: 11-2, total WAL checked: 2, total valid WAL: 0\n"
            "                missing: 0, checksum invalid: 0, size invalid: 0, other: 0\n"
            "              backup: none found");

        harnessLogLevelReset();

        HRN_STORAGE_REMOVE(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/11-2/0000000200000007/000000020000000700000FFE-bad817043007aa2100c44c712bcb456db705dab9",
            .comment = "remove duplicate WAL");

        HRN_STORAGE_PATH_CREATE(
            storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/9.4-1", .comment = "empty path for old archiveId");
        HRN_STORAGE_PATH_CREATE(
            storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/11-2/0000000100000000", .comment = "empty timeline path");

        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4",
            walBuffer, .compressType = compressTypeGz, .comment = "first WAL compressed - but checksum failure");
        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0),
            strZ(
                strNewFmt(
                    STORAGE_REPO_ARCHIVE "/11-2/0000000200000007/000000020000000700000FFF-%s",
                    strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, BUFSTRDEF("invalidsize")))))),
            BUFSTRDEF("invalidsize"), .comment = "WAL - invalid size");
        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0),
            strZ(strNewFmt(STORAGE_REPO_ARCHIVE "/11-2/0000000200000008/000000020000000800000000-%s", walBufferSha1)),
            walBuffer, .comment = "WAL - continue range");

        // Set log detail level to capture ranges
        harnessLogLevelSet(logLevelDetail);

        // Test verifyProcess directly
        unsigned int errorTotal = 0;
        TEST_RESULT_STR_Z(
            verifyProcess(&errorTotal),
            "Results:\n"
            "  archiveId: 9.4-1, total WAL checked: 0, total valid WAL: 0\n"
            "  archiveId: 11-2, total WAL checked: 4, total valid WAL: 2\n"
            "    missing: 0, checksum invalid: 1, size invalid: 1, other: 0\n"
            "  backup: none found",
            "verifyProcess() results");
        TEST_RESULT_UINT(errorTotal, 2, "errors");
        TEST_RESULT_LOG(
            "P00   WARN: no backups exist in the repo\n"
            "P00   WARN: archive path '9.4-1' is empty\n"
            "P00   WARN: path '11-2/0000000100000000' does not contain any valid WAL to be processed\n"
            "P01  ERROR: [028]: invalid checksum "
                "'11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz'\n"
            "P01  ERROR: [028]: invalid size "
                "'11-2/0000000200000007/000000020000000700000FFF-ee161f898c9012dd0c28b3fd1e7140b9cf411306'\n"
            "P00 DETAIL: archiveId: 11-2, wal start: 000000020000000700000FFD, wal stop: 000000020000000800000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid info files, start next timeline");

        // Load Parameters - single default repo
        argList = strLstDup(argListBase);
        HRN_CFG_LOAD(cfgCmdVerify, argList);

        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0),
            strZ(strNewFmt(STORAGE_REPO_ARCHIVE "/11-2/0000000200000008/000000020000000800000002-%s", walBufferSha1)),
            walBuffer, .comment = "WAL - starts next range");
        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0),
            strZ(strNewFmt(STORAGE_REPO_ARCHIVE "/11-2/0000000300000000/000000030000000000000000-%s", walBufferSha1)),
            walBuffer, .comment = "WAL - starts next timeline");
        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0),
            strZ(strNewFmt(STORAGE_REPO_ARCHIVE "/11-2/0000000300000000/000000030000000000000001-%s", walBufferSha1)),
            walBuffer, .comment = "WAL - end next timeline");

        // Set log level to errors only
        harnessLogLevelSet(logLevelError);

        TEST_ERROR(cmdVerify(), RuntimeError, "2 fatal errors encountered, see log for details");
        TEST_RESULT_LOG(
            "P01  ERROR: [028]: invalid checksum "
                "'11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz'\n"
            "P01  ERROR: [028]: invalid size "
                "'11-2/0000000200000007/000000020000000700000FFF-ee161f898c9012dd0c28b3fd1e7140b9cf411306'");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid info files - various archive/backup errors");

        // Load Parameters - single non-default repo
        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        HRN_CFG_LOAD(cfgCmdVerify, argList, .jobRetry = 1);

        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/11-2/0000000200000008/000000020000000800000003-656817043007aa2100c44c712bcb456db705dab9",
            walBuffer, .modeFile = 0200, .comment = "WAL - file not readable");

        HRN_STORAGE_PATH_CREATE(
            storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/20181119-152800F", .comment = "prior backup path missing manifests");

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/20181119-152810F/" BACKUP_MANIFEST_FILE,
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_DB_94
            TEST_MANIFEST_OPTION_ALL
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB,
            .comment = "manifest without target files");

        // Create full backup with files
        HRN_STORAGE_PUT_Z(
            storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/20181119-152900F/pg_data/PG_VERSION", "BOGUS",
            .comment = "put checksum-error backup file");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/20181119-152900F/pg_data/testzero",
            .comment = "put zero-size backup file");
        HRN_STORAGE_PUT_Z(
            storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/20181119-152900F/pg_data/testvalid", fileContents,
            .comment = "put valid file");

        // Write manifests for full backup
        String *manifestContent = strNewFmt(
            "[backup]\n"
            "backup-label=\"20181119-152900F\"\n"
            "backup-timestamp-copy-start=0\n"
            "backup-timestamp-start=0\n"
            "backup-timestamp-stop=0\n"
            "backup-type=\"full\"\n"
            "\n"
            "[backup:db]\n"
            TEST_BACKUP_DB2_11
            TEST_MANIFEST_OPTION_ALL
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            "pg_data/testvalid={\"checksum\":\"%s\",\"size\":7,\"timestamp\":1565282114}\n"
            "pg_data/testzero={\"repo-size\":20,\"size\":0,\"timestamp\":1601405663}\n"
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT,
            strZ(fileChecksum));

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/20181119-152900F/" BACKUP_MANIFEST_FILE, strZ(manifestContent),
            .comment = "valid manifest");
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/20181119-152900F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
            strZ(manifestContent), .comment = "valid manifest copy");

        // Create a manifest for the dependent that has references
        manifestContent = strNewFmt(
                "[backup]\n"
                "backup-label=\"20181119-152900F_20181119-152909D\"\n"
                "backup-timestamp-copy-start=0\n"
                "backup-timestamp-start=0\n"
                "backup-timestamp-stop=0\n"
                "backup-type=\"diff\"\n"
                "\n"
                "[backup:db]\n"
                TEST_BACKUP_DB2_11
                TEST_MANIFEST_OPTION_ALL
                TEST_MANIFEST_TARGET
                TEST_MANIFEST_DB
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\","
                    "\"reference\":\"20181119-152900F\",\"size\":4,\"timestamp\":1565282114}\n"
                "pg_data/testfile={\"checksum\":\"%s\",\"reference\":\"20181119-152900F\",\"size\":7,\"timestamp\":1565282114}\n"
                "pg_data/testfile2={\"checksum\":\"%s\",\"size\":7,\"timestamp\":1565282114}\n"
                "pg_data/testmissing="
                    "{\"checksum\":\"123473f470864e067ee3a22e64b47b0a1c356abc\",\"size\":7,\"timestamp\":1565282114}\n"
                "pg_data/testother={\"checksum\":\"%s\",\"reference\":\"UNPROCESSEDBACKUP\",\"size\":7,\"timestamp\":1565282114}\n"
                TEST_MANIFEST_FILE_DEFAULT
                TEST_MANIFEST_LINK
                TEST_MANIFEST_LINK_DEFAULT
                TEST_MANIFEST_PATH
                TEST_MANIFEST_PATH_DEFAULT,
            strZ(fileChecksum), strZ(fileChecksum), strZ(fileChecksum));

        // Write manifests for dependent backup
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/20181119-152900F_20181119-152909D/" BACKUP_MANIFEST_FILE,
            strZ(manifestContent), .comment = "manifest to dependent");
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/20181119-152900F_20181119-152909D/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
            strZ(manifestContent), .comment = "manifest copy to dependent");
        HRN_STORAGE_PUT_Z(
            storageRepoIdxWrite(0),
            STORAGE_REPO_BACKUP "/20181119-152900F_20181119-152909D/pg_data/testfile2", fileContents,
            .comment = "put valid file to dependent");

        // Create an unprocessed backup label with a file that will be referenced in the dependent manifest
        HRN_STORAGE_PUT_Z(
            storageRepoIdxWrite(0),
            STORAGE_REPO_BACKUP "/UNPROCESSEDBACKUP/pg_data/testother", fileContents, .modeFile = 0200,
            .comment = "put unreadable file to unprocessed backup");

        // Create in-progress backup
        HRN_STORAGE_PATH_CREATE(
            storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/20181119-153000F",
            .comment = "create empty backup path for newest backup so in-progress");

        // Set log level to capture ranges
        harnessLogLevelSet(logLevelDetail);

        TEST_ERROR(cmdVerify(), RuntimeError, "7 fatal errors encountered, see log for details");
        TEST_RESULT_LOG(
                "P00   WARN: archive path '9.4-1' is empty\n"
                "P00   WARN: path '11-2/0000000100000000' does not contain any valid WAL to be processed\n"
                "P01  ERROR: [028]: invalid checksum "
                    "'11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz'\n"
                "P01  ERROR: [028]: invalid size "
                    "'11-2/0000000200000007/000000020000000700000FFF-ee161f898c9012dd0c28b3fd1e7140b9cf411306'\n"
                "P01  ERROR: [039]: invalid result "
                    "11-2/0000000200000008/000000020000000800000003-656817043007aa2100c44c712bcb456db705dab9: [41] raised from "
                    "local-1 shim protocol: unable to open file '" TEST_PATH "/repo/archive/db/"
                    "11-2/0000000200000008/000000020000000800000003-656817043007aa2100c44c712bcb456db705dab9' for read:"
                    " [13] Permission denied\n"
                "            [FileOpenError] on retry after 0ms\n"
                "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/20181119-152800F/backup.manifest' for read\n"
                "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/20181119-152800F/backup.manifest.copy'"
                    " for read\n"
                "P00   WARN: manifest missing for '20181119-152800F' - backup may have expired\n"
                "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/20181119-152810F/backup.manifest.copy'"
                    " for read\n"
                "P00  ERROR: [028]: backup '20181119-152810F' manifest does not contain any target files to verify\n"
                "P01  ERROR: [028]: invalid checksum '20181119-152900F/pg_data/PG_VERSION'\n"
                "P01  ERROR: [028]: file missing '20181119-152900F_20181119-152909D/pg_data/testmissing'\n"
                "P00   WARN: unable to open missing file '" TEST_PATH "/repo/backup/db/20181119-153000F/backup.manifest' for read\n"
                "P00   INFO: backup '20181119-153000F' appears to be in progress, skipping\n"
                "P01  ERROR: [039]: invalid result UNPROCESSEDBACKUP/pg_data/testother: [41] raised from local-1 shim protocol:"
                    " unable to open file '" TEST_PATH "/repo/backup/db/UNPROCESSEDBACKUP/pg_data/testother' for read: [13]"
                    " Permission denied\n"
                "            [FileOpenError] on retry after 0ms\n"
                "P00 DETAIL: archiveId: 11-2, wal start: 000000020000000700000FFD, wal stop: 000000020000000800000000\n"
                "P00 DETAIL: archiveId: 11-2, wal start: 000000020000000800000002, wal stop: 000000020000000800000003\n"
                "P00 DETAIL: archiveId: 11-2, wal start: 000000030000000000000000, wal stop: 000000030000000000000001\n"
                "P00   INFO: Results:\n"
                "              archiveId: 9.4-1, total WAL checked: 0, total valid WAL: 0\n"
                "              archiveId: 11-2, total WAL checked: 8, total valid WAL: 5\n"
                "                missing: 0, checksum invalid: 1, size invalid: 1, other: 1\n"
                "              backup: 20181119-152800F, status: manifest missing, total files checked: 0, total valid files: 0\n"
                "              backup: 20181119-152810F, status: invalid, total files checked: 0, total valid files: 0\n"
                "              backup: 20181119-152900F, status: invalid, total files checked: 3, total valid files: 2\n"
                "                missing: 0, checksum invalid: 1, size invalid: 0, other: 0\n"
                "              backup: 20181119-152900F_20181119-152909D, status: invalid, total files checked: 5, "
                    "total valid files: 2\n"
                "                missing: 1, checksum invalid: 1, size invalid: 0, other: 1\n"
                "              backup: 20181119-153000F, status: in-progress, total files checked: 0, total valid files: 0");

        harnessLogLevelReset();
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        HRN_CFG_LOAD(cfgCmdVerify, argList);

        #define TEST_BACKUP_DB1_CURRENT_FULL3_DIFF1                                                                                \
            "20181119-152900F_20181119-152909D={"                                                                                  \
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","                                                              \
            "\"backup-archive-start\":\"000000010000000000000006\",\"backup-archive-stop\":\"000000010000000000000007\","          \
            "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"                                           \
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"                                                   \
            "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","                 \
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"             \
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"

        #define TEST_BACKUP_DB2_CURRENT_FULL1                                                                                      \
            "20201119-163000F={"                                                                                                   \
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","                                                              \
            "\"backup-archive-start\":\"000000020000000000000001\",\"backup-archive-stop\":\"000000020000000000000001\","          \
            "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"                                           \
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"                                                   \
            "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","                 \
            "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"             \
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("prior backup verification incomplete - referenced file checked");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE, TEST_ARCHIVE_INFO_MULTI_HISTORY_BASE, .comment = "valid archive.info");
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT, TEST_ARCHIVE_INFO_MULTI_HISTORY_BASE,
            .comment = "valid archive.info.copy");

        #define TEST_BACKUP_INFO                                                                                                   \
            "[backup:current]\n"                                                                                                   \
            TEST_BACKUP_DB1_CURRENT_FULL3                                                                                          \
            TEST_BACKUP_DB1_CURRENT_FULL3_DIFF1                                                                                    \
            TEST_BACKUP_DB2_CURRENT_FULL1                                                                                          \
            "\n"                                                                                                                   \
            "[db]\n"                                                                                                               \
            TEST_BACKUP_DB2_11                                                                                                     \
            "\n"                                                                                                                   \
            "[db:history]\n"                                                                                                       \
            TEST_BACKUP_DB1_HISTORY                                                                                                \
            "\n"                                                                                                                   \
            TEST_BACKUP_DB2_HISTORY

        HRN_INFO_PUT(storageRepoWrite(), INFO_BACKUP_PATH_FILE, TEST_BACKUP_INFO);
        HRN_INFO_PUT(storageRepoWrite(), INFO_BACKUP_PATH_FILE INFO_COPY_EXT, TEST_BACKUP_INFO);

        // Create valid full backup for DB1
        #define TEST_MANIFEST_FULL_DB1                                                                                             \
            TEST_MANIFEST_HEADER                                                                                                   \
            TEST_MANIFEST_DB_94                                                                                                    \
            TEST_MANIFEST_OPTION_ALL                                                                                               \
            TEST_MANIFEST_TARGET                                                                                                   \
            TEST_MANIFEST_DB                                                                                                       \
            TEST_MANIFEST_FILE                                                                                                     \
            TEST_MANIFEST_FILE_DEFAULT                                                                                             \
            TEST_MANIFEST_LINK                                                                                                     \
            TEST_MANIFEST_LINK_DEFAULT                                                                                             \
            TEST_MANIFEST_PATH                                                                                                     \
            TEST_MANIFEST_PATH_DEFAULT

        // Write manifests for full backup
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F/" BACKUP_MANIFEST_FILE, TEST_MANIFEST_FULL_DB1,
            .comment = "valid manifest - full");
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, TEST_MANIFEST_FULL_DB1,
            .comment = "valid manifest copy - full");

        // Create valid diff backup for DB1
        #define TEST_MANIFEST_DIFF_DB1                                                                                             \
            TEST_MANIFEST_HEADER                                                                                                   \
            TEST_MANIFEST_DB_94                                                                                                    \
            TEST_MANIFEST_OPTION_ALL                                                                                               \
            TEST_MANIFEST_TARGET                                                                                                   \
            TEST_MANIFEST_DB                                                                                                       \
            "\n"                                                                                                                   \
            "[target:file]\n"                                                                                                      \
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"reference\":\"20181119-152900F\""     \
                ",\"size\":4,\"timestamp\":1565282114}\n"                                                                          \
            TEST_MANIFEST_FILE_DEFAULT                                                                                             \
            TEST_MANIFEST_LINK                                                                                                     \
            TEST_MANIFEST_LINK_DEFAULT                                                                                             \
            TEST_MANIFEST_PATH                                                                                                     \
            TEST_MANIFEST_PATH_DEFAULT

        // Write manifests for diff backup
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F_20181119-152909D/" BACKUP_MANIFEST_FILE,
            TEST_MANIFEST_DIFF_DB1, .comment = "valid manifest - diff");
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F_20181119-152909D/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, TEST_MANIFEST_DIFF_DB1, .comment = "valid manifest copy - diff");

        // Put the file referenced by both backups into the full backup
        HRN_STORAGE_PUT_Z(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F/pg_data/PG_VERSION", fileContents);

        TEST_ERROR(cmdVerify(), RuntimeError, "2 fatal errors encountered, see log for details");

        // The error for the referenced file is logged twice because it is checked again by the second backup since the first backup
        // verification had not yet completed before the second backup verification began
        TEST_RESULT_LOG(
            "P00   WARN: no archives exist in the repo\n"
            "P01  ERROR: [028]: invalid checksum '20181119-152900F/pg_data/PG_VERSION'\n"
            "P01  ERROR: [028]: invalid checksum '20181119-152900F/pg_data/PG_VERSION'\n"
            "P00   INFO: Results:\n"
            "              archiveId: none found\n"
            "              backup: 20181119-152900F, status: invalid, total files checked: 1, total valid files: 0\n"
            "                missing: 0, checksum invalid: 1, size invalid: 0, other: 0\n"
            "              backup: 20181119-152900F_20181119-152909D, status: invalid, total files checked: 1, total valid files: 0"
            "\n"
            "                missing: 0, checksum invalid: 1, size invalid: 0, other: 0");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid backup, prior backup verification complete - referenced file not checked");

        // Set process max to 1 and add more files to check so first backup completes before second is checked
        hrnCfgArgRawZ(argList, cfgOptProcessMax, "1");
        HRN_CFG_LOAD(cfgCmdVerify, argList);

        String *manifestContent = strNewFmt(
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_DB_94
            TEST_MANIFEST_OPTION_ALL
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            "pg_data/base/1/555_init={\"checksum\":\"%s\",\"size\":1,\"timestamp\":1565282114}\n"
            "pg_data/base/1/555_init.1={\"size\":0,\"timestamp\":1565282114}\n"
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT,
            strZ(fileChecksum));

        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F/" BACKUP_MANIFEST_FILE, strZ(manifestContent),
            .comment = "valid manifest - full");
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(manifestContent),
            .comment = "valid manifest copy - full");

        HRN_STORAGE_PUT_Z(
            storageRepoWrite(),
            STORAGE_REPO_BACKUP "/20181119-152900F/pg_data/base/1/555_init", fileContents, .comment = "invalid size");

        // Diff manifest
        manifestContent = strNewZ(
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_DB_94
            TEST_MANIFEST_OPTION_ALL
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            "\n"
            "[target:file]\n"
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"reference\":\"20181119-152900F\""
                ",\"size\":4,\"timestamp\":1565282114}\n"
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT);

        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F_20181119-152909D/" BACKUP_MANIFEST_FILE,
            strZ(manifestContent), .comment = "valid manifest - diff");
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F_20181119-152909D/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
            strZ(manifestContent), .comment = "valid manifest copy - diff");

        // Create valid full backup and valid diff backup
        manifestContent = strNewFmt(
                TEST_MANIFEST_HEADER
                "\n"
                "[backup:db]\n"
                TEST_BACKUP_DB2_11
                TEST_MANIFEST_OPTION_ALL
                TEST_MANIFEST_TARGET
                TEST_MANIFEST_DB
                "\n"
                "[target:file]\n"
                "pg_data/validfile={\"bni\":1,\"checksum\":\"%s\",\"size\":%u,\"timestamp\":1565282114}\n"
                TEST_MANIFEST_FILE_DEFAULT
                TEST_MANIFEST_LINK
                TEST_MANIFEST_LINK_DEFAULT
                TEST_MANIFEST_PATH
                TEST_MANIFEST_PATH_DEFAULT,
                strZ(fileChecksum), (unsigned int)fileSize);

        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20201119-163000F/" BACKUP_MANIFEST_FILE, strZ(manifestContent),
            .comment = "valid manifest - full");
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20201119-163000F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(manifestContent),
            .comment = "valid manifest copy - full");

        HRN_STORAGE_PUT_Z(
            storageRepoWrite(), STORAGE_REPO_BACKUP  "/20201119-163000F/bundle/1", fileContents, .comment = "valid file");

        // Create WAL file with just header info and small WAL size
        Buffer *walBuffer = bufNew((size_t)(1024 * 1024));
        bufUsedSet(walBuffer, bufSize(walBuffer));
        memset(bufPtr(walBuffer), 0, bufSize(walBuffer));
        hrnPgWalToBuffer((PgWal){.version = PG_VERSION_11, .size = 1024 * 1024}, walBuffer);
        const char *walBufferSha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer)));

        HRN_STORAGE_PUT(
            storageRepoWrite(),
            strZ(strNewFmt(STORAGE_REPO_ARCHIVE "/11-2/0000000200000000/000000020000000000000001-%s", walBufferSha1)), walBuffer,
            .comment = "valid WAL");

        TEST_ERROR(cmdVerify(), RuntimeError, "3 fatal errors encountered, see log for details");

        TEST_RESULT_LOG(
            "P01  ERROR: [028]: invalid checksum '20181119-152900F/pg_data/PG_VERSION'\n"
            "P01  ERROR: [028]: invalid size '20181119-152900F/pg_data/base/1/555_init'\n"
            "P01  ERROR: [028]: file missing '20181119-152900F/pg_data/base/1/555_init.1'\n"
            "P00   INFO: Results:\n"
            "              archiveId: 11-2, total WAL checked: 1, total valid WAL: 1\n"
            "                missing: 0, checksum invalid: 0, size invalid: 0, other: 0\n"
            "              backup: 20181119-152900F, status: invalid, total files checked: 3, total valid files: 0\n"
            "                missing: 1, checksum invalid: 1, size invalid: 1, other: 0\n"
            "              backup: 20181119-152900F_20181119-152909D, status: invalid, total files checked: 1, total valid files: 0\n"
            "                missing: 0, checksum invalid: 1, size invalid: 0, other: 0\n"
            "              backup: 20201119-163000F, status: valid, total files checked: 1, total valid files: 1\n"
            "                missing: 0, checksum invalid: 0, size invalid: 0, other: 0");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
