/***********************************************************************************************************************************
Test Stanza Commands
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPq.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "postgres/interface.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    String *stanza = strNew("db");
    String *backupStanzaPath = strNewFmt("repo/backup/%s", strZ(stanza));
    String *backupInfoFileName = strNewFmt("%s/" INFO_BACKUP_FILE, strZ(backupStanzaPath));
    String *backupInfoFileNameCopy = strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName));
    String *archiveStanzaPath = strNewFmt("repo/archive/%s", strZ(stanza));
    String *archiveInfoFileName = strNewFmt("%s/" INFO_ARCHIVE_FILE, strZ(archiveStanzaPath));
    String *archiveInfoFileNameCopy = strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileName));

    StringList *argListBase = strLstNew();
    strLstAdd(argListBase, strNewFmt("--stanza=%s", strZ(stanza)));
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));

    const char *fileContents = "acefile";
    uint64_t fileSize = 7;
    const String *fileChecksum = STRDEF("d1cd8a7d11daa26814b93eb604e1d49ab4b43770");

    #define TEST_BACKUP_DB1_94                                                                                                     \
        "db-catalog-version=201409291\n"                                                                                           \
        "db-control-version=942\n"                                                                                                 \
        "db-id=1\n"                                                                                                                \
        "db-system-id=6625592122879095702\n"                                                                                       \
        "db-version=\"9.4\"\n"

    #define TEST_BACKUP_DB2_11                                                                                                     \
        "db-catalog-version=201707211\n"                                                                                           \
        "db-control-version=1100\n"                                                                                                \
        "db-id=2\n"                                                                                                                \
        "db-system-id=6626363367545678089\n"                                                                                       \
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
        "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"                     \
            "\"db-version\":\"9.4\"}"

    #define TEST_BACKUP_DB2_HISTORY                                                                                                \
        "2={\"db-catalog-version\":201707211,\"db-control-version\":1100,\"db-system-id\":6626363367545678089,"                    \
            "\"db-version\":\"11\"}"

    String *backupInfoContent = strNewFmt(
        "[backup:current]\n"
        TEST_BACKUP_DB1_CURRENT_FULL1
        "\n"
        "[db]\n"
        TEST_BACKUP_DB1_94
        "\n"
        "[db:history]\n"
        TEST_BACKUP_DB1_HISTORY
        );

    const Buffer *backupInfoBase = harnessInfoChecksumZ(strZ(backupInfoContent));

    String *backupInfoMultiHistoryContent = strNewFmt(
        "[backup:current]\n"
        TEST_BACKUP_DB1_CURRENT_FULL1
        TEST_BACKUP_DB1_CURRENT_FULL2
        TEST_BACKUP_DB1_CURRENT_FULL3
        "\n"
        "[db]\n"
        TEST_BACKUP_DB2_11
        "\n"
        "[db:history]\n"
        TEST_BACKUP_DB1_HISTORY
        "\n"
        TEST_BACKUP_DB2_HISTORY
        );

    const Buffer *backupInfoMultiHistoryBase = harnessInfoChecksumZ(strZ(backupInfoMultiHistoryContent));

    String *archiveInfoContent = strNewFmt(
        "[db]\n"
        "db-id=1\n"
        "db-system-id=6625592122879095702\n"
        "db-version=\"9.4\"\n"
        "\n"
        "[db:history]\n"
        "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}");

    const Buffer *archiveInfoBase = harnessInfoChecksumZ(strZ(archiveInfoContent));

    String *archiveInfoMultiHistoryContent = strNewFmt(
        "[db]\n"
        "db-id=2\n"
        "db-system-id=6626363367545678089\n"
        "db-version=\"11\"\n"
        "\n"
        "[db:history]\n"
        "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
        "2={\"db-id\":6626363367545678089,\"db-version\":\"11\"}");

    const Buffer *archiveInfoMultiHistoryBase = harnessInfoChecksumZ(strZ(archiveInfoMultiHistoryContent));

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
        "db-system-id=6625592122879095702\n"                                                                                       \
        "db-version=\"9.2\"\n"

    #define TEST_MANIFEST_DB_94                                                                                                    \
        "\n"                                                                                                                       \
        "[backup:db]\n"                                                                                                            \
        "db-catalog-version=201409291\n"                                                                                           \
        "db-control-version=942\n"                                                                                                 \
        "db-id=1\n"                                                                                                                \
        "db-system-id=6625592122879095702\n"                                                                                       \
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
        "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true"                            \
            ",\"size\":4,\"timestamp\":1565282114}\n"

    #define TEST_MANIFEST_FILE_DEFAULT                                                                                             \
        "\n"                                                                                                                       \
        "[target:file:default]\n"                                                                                                  \
        "group=\"group1\"\n"                                                                                                       \
        "master=false\n"                                                                                                           \
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

    // *****************************************************************************************************************************
    if (testBegin("verifyManifestFile()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdVerify, argList);

        const Buffer *contentLoad = harnessInfoChecksumZ
        (
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
            TEST_MANIFEST_PATH_DEFAULT
        );

        Manifest *manifest = NULL;
        String *backupLabel = strNew("20181119-152138F");
        String *manifestFile = strNewFmt("%s/%s/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath), strZ(backupLabel));
        String *manifestFileCopy = strNewFmt("%s" INFO_COPY_EXT, strZ(manifestFile));
        unsigned int jobErrorTotal = 0;
        VerifyBackupResult backupResult = {.backupLabel = strDup(backupLabel)};

        InfoArchive *archiveInfo = NULL;
        TEST_ASSIGN(archiveInfo, infoArchiveNewLoad(ioBufferReadNew(archiveInfoBase)), "archive.info");
        InfoPg *infoPg = infoArchivePg(archiveInfo);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("manifest.copy exists, no manifest main, manifest db version not in history, not current db");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFile), contentLoad), "write manifest db section mismatch");

        backupResult.status = backupValid;
        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, false, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_PTR(manifest, NULL, "manifest not set - pg version mismatch");
        TEST_RESULT_UINT(backupResult.status, backupInvalid, "manifest unusable - backup invalid");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: unable to open missing file '%s/%s/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT "' for read\n"
            "P00  ERROR: [028]: '%s' may not be recoverable - PG data (id 1, version 9.2, system-id 6625592122879095702) is not "
                "in the backup.info history, skipping",
            testPath(), strZ(backupStanzaPath), strZ(backupLabel), strZ(backupLabel))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("rerun test with db-system-id invalid and no main");

        contentLoad = harnessInfoChecksumZ
        (
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
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_VOID(storageRemoveP(storageTest, manifestFile), "remove main manifest");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileCopy), contentLoad), "write manifest copy invalid system-id");

        backupResult.status = backupValid;
        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, false, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_PTR(manifest, NULL, "manifest not set - pg system-id mismatch");
        TEST_RESULT_UINT(backupResult.status, backupInvalid, "manifest unusable - backup invalid");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: unable to open missing file '%s/%s/%s/" BACKUP_MANIFEST_FILE "' for read\n"
            "P00   WARN: %s/backup.manifest is missing or unusable, using copy\n"
            "P00  ERROR: [028]: '%s' may not be recoverable - PG data (id 1, version 9.4, system-id 0) is not "
                "in the backup.info history, skipping",
            testPath(), strZ(backupStanzaPath), strZ(backupLabel), strZ(backupLabel), strZ(backupLabel))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("rerun copy test with db-id invalid");

        contentLoad = harnessInfoChecksumZ
        (
            TEST_MANIFEST_HEADER
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=0\n"
            "db-system-id=6625592122879095702\n"
            "db-version=\"9.4\"\n"
            TEST_MANIFEST_OPTION_ALL
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileCopy), contentLoad), "write manifest copy invalid db-id");

        backupResult.status = backupValid;
        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, false, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_PTR(manifest, NULL, "manifest not set - pg db-id mismatch");
        TEST_RESULT_UINT(backupResult.status, backupInvalid, "manifest unusable - backup invalid");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: unable to open missing file '%s/%s/%s/" BACKUP_MANIFEST_FILE "' for read\n"
            "P00   WARN: %s/backup.manifest is missing or unusable, using copy\n"
            "P00  ERROR: [028]: '%s' may not be recoverable - PG data (id 0, version 9.4, system-id 6625592122879095702) is not "
                "in the backup.info history, skipping",
            testPath(), strZ(backupStanzaPath), strZ(backupLabel), strZ(backupLabel), strZ(backupLabel))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing main manifest, errored copy");

        backupResult.status = backupValid;
        contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-checksum=\"BOGUS\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.28\"\n");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileCopy), contentLoad), "write invalid manifest copy");
        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, false, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_UINT(backupResult.status, backupInvalid, "manifest unusable - backup invalid");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: unable to open missing file '%s/%s/%s/" BACKUP_MANIFEST_FILE "' for read\n"
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS' "
            STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, testPath(), strZ(backupStanzaPath), strZ(backupLabel),
            strZ(backupLabel))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("current backup true");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFile), contentLoad), "write invalid manifest");

        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, true, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_PTR(manifest, NULL, "manifest not set");
        TEST_RESULT_UINT(backupResult.status, backupInvalid, "manifest unusable - backup invalid");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS' "
            STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE "\n"
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS' "
            STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(backupLabel), strZ(backupLabel))));

        // Write a valid manifest with a manifest copy that is invalid
        contentLoad = harnessInfoChecksumZ
        (
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
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFile), contentLoad), "write valid manifest");

        backupResult.status = backupValid;
        TEST_ASSIGN(manifest, verifyManifestFile(&backupResult, NULL, true, infoPg, &jobErrorTotal), "verify manifest");
        TEST_RESULT_PTR_NE(manifest, NULL, "manifest set");
        TEST_RESULT_UINT(backupResult.status, backupValid, "manifest usable");
        harnessLogResult(strZ(strNewFmt("P00   WARN: backup '%s' manifest.copy does not match manifest", strZ(backupLabel))));
    }

    // *****************************************************************************************************************************
    if (testBegin("verifyCreateArchiveIdRange()"))
    {
        VerifyWalRange *walRangeResult = NULL;
        unsigned int errTotal = 0;
        StringList *walFileList = strLstNew();

        VerifyArchiveResult archiveResult =
        {
            .archiveId = strNew("9.4-1"),
            .walRangeList = lstNewP(sizeof(VerifyWalRange), .comparator =  lstComparatorStr),
        };
        List *archiveIdResultList = lstNewP(sizeof(VerifyArchiveResult), .comparator =  archiveIdComparator);
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
        harnessLogResult("P00  ERROR: [028]: duplicate WAL '000000020000000200000000' for '9.4-1' exists, skipping");

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
        harnessLogResult(
            "P00  ERROR: [028]: duplicate WAL '000000020000000100000000' for '9.4-1' exists, skipping\n"
            "P00  ERROR: [028]: duplicate WAL '000000020000000200000001' for '9.4-1' exists, skipping");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("FF Wal skipped <= 9.2, duplicates in middle of list removed");

        // Clear the range lists and rerun the test with PG_VERSION_92 to ensure FF is reported as an error
        lstClear(archiveIdResult->walRangeList);
        errTotal = 0;
        archiveIdResult->archiveId = strNew("9.2-1");
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

        harnessLogResult(
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
        archiveIdResult->archiveId = strNew("9.6-1");
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
        TEST_ASSIGN(backupInfo, infoBackupNewLoad(ioBufferReadNew(backupInfoMultiHistoryBase)), "backup.info multi-history");

        // Create archive.info - history mismatch
        InfoArchive *archiveInfo = NULL;
        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"11\"}"))), "archive.info missing history");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");

        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6625592122879095777,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"11\"}"))), "archive.info history system id mismatch");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");

        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6625592122879095702,\"db-version\":\"9.5\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"11\"}"))), "archive.info history version mismatch");

        TEST_ERROR(
            verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo)), FormatError,
            "archive and backup history lists do not match");


        TEST_ASSIGN(
            archiveInfo, infoArchiveNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"11\"\n"
                "\n"
                "[db:history]\n"
                "3={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"11\"}"))), "archive.info history id mismatch");

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
        TEST_ASSIGN(backupInfo, infoBackupNewLoad(ioBufferReadNew(backupInfoMultiHistoryBase)), "backup.info");
        TEST_ASSIGN(archiveInfo, infoArchiveNewLoad(ioBufferReadNew(archiveInfoMultiHistoryBase)), "archive.info");
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
        harnessLogResult("P00  ERROR: [044]: archiveIds '12-3' are not in the archive.info history list");

        errTotal = 0;
        strLstAddZ(archiveIdList, "13-4");
        TEST_RESULT_STR_Z(
            verifySetBackupCheckArchive(backupList, backupInfo, archiveIdList, pgHistory, &errTotal),
            "20181119-153000F", "test multiple archiveIds on disk not in archive.info");
        TEST_RESULT_UINT(errTotal, 1, "error logged");
        harnessLogResult("P00  ERROR: [044]: archiveIds '12-3, 13-4' are not in the archive.info history list");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyLogInvalidResult() - missing file");

        TEST_RESULT_UINT(
            verifyLogInvalidResult(STORAGE_REPO_ARCHIVE_STR, verifyFileMissing, 0, strNew("missingfilename")),
            0, "file missing message");
        harnessLogResult("P00   WARN: file missing 'missingfilename'");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyRender() - missing file, empty invalidList");

        List *archiveIdResultList = lstNewP(sizeof(VerifyArchiveResult), .comparator =  archiveIdComparator);
        List *backupResultList = lstNewP(sizeof(VerifyBackupResult), .comparator = lstComparatorStr);

        VerifyArchiveResult archiveIdResult =
        {
            .archiveId = strNew("9.6-1"),
            .totalWalFile = 1,
            .walRangeList = lstNewP(sizeof(VerifyWalRange), .comparator =  lstComparatorStr),
        };
        VerifyWalRange walRange =
        {
            .start = strNew("0"),
            .stop = strNew("2"),
            .invalidFileList = lstNewP(sizeof(VerifyInvalidFile), .comparator =  lstComparatorStr),
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
            .fileName = strNew("file"),
            .reason = verifyFileMissing,
        };
        lstAdd(walRange.invalidFileList, &invalidFile);

        VerifyBackupResult backupResult =
        {
            .backupLabel = strNew("test-backup-label"),
            .status = backupInvalid,
            .totalFileVerify = 1,
            .invalidFileList = lstNewP(sizeof(VerifyInvalidFile), .comparator =  lstComparatorStr),
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
            verifyAddInvalidWalFile(archiveIdResult.walRangeList, verifyFileMissing, strNew("test"), strNew("3")), "coverage test");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify() - info files"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdVerify, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info invalid checksum, neither backup copy nor archive infos exist");

        const Buffer *contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-checksum=\"BOGUS\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.28\"\n");

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, backupInfoFileName), contentLoad), "write invalid backup.info");
        TEST_ERROR(cmdVerify(), RuntimeError, "2 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS' "
                "<REPO:BACKUP>/backup.info\n"
            "P00   WARN: unable to open missing file '%s/%s/backup.info.copy' for read\n"
            "P00  ERROR: [029]: No usable backup.info file\n"
            "P00   WARN: unable to open missing file '%s/%s/archive.info' for read\n"
            "P00   WARN: unable to open missing file '%s/%s/archive.info.copy' for read\n"
            "P00  ERROR: [029]: No usable archive.info file", testPath(), strZ(backupStanzaPath), testPath(),
            strZ(archiveStanzaPath), testPath(), strZ(archiveStanzaPath))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info invalid checksum, backup.info.copy valid, archive.info not exist, archive copy checksum invalid");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileNameCopy), contentLoad), "write invalid archive.info.copy");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileNameCopy), backupInfoBase), "write valid backup.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS'"
                " <REPO:BACKUP>/backup.info\n"
            "P00   WARN: unable to open missing file '%s/%s/archive.info' for read\n"
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS'"
                " <REPO:ARCHIVE>/archive.info.copy\n"
            "P00  ERROR: [029]: No usable archive.info file", testPath(), strZ(archiveStanzaPath))));


        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy valid but checksum mismatch, archive.info checksum invalid, archive.info copy valid");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileName), backupInfoMultiHistoryBase), "write valid backup.info");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileName), contentLoad), "write invalid archive.info");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileNameCopy), archiveInfoBase), "write valid archive.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        harnessLogResult(
            "P00   WARN: backup.info.copy does not match backup.info\n"
            "P00   WARN: invalid checksum, actual 'e056f784a995841fd4e2802b809299b8db6803a2' but expected 'BOGUS'"
                " <REPO:ARCHIVE>/archive.info\n"
            "P00  ERROR: [029]: backup info file and archive info file do not match\n"
            "            archive: id = 1, version = 9.4, system-id = 6625592122879095702\n"
            "            backup : id = 2, version = 11, system-id = 6626363367545678089\n"
            "            HINT: this may be a symptom of repository corruption!");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy valid and checksums match, archive.info and copy valid, but checksum mismatch");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileNameCopy), backupInfoMultiHistoryBase),
            "write valid backup.info.copy");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileName), archiveInfoMultiHistoryBase),
            "write valid archive.info");
        TEST_RESULT_VOID(cmdVerify(), "usable backup and archive info files");
        harnessLogResult(
            "P00   WARN: archive.info.copy does not match archive.info\n"
            "P00   WARN: no archives or backups exist in the repo");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info valid, copy invalid, archive.info valid, copy invalid");

        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileNameCopy), "remove backup.info.copy");
        TEST_RESULT_VOID(storageRemoveP(storageTest, archiveInfoFileNameCopy), "remove archive.info.copy");
        TEST_RESULT_VOID(cmdVerify(), "usable backup and archive info files");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: unable to open missing file '%s/%s/backup.info.copy' for read\n"
            "P00   WARN: unable to open missing file '%s/%s/archive.info.copy' for read\n"
            "P00   WARN: no archives or backups exist in the repo", testPath(), strZ(backupStanzaPath), testPath(),
            strZ(archiveStanzaPath))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info and copy missing, archive.info and copy valid");

        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileName), "remove backup.info");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileNameCopy), archiveInfoMultiHistoryBase),
            "write valid and matching archive.info.copy");
        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   WARN: unable to open missing file '%s/%s/backup.info' for read\n"
            "P00   WARN: unable to open missing file '%s/%s/backup.info.copy' for read\n"
            "P00  ERROR: [029]: No usable backup.info file", testPath(), strZ(backupStanzaPath), testPath(),
            strZ(backupStanzaPath))));
    }

    // *****************************************************************************************************************************
    if (testBegin("verifyFile(), verifyProtocol()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdVerify, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyFile()");

        String *filePathName =  strNewFmt(STORAGE_REPO_ARCHIVE "/testfile");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageRepoWrite(), filePathName), BUFSTRDEF("")), "put zero-sized file");
        TEST_RESULT_UINT(verifyFile(filePathName, STRDEF(HASH_TYPE_SHA1_ZERO), 0, NULL), verifyOk, "file ok");

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageRepoWrite(), filePathName), BUFSTRZ(fileContents)), "put file");

        TEST_RESULT_UINT(verifyFile(filePathName, fileChecksum, 0, NULL), verifySizeInvalid, "file size invalid");
        TEST_RESULT_UINT(
            verifyFile(
                strNewFmt(STORAGE_REPO_ARCHIVE "/missingFile"), fileChecksum, 0, NULL), verifyFileMissing, "file missing");

        // Create a compressed encrypted repo file
        filePathName = strNew(STORAGE_REPO_BACKUP "/testfile.gz");
        StorageWrite *write = storageNewWriteP(storageRepoWrite(), filePathName);
        IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(write));
        ioFilterGroupAdd(filterGroup, compressFilter(compressTypeGz, 3));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRDEF("pass"), NULL));
        TEST_RESULT_VOID(storagePutP(write, BUFSTRZ(fileContents)), "write encrypted, compressed file");

        TEST_RESULT_UINT(
            verifyFile(filePathName, fileChecksum, fileSize, strNew("pass")), verifyOk, "file encrypted compressed ok");
        TEST_RESULT_UINT(
            verifyFile(
                filePathName, strNew("badchecksum"), fileSize, strNew("pass")), verifyChecksumMismatch,
                "file encrypted compressed checksum mismatch");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verifyProtocol()");

        // Start a protocol server to test the protocol directly
        Buffer *serverWrite = bufNew(8192);
        IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
        ioWriteOpen(serverWriteIo);
        ProtocolServer *server = protocolServerNew(strNew("test"), strNew("test"), ioBufferReadNew(bufNew(0)), serverWriteIo);
        bufUsedSet(serverWrite, 0);

        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(filePathName));
        varLstAdd(paramList, varNewStr(fileChecksum));
        varLstAdd(paramList, varNewUInt64(fileSize));
        varLstAdd(paramList, varNewStrZ("pass"));

        TEST_RESULT_VOID(verifyProtocol(paramList, server), "protocol verify file");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{\"out\":0}\n", "check result");
        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify(), verifyProcess() - errors"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        // Load Parameters with multi-repo
        StringList *argList = strLstDup(argListBase);
        hrnCfgArgKeyRawFmt(argList, cfgOptRepoPath, 4, "%s/repo4", testPath());
        harnessCfgLoad(cfgCmdVerify, argList);

        // Store valid archive/backup info files
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileName), archiveInfoMultiHistoryBase),
            "write valid archive.info");
        storageCopy(storageNewReadP(storageTest, archiveInfoFileName), storageNewWriteP(storageTest, archiveInfoFileNameCopy));

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileName),
                harnessInfoChecksumZ(
                    "[db]\n"
                    TEST_BACKUP_DB2_11
                    "\n"
                    "[db:history]\n"
                    TEST_BACKUP_DB1_HISTORY
                    "\n"
                    TEST_BACKUP_DB2_HISTORY
                    )),
            "put backup.info files - no current backups");
        storageCopy(storageNewReadP(storageTest, backupInfoFileName), storageNewWriteP(storageTest, backupInfoFileNameCopy));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid info files, WAL files present, no backups");

        // Create WAL file with just header info and small WAL size
        Buffer *walBuffer = bufNew((size_t)(1024 * 1024));
        bufUsedSet(walBuffer, bufSize(walBuffer));
        memset(bufPtr(walBuffer), 0, bufSize(walBuffer));
        pgWalTestToBuffer(
            (PgWal){.version = PG_VERSION_11, .systemId = 6626363367545678089, .size = 1024 * 1024}, walBuffer);
        const char *walBufferSha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer)));

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000007/000000020000000700000FFE-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write valid WAL");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000007/000000020000000700000FFE-bad817043007aa2100c44c712bcb456db705dab9",
                    strZ(archiveStanzaPath))),
                walBuffer),
            "write duplicate WAL");

        // Set log detail level to capture ranges (there should be none)
        harnessLogLevelSet(logLevelDetail);

        TEST_ERROR(cmdVerify(), RuntimeError, "1 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNew(
                "P00   WARN: no backups exist in the repo\n"
                "P00  ERROR: [028]: duplicate WAL '000000020000000700000FFE' for '11-2' exists, skipping\n"
                "P00   WARN: path '11-2/0000000200000007' does not contain any valid WAL to be processed\n"
                "P00   INFO: Results:\n"
                "              archiveId: 11-2, total WAL checked: 2, total valid WAL: 0\n"
                "                missing: 0, checksum invalid: 0, size invalid: 0, other: 0\n"
                "              backup: none found")));

        harnessLogLevelReset();

        TEST_RESULT_VOID(
            storageRemoveP(
                storageTest, strNewFmt("%s/11-2/0000000200000007/000000020000000700000FFE-bad817043007aa2100c44c712bcb456db705dab9",
                strZ(archiveStanzaPath))),
            "remove duplicate WAL");

        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("%s/9.4-1", strZ(archiveStanzaPath))),
            "create empty path for old archiveId");

        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("%s/11-2/0000000100000000", strZ(archiveStanzaPath))),
            "create empty timeline path");

        StorageWrite *write = storageNewWriteP(
            storageTest,
            strNewFmt("%s/11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz",
            strZ(archiveStanzaPath)));
        ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(write)), compressFilter(compressTypeGz, 3));
        TEST_RESULT_VOID(storagePutP(write, walBuffer), "write first WAL compressed - but checksum failure");

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000007/000000020000000700000FFF-%s", strZ(archiveStanzaPath),
                    strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, BUFSTRDEF("invalidsize")))))),
                BUFSTRDEF("invalidsize")),
            "write WAL - invalid size");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000008/000000020000000800000000-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL - continue range");

        // Set log detail level to capture ranges
        harnessLogLevelSet(logLevelDetail);

        // Test verifyProcess directly
        unsigned int errorTotal = 0;
        TEST_RESULT_STR_Z(
            verifyProcess(&errorTotal),
            strZ(strNew(
                "Results:\n"
                "  archiveId: 9.4-1, total WAL checked: 0, total valid WAL: 0\n"
                "  archiveId: 11-2, total WAL checked: 4, total valid WAL: 2\n"
                "    missing: 0, checksum invalid: 1, size invalid: 1, other: 0\n"
                "  backup: none found")),
            "verifyProcess() results");
        TEST_RESULT_UINT(errorTotal, 2, "errors");
        harnessLogResult(
            strZ(strNew(
                "P00   WARN: no backups exist in the repo\n"
                "P00   WARN: archive path '9.4-1' is empty\n"
                "P00   WARN: path '11-2/0000000100000000' does not contain any valid WAL to be processed\n"
                "P01  ERROR: [028]: invalid checksum "
                    "'11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz'\n"
                "P01  ERROR: [028]: invalid size "
                    "'11-2/0000000200000007/000000020000000700000FFF-ee161f898c9012dd0c28b3fd1e7140b9cf411306'\n"
                "P00 DETAIL: archiveId: 11-2, wal start: 000000020000000700000FFD, wal stop: 000000020000000800000000")));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid info files, start next timeline");

        // Load Parameters - single default repo
        argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdVerify, argList);

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000008/000000020000000800000002-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL - starts next range");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000300000000/000000030000000000000000-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL - starts next timeline");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000300000000/000000030000000000000001-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write WAL - end next timeline");

        // Set log level to errors only
        harnessLogLevelSet(logLevelError);

        TEST_ERROR(cmdVerify(), RuntimeError, "2 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNew(
                "P01  ERROR: [028]: invalid checksum "
                    "'11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz'\n"
                "P01  ERROR: [028]: invalid size "
                    "'11-2/0000000200000007/000000020000000700000FFF-ee161f898c9012dd0c28b3fd1e7140b9cf411306'")));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid info files - various archive/backup errors");

        // Load Parameters - single non-default repo
        argList = strLstNew();
        hrnCfgArgKeyRawFmt(argList, cfgOptRepoPath, 2, "%s/repo", testPath());
        hrnCfgArgRawFmt(argList, cfgOptStanza, "%s", strZ(stanza));
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        harnessCfgLoad(cfgCmdVerify, argList);

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000008/000000020000000800000003-656817043007aa2100c44c712bcb456db705dab9",
                        strZ(archiveStanzaPath)),
                .modeFile = 0200),
                walBuffer),
            "write WAL - file not readable");

        String *backupLabelPriorNoManifest = strNew("20181119-152800F");
        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("%s/%s", strZ(backupStanzaPath), strZ(backupLabelPriorNoManifest))),
            "prior backup path missing manifests");

        String *backupLabelManifestNoTargetFile = strNew("20181119-152810F");
        const Buffer *contentLoad = harnessInfoChecksumZ
        (
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_DB_94
            TEST_MANIFEST_OPTION_ALL
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
        );

        String *manifestFileNoTarget = strNewFmt(
            "%s/%s/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath), strZ(backupLabelManifestNoTargetFile));
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileNoTarget), contentLoad), "write manifest without target files");

        // Create full backup with files
        String *backupLabel = strNew("20181119-152900F");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/%s/pg_data/PG_VERSION", strZ(backupStanzaPath),
            strZ(backupLabel))), BUFSTRDEF("BOGUS")), "put checksum-error backup file");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/%s/pg_data/testzero", strZ(backupStanzaPath),
                strZ(backupLabel))), BUFSTRDEF("")), "put zero-size backup file");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/%s/pg_data/testvalid", strZ(backupStanzaPath),
            strZ(backupLabel))), BUFSTRZ(fileContents)), "put valid file");

        contentLoad = harnessInfoChecksumZ
        (
            strZ(strNewFmt(
                "[backup]\n"
                "backup-label=\"%s\"\n"
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
                "pg_data/testvalid={\"checksum\":\"%s\",\"master\":true,\"size\":7,\"timestamp\":1565282114}\n"
                "pg_data/testzero={\"repo-size\":20,\"size\":0,\"timestamp\":1601405663}\n"
                TEST_MANIFEST_FILE_DEFAULT
                TEST_MANIFEST_LINK
                TEST_MANIFEST_LINK_DEFAULT
                TEST_MANIFEST_PATH
                TEST_MANIFEST_PATH_DEFAULT,
            strZ(backupLabel), strZ(fileChecksum)))
        );

        // Write manifests for full backup
        String *manifestFile = strNewFmt("%s/%s/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath), strZ(backupLabel));
        String *manifestFileCopy = strNewFmt("%s" INFO_COPY_EXT, strZ(manifestFile));
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFile), contentLoad), "write valid manifest");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileCopy), contentLoad),
            "write valid manifest copy");

        // Create a manifest for the dependent that has references
        String *backupLabelDependent = strNew("20181119-152900F_20181119-152909D");

        // Create an unprocessed backup label with a file that will be referenced in this manifest
        String *unprocessedBackup = strNew("UNPROCESSEDBACKUP");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/%s/pg_data/testother", strZ(backupStanzaPath),
            strZ(unprocessedBackup)), .modeFile = 0200), BUFSTRZ(fileContents)), "put unreadable file to unprocessed backup");

        contentLoad = harnessInfoChecksumZ
        (
            strZ(strNewFmt(
                "[backup]\n"
                "backup-label=\"%s\"\n"
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
                "pg_data/PG_VERSION="
                    "{\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true,\"reference\":\"%s\",\"size\":4,"
                    "\"timestamp\":1565282114}\n"
                "pg_data/testfile={\"checksum\":\"%s\",\"master\":true,\"reference\":\"%s\",\"size\":7,\"timestamp\":1565282114}\n"
                "pg_data/testfile2={\"checksum\":\"%s\",\"master\":true,\"size\":7,\"timestamp\":1565282114}\n"
                "pg_data/testmissing="
                    "{\"checksum\":\"123473f470864e067ee3a22e64b47b0a1c356abc\",\"size\":7,\"timestamp\":1565282114}\n"
                "pg_data/testother={\"checksum\":\"%s\",\"master\":true,\"reference\":\"%s\",\"size\":7,\"timestamp\":1565282114}\n"
                TEST_MANIFEST_FILE_DEFAULT
                TEST_MANIFEST_LINK
                TEST_MANIFEST_LINK_DEFAULT
                TEST_MANIFEST_PATH
                TEST_MANIFEST_PATH_DEFAULT,
            strZ(backupLabelDependent), strZ(backupLabel), strZ(fileChecksum), strZ(backupLabel), strZ(fileChecksum),
            strZ(fileChecksum), strZ(unprocessedBackup)))
        );

        // Write manifests for dependent backup
        manifestFile = strNewFmt("%s/%s/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath), strZ(backupLabelDependent));
        manifestFileCopy = strNewFmt("%s" INFO_COPY_EXT, strZ(manifestFile));

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFile), contentLoad), "write manifest to dependent");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileCopy), contentLoad), "write manifest copy to dependent");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/%s/pg_data/testfile2", strZ(backupStanzaPath),
            strZ(backupLabelDependent))), BUFSTRZ(fileContents)), "put valid file to dependent");

        // Create in-progress backup
        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("%s/%s", strZ(backupStanzaPath), "20181119-153000F")),
            "create empty backup path for newest backup so in-progress");

        // Set log level to capture ranges
        harnessLogLevelSet(logLevelDetail);

        TEST_ERROR(cmdVerify(), RuntimeError, "7 fatal errors encountered, see log for details");
        harnessLogResult(
            strZ(strNewFmt(
                "P00   WARN: archive path '9.4-1' is empty\n"
                "P00   WARN: path '11-2/0000000100000000' does not contain any valid WAL to be processed\n"
                "P01  ERROR: [028]: invalid checksum "
                    "'11-2/0000000200000007/000000020000000700000FFD-a6e1a64f0813352bc2e97f116a1800377e17d2e4.gz'\n"
                "P01  ERROR: [028]: invalid size "
                    "'11-2/0000000200000007/000000020000000700000FFF-ee161f898c9012dd0c28b3fd1e7140b9cf411306'\n"
                "P01  ERROR: [039]: invalid result "
                    "11-2/0000000200000008/000000020000000800000003-656817043007aa2100c44c712bcb456db705dab9: [41] raised from "
                    "local-1 protocol: unable to open file "
                    "'%s/%s/11-2/0000000200000008/000000020000000800000003-656817043007aa2100c44c712bcb456db705dab9' for read: "
                    "[13] Permission denied\n"
                "P00   WARN: unable to open missing file '%s/%s/20181119-152800F/backup.manifest' for read\n"
                "P00   WARN: unable to open missing file '%s/%s/20181119-152800F/backup.manifest.copy' for read\n"
                "P00   WARN: manifest missing for '20181119-152800F' - backup may have expired\n"
                "P00   WARN: unable to open missing file '%s/%s/20181119-152810F/backup.manifest.copy' for read\n"
                "P00  ERROR: [028]: backup '20181119-152810F' manifest does not contain any target files to verify\n"
                "P01  ERROR: [028]: invalid checksum '20181119-152900F/pg_data/PG_VERSION'\n"
                "P01  ERROR: [028]: file missing '20181119-152900F_20181119-152909D/pg_data/testmissing'\n"
                "P00   WARN: unable to open missing file '%s/%s/20181119-153000F/backup.manifest' for read\n"
                "P00   INFO: backup '20181119-153000F' appears to be in progress, skipping\n"
                "P01  ERROR: [039]: invalid result UNPROCESSEDBACKUP/pg_data/testother: [41] raised from local-1 protocol: unable "
                    "to open file '%s/%s/UNPROCESSEDBACKUP/pg_data/testother' for read: [13] Permission denied\n"
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
                "              backup: 20181119-153000F, status: in-progress, total files checked: 0, total valid files: 0",
                testPath(), strZ(archiveStanzaPath), testPath(), strZ(backupStanzaPath), testPath(), strZ(backupStanzaPath),
                testPath(), strZ(backupStanzaPath), testPath(), strZ(backupStanzaPath), testPath(), strZ(backupStanzaPath))));

        harnessLogLevelReset();
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdVerify, argList);

        // Backup labels
        String *backupLabelFull = strNew("20181119-152900F");
        String *backupLabelDiff = strNew("20181119-152900F_20181119-152909D");
        String *backupLabelFullDb2 = strNew("20201119-163000F");

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

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, archiveInfoFileName), archiveInfoMultiHistoryBase),
            "write archive.info");
        storageCopy(storageNewReadP(storageTest, archiveInfoFileName), storageNewWriteP(storageTest, archiveInfoFileNameCopy));

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, backupInfoFileName),
                harnessInfoChecksumZ(
                    "[backup:current]\n"
                    TEST_BACKUP_DB1_CURRENT_FULL3
                    TEST_BACKUP_DB1_CURRENT_FULL3_DIFF1
                    TEST_BACKUP_DB2_CURRENT_FULL1
                    "\n"
                    "[db]\n"
                    TEST_BACKUP_DB2_11
                    "\n"
                    "[db:history]\n"
                    TEST_BACKUP_DB1_HISTORY
                    "\n"
                    TEST_BACKUP_DB2_HISTORY
                    )),
            "write backup.info");
        storageCopy(storageNewReadP(storageTest, backupInfoFileName), storageNewWriteP(storageTest, backupInfoFileNameCopy));

        // Create valid full backup and valid diff backup for DB1
        const Buffer *contentLoad = harnessInfoChecksumZ
        (
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
            TEST_MANIFEST_PATH_DEFAULT
        );

        // Write manifests for full backup
        String *manifestFile = strNewFmt("%s/%s/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath), strZ(backupLabelFull));
        String *manifestFileCopy = strNewFmt("%s" INFO_COPY_EXT, strZ(manifestFile));
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFile), contentLoad), "write valid manifest - full");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileCopy), contentLoad), "write valid manifest copy - full");

        contentLoad = harnessInfoChecksumZ
        (
            strZ(strNewFmt(
                TEST_MANIFEST_HEADER
                TEST_MANIFEST_DB_94
                TEST_MANIFEST_OPTION_ALL
                TEST_MANIFEST_TARGET
                TEST_MANIFEST_DB
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION="
                    "{\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true,\"reference\":\"%s\",\"size\":4,"
                    "\"timestamp\":1565282114}\n"
                TEST_MANIFEST_FILE_DEFAULT
                TEST_MANIFEST_LINK
                TEST_MANIFEST_LINK_DEFAULT
                TEST_MANIFEST_PATH
                TEST_MANIFEST_PATH_DEFAULT,
            strZ(backupLabelFull)))
        );

        // Write manifests for diff backup
        String *manifestFileDiff = strNewFmt("%s/%s/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath), strZ(backupLabelDiff));
        String *manifestFileCopyDiff = strNewFmt("%s" INFO_COPY_EXT, strZ(manifestFileDiff));
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileDiff), contentLoad), "write valid manifest - diff");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileCopyDiff), contentLoad), "write valid manifest copy - diff");

        // Put the file referenced by both backups into the full backup
        String *filePathName = strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/PG_VERSION", strZ(backupLabelFull));
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageRepoWrite(), filePathName), BUFSTRZ(fileContents)), "put file");

        TEST_ERROR(cmdVerify(), RuntimeError, "2 fatal errors encountered, see log for details");

        // The error for the referenced file is logged twice because it is checked again by the second backup since the first backup
        // verification had not yet completed before the second backup verification began
        harnessLogResult(
            strZ(strNewFmt(
                "P00   WARN: no archives exist in the repo\n"
                "P01  ERROR: [028]: invalid checksum '%s/pg_data/PG_VERSION'\n"
                "P01  ERROR: [028]: invalid checksum '%s/pg_data/PG_VERSION'\n"
                "P00   INFO: Results:\n"
                "              archiveId: none found\n"
                "              backup: %s, status: invalid, total files checked: 1, total valid files: 0\n"
                "                missing: 0, checksum invalid: 1, size invalid: 0, other: 0\n"
                "              backup: %s, status: invalid, total files checked: 1, total valid files: 0\n"
                "                missing: 0, checksum invalid: 1, size invalid: 0, other: 0",
                strZ(backupLabelFull), strZ(backupLabelFull), strZ(backupLabelFull), strZ(backupLabelDiff))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("valid backup, prior backup verification complete - referenced file not checked");

        // Set process max to 1 and add more files to check so first backup completes before second is checked
        strLstAddZ(argList, "--process-max=1");
        harnessCfgLoad(cfgCmdVerify, argList);

        contentLoad = harnessInfoChecksumZ
        (
            strZ(strNewFmt(
                TEST_MANIFEST_HEADER
                TEST_MANIFEST_DB_94
                TEST_MANIFEST_OPTION_ALL
                TEST_MANIFEST_TARGET
                TEST_MANIFEST_DB
                TEST_MANIFEST_FILE
                "pg_data/base/1/555_init="
                    "{\"checksum\":\"%s\",\"master\":false,\"size\":1,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_init.1={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                TEST_MANIFEST_FILE_DEFAULT
                TEST_MANIFEST_LINK
                TEST_MANIFEST_LINK_DEFAULT
                TEST_MANIFEST_PATH
                TEST_MANIFEST_PATH_DEFAULT,
                strZ(fileChecksum)))
        );
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFile), contentLoad), "write valid manifest - full");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileCopy), contentLoad), "write valid manifest copy - full");
        filePathName = strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/base/1/555_init", strZ(backupLabelFull));
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageRepoWrite(), filePathName), BUFSTRZ(fileContents)),
            "put file - invalid size");

        contentLoad = harnessInfoChecksumZ
        (
            strZ(strNewFmt(
                TEST_MANIFEST_HEADER
                TEST_MANIFEST_DB_94
                TEST_MANIFEST_OPTION_ALL
                TEST_MANIFEST_TARGET
                TEST_MANIFEST_DB
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION="
                    "{\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true,\"reference\":\"%s\",\"size\":4,"
                    "\"timestamp\":1565282114}\n"
                TEST_MANIFEST_FILE_DEFAULT
                TEST_MANIFEST_LINK
                TEST_MANIFEST_LINK_DEFAULT
                TEST_MANIFEST_PATH
                TEST_MANIFEST_PATH_DEFAULT,
                strZ(backupLabelFull)))
        );
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileDiff), contentLoad), "write valid manifest - diff");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileCopyDiff), contentLoad), "write valid manifest copy - diff");

        // Create valid full backup and valid diff backup
        contentLoad = harnessInfoChecksumZ
        (
            strZ(strNewFmt(
                TEST_MANIFEST_HEADER
                "\n"
                "[backup:db]\n"
                TEST_BACKUP_DB2_11
                TEST_MANIFEST_OPTION_ALL
                TEST_MANIFEST_TARGET
                TEST_MANIFEST_DB
                "\n"
                "[target:file]\n"
                "pg_data/validfile={\"checksum\":\"%s\",\"master\":true,\"size\":%u,\"timestamp\":1565282114}\n"
                TEST_MANIFEST_FILE_DEFAULT
                TEST_MANIFEST_LINK
                TEST_MANIFEST_LINK_DEFAULT
                TEST_MANIFEST_PATH
                TEST_MANIFEST_PATH_DEFAULT,
                strZ(fileChecksum), (unsigned int)fileSize))
        );

        manifestFile = strNewFmt("%s/%s/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath), strZ(backupLabelFullDb2));
        manifestFileCopy = strNewFmt("%s" INFO_COPY_EXT, strZ(manifestFile));
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFile), contentLoad), "write valid manifest - full");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, manifestFileCopy), contentLoad), "write valid manifest copy - full");
        filePathName =  strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/validfile", strZ(backupLabelFullDb2));
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageRepoWrite(), filePathName), BUFSTRZ(fileContents)), "put valid file");

        // Create WAL file with just header info and small WAL size
        Buffer *walBuffer = bufNew((size_t)(1024 * 1024));
        bufUsedSet(walBuffer, bufSize(walBuffer));
        memset(bufPtr(walBuffer), 0, bufSize(walBuffer));
        pgWalTestToBuffer(
            (PgWal){.version = PG_VERSION_11, .systemId = 6626363367545678089, .size = 1024 * 1024}, walBuffer);
        const char *walBufferSha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer)));
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNewFmt("%s/11-2/0000000200000000/000000020000000000000001-%s", strZ(archiveStanzaPath), walBufferSha1)),
                walBuffer),
            "write valid WAL");

        TEST_ERROR(cmdVerify(), RuntimeError, "3 fatal errors encountered, see log for details");

        harnessLogResult(
            strZ(strNewFmt(
                "P01  ERROR: [028]: invalid checksum '%s/pg_data/PG_VERSION'\n"
                "P01  ERROR: [028]: invalid size '%s/pg_data/base/1/555_init'\n"
                "P01  ERROR: [028]: file missing '%s/pg_data/base/1/555_init.1'\n"
                "P00   INFO: Results:\n"
                "              archiveId: 11-2, total WAL checked: 1, total valid WAL: 1\n"
                "                missing: 0, checksum invalid: 0, size invalid: 0, other: 0\n"
                "              backup: %s, status: invalid, total files checked: 3, total valid files: 0\n"
                "                missing: 1, checksum invalid: 1, size invalid: 1, other: 0\n"
                "              backup: %s, status: invalid, total files checked: 1, total valid files: 0\n"
                "                missing: 0, checksum invalid: 1, size invalid: 0, other: 0\n"
                "              backup: %s, status: valid, total files checked: 1, total valid files: 1\n"
                "                missing: 0, checksum invalid: 0, size invalid: 0, other: 0",
                strZ(backupLabelFull), strZ(backupLabelFull), strZ(backupLabelFull), strZ(backupLabelFull),
                strZ(backupLabelDiff), strZ(backupLabelFullDb2))));
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
