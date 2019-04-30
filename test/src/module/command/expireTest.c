/***********************************************************************************************************************************
Test Command Control
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "storage/driver/posix/storage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storageDriverPosixInterface(
        storageDriverPosixNew(strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL));

    String *backupStanzaPath = strNew("repo/backup/db");
    String *backupInfoFileName = strNewFmt("%s/backup.info", strPtr(backupStanzaPath));
    StringList *argListBase = strLstNew();
    strLstAddZ(argListBase, "pgbackrest");
    strLstAddZ(argListBase, "--stanza=db");
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));
    strLstAddZ(argListBase, "expire");

    String *backupInfoBase = strNew(
             "[backup:current]\n"
            "20181119-152138F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","
            "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
            "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152800F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
            "\"backup-archive-start\":\"000000010000000000000006\",\"backup-archive-stop\":\"000000010000000000000006\","
            "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
            "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152800F_20181119-152152D={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000003\","
            "\"backup-archive-stop\":\"000000010000000000000003\",\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152800F\",\"backup-reference\":[\"20181119-152800F\"],"
            "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152800F_20181119-152155I={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000004\","
            "\"backup-archive-stop\":\"000000010000000000000004\",\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152800F_20181119-152152D\","
            "\"backup-reference\":[\"20181119-152800F\",\"20181119-152800F_20181119-152152D\"],"
            "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"incr\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152900F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
            "\"backup-archive-start\":\"000000010000000000000007\",\"backup-archive-stop\":\"000000010000000000000007\","
            "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
            "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152900F_20181119-152600D={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000008\","
            "\"backup-archive-stop\":\"000000010000000000000008\",\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152138F\",\"backup-reference\":[\"20181119-152900F\"],"
            "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6625592122879095702\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                "\"db-version\":\"9.4\"}");


    // // Create the repo directories
    // String *repoPath = strNewFmt("%s/repo", testPath());
    // String *archivePath = strNewFmt("%s/%s", strPtr(repoPath), "archive");
    // String *backupPath = strNewFmt("%s/%s", strPtr(repoPath), "backup");
    // String *stanza = strNew("db");
    // String *archiveStanzaPath = strNewFmt("%s/%s", strPtr(archivePath), strPtr(stanza));
    // String *backupStanzaPath = strNewFmt("%s/%s", strPtr(backupPath), strPtr(stanza));

//     // *****************************************************************************************************************************
//     if (testBegin("cmdExpire()"))
//     {
//         storagePathCreateNP(storageLocalWrite(), archivePath);
//         storagePathCreateNP(storageLocalWrite(), backupPath);
//         storagePathCreateNP(storageLocalWrite(), backupStanzaPath);
//         storagePathCreateNP(storageLocalWrite(), archiveStanzaPath);
//
        // // Create backup directories and manifest files
        // String *full1 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152138F");
        // String *full2 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152800F");
        // String *full2Diff1 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152800F_20181119-152152D");
        // String *full2Incr1 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152800F_20181119-152155I");
        // String *full3 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152900F");
        // String *full3Diff2 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152900F_20181119-152600D");
        //
        // TEST_RESULT_VOID(
        //     storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full1), INFO_MANIFEST_FILE)),
        //         BUFSTRDEF(BOGUS_STR)), "put manifest");
        // TEST_RESULT_VOID(
        //     storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full1), INFO_MANIFEST_FILE ".copy")),
        //         BUFSTRDEF(BOGUS_STR)), "put manifest copy");
        // TEST_RESULT_VOID(
        //     storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full2Diff1), INFO_MANIFEST_FILE)),
        //         BUFSTRDEF(BOGUS_STR)), "put only manifest - no copy");
        // TEST_RESULT_VOID(
        //     storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full2Incr1), INFO_MANIFEST_FILE ".copy")),
        //         BUFSTRDEF(BOGUS_STR)), "put only manifest copy");
//
//         storagePathCreateNP(storageLocalWrite(), full1);
//         storagePathCreateNP(storageLocalWrite(), full1Diff1);
//         storagePathCreateNP(storageLocalWrite(), full1Incr1);
//         storagePathCreateNP(storageLocalWrite(), full1Diff2);
//         storagePathCreateNP(storageLocalWrite(), full2);
//         storagePathCreateNP(storageLocalWrite(), full3);
//
//         system(strPtr(strNewFmt("touch %s/%s", strPtr(full1), INFO_MANIFEST_FILE)));
//         system(strPtr(strNewFmt("touch %s/%s", strPtr(full1), INFO_MANIFEST_FILE ".copy")));
//         system(strPtr(strNewFmt("touch %s/%s", strPtr(full1Diff1), INFO_MANIFEST_FILE)));
//         system(strPtr(strNewFmt("touch %s/%s", strPtr(full1Incr1), INFO_MANIFEST_FILE ".copy")));
//         system(strPtr(strNewFmt("touch %s/%s", strPtr(full1Diff2), INFO_MANIFEST_FILE)));
//         system(strPtr(strNewFmt("touch %s/%s", strPtr(full1Diff2), INFO_MANIFEST_FILE ".copy")));
//         system(strPtr(strNewFmt("touch %s/%s", strPtr(full2), INFO_MANIFEST_FILE)));
//         system(strPtr(strNewFmt("touch %s/%s", strPtr(full2), INFO_MANIFEST_FILE ".copy")));
//
//         String *backupContent = strNew
//         (
//             "[backrest]\n"
//             "backrest-checksum=\"03b699df7362ce202995231c9060c88e6939e481\"\n"
//             "backrest-format=5\n"
//             "backrest-version=\"2.08dev\"\n"
//             "\n"
//             "[backup:current]\n"
//             "20181119-152138F={"
//             "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
//             "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","
//             "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
//             "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
//             "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
//             "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
//             "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
//             "20181119-152138F_20181119-152152D={"
//             "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000003\","
//             "\"backup-archive-stop\":\"000000010000000000000003\",\"backup-info-repo-size\":2369186,"
//             "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
//             "\"backup-prior\":\"20181119-152138F\",\"backup-reference\":[\"20181119-152138F\"],"
//             "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
//             "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
//             "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
//             "20181119-152138F_20181119-152155I={"
//             "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000004\","
//             "\"backup-archive-stop\":\"000000010000000000000004\",\"backup-info-repo-size\":2369186,"
//             "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
//             "\"backup-prior\":\"20181119-152138F_20181119-152152D\","
//             "\"backup-reference\":[\"20181119-152138F\",\"20181119-152138F_20181119-152152D\"],"
//             "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"incr\","
//             "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
//             "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
//             "20181119-152138F_20181119-152600D={"
//             "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000005\","
//             "\"backup-archive-stop\":\"000000010000000000000005\",\"backup-info-repo-size\":2369186,"
//             "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
//             "\"backup-prior\":\"20181119-152138F\",\"backup-reference\":[\"20181119-152138F\"],"
//             "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
//             "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
//             "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
//             "20181119-152800F={"
//             "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
//             "\"backup-archive-start\":\"000000010000000000000006\",\"backup-archive-stop\":\"000000010000000000000006\","
//             "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
//             "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
//             "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
//             "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
//             "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
//             "20181119-152900F={"
//             "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
//             "\"backup-archive-start\":\"000000010000000000000007\",\"backup-archive-stop\":\"000000010000000000000007\","
//             "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
//             "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
//             "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
//             "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
//             "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
//             "20181119-152900F_20181119-152600D={"
//             "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000008\","
//             "\"backup-archive-stop\":\"000000010000000000000008\",\"backup-info-repo-size\":2369186,"
//             "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
//             "\"backup-prior\":\"20181119-152138F\",\"backup-reference\":[\"20181119-152900F\"],"
//             "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
//             "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
//             "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
//             "\n"
//             "[db]\n"
//             "db-catalog-version=201510051\n"
//             "db-control-version=942\n"
//             "db-id=2\n"
//             "db-system-id=6626363367545678089\n"
//             "db-version=\"9.5\"\n"
//             "\n"
//             "[db:history]\n"
//             "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
//                 "\"db-version\":\"9.4\"}\n"
//             "2={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6626363367545678089,"
//                 "\"db-version\":\"9.5\"}\n"
//         );
//
//         TEST_RESULT_VOID(
//             storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/backup.info", strPtr(backupStanzaPath))),
//                 BUFSTR(backupContent)), "put backup info to file");
//
//         String *archiveContent = strNew
//         (
//             "[backrest]\n"
//             "backrest-checksum=\"075a202d42c3b6a0257da5f73a68fa77b342f777\"\n"
//             "backrest-format=5\n"
//             "backrest-version=\"2.08dev\"\n"
//             "\n"
//             "[db]\n"
//             "db-id=2\n"
//             "db-system-id=6626363367545678089\n"
//             "db-version=\"9.5\"\n"
//             "\n"
//             "[db:history]\n"
//             "1={\"db-id\":6625592122879095702,"
//                 "\"db-version\":\"9.4\"}\n"
//             "2={\"db-id\":6626363367545678089,"
//                 "\"db-version\":\"9.5\"}\n"
//         );
//
//         TEST_RESULT_VOID(
//             storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/archive.info", strPtr(archiveStanzaPath))),
//                 BUFSTR(archiveContent)), "put archive info to file");
//
//         StringList *argList = strLstNew();
//         strLstAddZ(argList, "pgbackrest");
//         strLstAdd(argList, strNewFmt("--stanza=%s", strPtr(stanza)));
//         strLstAdd(argList, strNewFmt("--repo-path=%s", strPtr(repoPath)));
//         strLstAddZ(argList, "--repo1-retention-full=1");
//         strLstAddZ(argList, "expire");
//         harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
//
// // CSHANG First test should be Nothing is expired? Or maybe that should be the last
//         TEST_RESULT_VOID(cmdExpire(), "expire");
//
// // CSHANG Test that only the direcories matching the regex of full, incr or diff are removed - maybe create a file with a FULL exp then _save then make sure it is not deleted but the full fir of the same name is. Also, confirm recursion
//
        // TEST_RESULT_VOID(
        //     storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full1), INFO_MANIFEST_FILE)),
        //         BUFSTRDEF(BOGUS_STR)), "put manifest");
        // TEST_RESULT_VOID(
        //     storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full1), INFO_MANIFEST_FILE ".copy")),
        //         BUFSTRDEF(BOGUS_STR)), "put manifest copy");
//     }

    // *****************************************************************************************************************************
    if (testBegin("expireBackup()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");  // surpress warning
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create backup.info
        storagePutNP(storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(backupInfoBase));

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(storageTest, backupInfoFileName, false, cipherTypeNone, NULL),
            "get backup.info");

        // Create backup directories and manifest files
        String *full1 = strNew("20181119-152138F");
        String *full2 = strNew("20181119-152800F");
        String *full2Diff1 = strNew("20181119-152800F_20181119-152152D");
        String *full2Incr1 = strNew("20181119-152800F_20181119-152155I");
        String *full3 = strNew("20181119-152900F");

        String *full1Path = strNewFmt("%s/%s", strPtr(backupStanzaPath), strPtr(full1));
        String *full2Path = strNewFmt("%s/%s", strPtr(backupStanzaPath), strPtr(full2));
        String *full2Diff1Path = strNewFmt("%s/%s", strPtr(backupStanzaPath), strPtr(full2Diff1));
        String *full2Incr1Path = strNewFmt("%s/%s", strPtr(backupStanzaPath), strPtr(full2Incr1));
        String *full3Path = strNewFmt("%s/%s", strPtr(backupStanzaPath), strPtr(full3));

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full1Path), INFO_MANIFEST_FILE)),
                BUFSTRDEF(BOGUS_STR)), "full1 put manifest");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full1Path), INFO_MANIFEST_FILE ".copy")),
                BUFSTRDEF(BOGUS_STR)), "full1 put manifest copy");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full1Path), "bogus")),
                BUFSTRDEF(BOGUS_STR)), "full1 put extra file");
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, full2Path), "full2 empty");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full2Diff1Path), INFO_MANIFEST_FILE)),
                BUFSTRDEF(BOGUS_STR)), "full2Diff1 put only manifest - no copy");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full2Incr1Path), INFO_MANIFEST_FILE ".copy")),
                BUFSTRDEF(BOGUS_STR)), "full2Incr1 put only manifest copy");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full3Path), INFO_MANIFEST_FILE)),
                BUFSTRDEF(BOGUS_STR)), "full3 put manifest");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full3Path), INFO_MANIFEST_FILE ".copy")),
                BUFSTRDEF(BOGUS_STR)), "full3 put manifest copy");

        String *backupExpired = strNew("");

        TEST_RESULT_VOID(expireBackup(infoBackup, full1, backupExpired), "expire backup with both manifest files");
        TEST_RESULT_BOOL(
            (strLstSize(storageListNP(storageTest, full1Path)) && strLstExistsZ(storageListNP(storageTest, full1Path), "bogus")),
            true, "  full1 - only manifest files removed");
// CSHANG Need to finish these
        TEST_RESULT_VOID(expireBackup(infoBackup, full2Incr1, backupExpired), "expire backup with manifest copy only");
        TEST_RESULT_VOID(expireBackup(infoBackup, full2Diff1, backupExpired), "expire backup with manifest only");
        TEST_RESULT_VOID(expireBackup(infoBackup, full2, backupExpired), "expire backup with no manifest");
    }

    // *****************************************************************************************************************************
    if (testBegin("expireFullBackup()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create backup.info
        storagePutNP(storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(backupInfoBase));

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(storageTest, backupInfoFileName, false, cipherTypeNone, NULL),
            "get backup.info");

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 0, "retention-full not set - nothing expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 6, "  current backups not expired");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), "
            "set option 'repo1-retention-full' to the maximum.");

        //--------------------------------------------------------------------------------------------------------------------------
        // Add retention-full
        strLstAddZ(argList, "--repo1-retention-full=2");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 1, "retention-full=2 - one full backup expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 5, "  current backups reduced by 1 full - no dependencies");
        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelListP(infoBackup), ", ")),
            "20181119-152800F, 20181119-152800F_20181119-152152D, 20181119-152800F_20181119-152155I, "
            "20181119-152900F, 20181119-152900F_20181119-152600D", "  remaining backups correct");
        harnessLogResult("P00   INFO: expire full backup 20181119-152138F");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 3, "retention-full=1 - one full backup and dependencies expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "  current backups reduced by 1 full and dependencies");
        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelListP(infoBackup), ", ")),
            "20181119-152900F, 20181119-152900F_20181119-152600D", "  remaining backups correct");
        harnessLogResult(
            "P00   INFO: expire full backup set: 20181119-152800F, 20181119-152800F_20181119-152152D, "
            "20181119-152800F_20181119-152155I");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(expireFullBackup(infoBackup), 0, "retention-full=1 - not enough backups to expire any");
        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelListP(infoBackup), ", ")),
            "20181119-152900F, 20181119-152900F_20181119-152600D", "  remaining backups correct");
    }

    // *****************************************************************************************************************************
    if (testBegin("expireDiffBackup()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");  // avoid warning
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create backup.info
        storagePutNP(storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(backupInfoBase));

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(storageTest, backupInfoFileName, false, cipherTypeNone, NULL),
            "get backup.info");

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 0, "retention-diff not set - nothing expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 6, "  current backups not expired");

        //--------------------------------------------------------------------------------------------------------------------------
        // Add retention-diff
        StringList *argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "--repo1-retention-diff=6");
        harnessCfgLoad(strLstSize(argListTemp), strLstPtr(argListTemp));

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 0, "retention-diff=6 - too soon to expire");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 6, "  current backups not expired");

        //--------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--repo1-retention-diff=2");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 2, "retention-diff=2 - full considered in diff");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 4, "  current backups reduced by 1 diff and dependent increment");
        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelListP(infoBackup), ", ")),
            "20181119-152138F, 20181119-152800F, 20181119-152900F, 20181119-152900F_20181119-152600D",
            "  remaining backups correct");
        harnessLogResult(
            "P00   INFO: expire diff backup set: 20181119-152800F_20181119-152152D, 20181119-152800F_20181119-152155I");

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 0, "retention-diff=2 but no more to expire");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 4, "  current backups not reduced");
    }

    // *****************************************************************************************************************************
    if (testBegin("removeExpiredBackup()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create backup.info
        storagePutNP(
            storageNewWriteNP(storageTest, backupInfoFileName),
            harnessInfoChecksumZ(
                "[backup:current]\n"
                "20181119-152138F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "\n"
                "[db]\n"
                "db-catalog-version=201409291\n"
                "db-control-version=942\n"
                "db-id=1\n"
                "db-system-id=6625592122879095702\n"
                "db-version=\"9.4\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                    "\"db-version\":\"9.4\"}"));

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(storageTest, backupInfoFileName, false, cipherTypeNone, NULL),
            "get backup.info");

        // Create backup directories, manifest files and other path/file
        String *full = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152100F");
        String *diff = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152100F_20181119-152152D");
        String *otherPath = strNewFmt("%s/%s", strPtr(backupStanzaPath), "bogus");
        String *otherFile = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181118-152100F_20181119-152152D.save");
        String *full1 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152138F");

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full), "bogus")),
                BUFSTRDEF(BOGUS_STR)), "put file");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/%s", strPtr(full1), "somefile")),
                BUFSTRDEF(BOGUS_STR)), "put file");
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, diff), "empty backup directory must not error on delete");
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, otherPath), "other path must not be removed");
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, otherFile),
                BUFSTRDEF(BOGUS_STR)), "directory look-alike file must not be removed");

        TEST_RESULT_VOID(removeExpiredBackup(infoBackup), "remove backups not in backup.info current");

        harnessLogResult(
            "P00   INFO: remove expired backup 20181119-152100F_20181119-152152D\n"
            "P00   INFO: remove expired backup 20181119-152100F");

        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(storageTest, backupStanzaPath), sortOrderAsc), ", ")),
            "20181118-152100F_20181119-152152D.save, 20181119-152138F, backup.info, bogus", "  remaining file/directories correct");

        //--------------------------------------------------------------------------------------------------------------------------
        // Create backup.info without current backups
        storagePutNP(
            storageNewWriteNP(storageTest, backupInfoFileName),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-catalog-version=201409291\n"
                "db-control-version=942\n"
                "db-id=1\n"
                "db-system-id=6625592122879095702\n"
                "db-version=\"9.4\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                    "\"db-version\":\"9.4\"}"));

        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(storageTest, backupInfoFileName, false, cipherTypeNone, NULL),
            "get backup.info");

        TEST_RESULT_VOID(removeExpiredBackup(infoBackup), "remove backups - backup.info current empty");

        harnessLogResult("P00   INFO: remove expired backup 20181119-152138F");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(storageTest, backupStanzaPath), sortOrderAsc), ", ")),
            "20181118-152100F_20181119-152152D.save, backup.info, bogus", "  remaining file/directories correct");
    }

    // *****************************************************************************************************************************
    if (testBegin("sortArchiveId()"))
    {
        StringList *list = strLstNew();

        strLstAddZ(list, "11-10");
        strLstAddZ(list, "10-4");
        strLstAddZ(list, "9.4-2");
        strLstAddZ(list, "9.6-1");

        TEST_RESULT_STR(strPtr(strLstJoin(sortArchiveId(list, sortOrderAsc), ", ")), "9.6-1, 9.4-2, 10-4, 11-10", "sort ascending");
        TEST_RESULT_STR(
            strPtr(strLstJoin(sortArchiveId(list, sortOrderDesc), ", ")), "11-10, 10-4, 9.4-2, 9.6-1", "sort descending");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
