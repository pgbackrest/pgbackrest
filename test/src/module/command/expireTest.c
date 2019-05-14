/***********************************************************************************************************************************
Test Command Control
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
void
archiveGenerate(
    Storage *storageTest, String *archiveStanzaPath, const unsigned int start, unsigned int end, const char *archiveId,
    const char *majorWal)
{
    // For simplicity, only allow 2 digits
    if (end > 99)
        end = 99;

    String *wal = NULL;

    for (unsigned int i = start; i <= end; i++)
    {
        if (i < 10)
            wal = strNewFmt("%s0000000%u-9baedd24b61aa15305732ac678c4e2c102435a09", majorWal, i);
        else
            wal = strNewFmt("%s000000%u-9baedd24b61aa15305732ac678c4e2c102435a09", majorWal, i);

        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/%s/%s/%s", strPtr(archiveStanzaPath), archiveId, majorWal, strPtr(wal))),
            BUFSTRDEF(BOGUS_STR));
    }
}

String *
archiveExpectList(const unsigned int start, unsigned int end, const char *majorWal)
{
    String *result = strNew("");

    // For simplicity, only allow 2 digits
    if (end > 99)
        end = 99;

    String *wal = NULL;

    for (unsigned int i = start; i <= end; i++)
    {
        if (i < 10)
            wal = strNewFmt("%s0000000%u-9baedd24b61aa15305732ac678c4e2c102435a09", majorWal, i);
        else
            wal = strNewFmt("%s000000%u-9baedd24b61aa15305732ac678c4e2c102435a09", majorWal, i);

        if (strSize(result) == 0)
            strCat(result, strPtr(wal));
        else
            strCatFmt(result, ", %s", strPtr(wal));
    }

    return result;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    String *backupStanzaPath = strNew("repo/backup/db");
    String *backupInfoFileName = strNewFmt("%s/backup.info", strPtr(backupStanzaPath));
    String *archiveStanzaPath = strNew("repo/archive/db");
    String *archiveInfoFileName = strNewFmt("%s/archive.info", strPtr(archiveStanzaPath));

    StringList *argListBase = strLstNew();
    strLstAddZ(argListBase, "pgbackrest");
    strLstAddZ(argListBase, "--stanza=db");
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));
    strLstAddZ(argListBase, "expire");

    StringList *argListAvoidWarn = strLstDup(argListBase);
    strLstAddZ(argListAvoidWarn, "--repo1-retention-full=1");  // avoid warning

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

    // *****************************************************************************************************************************
    if (testBegin("expireBackup()"))
    {
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
        String *full1Path = strNewFmt("%s/%s", strPtr(backupStanzaPath), strPtr(full1));
        String *full2Path = strNewFmt("%s/%s", strPtr(backupStanzaPath), strPtr(full2));

        // Load Parameters
        StringList *argList = strLstDup(argListAvoidWarn);
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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

        String *backupExpired = strNew("");

        TEST_RESULT_VOID(expireBackup(infoBackup, full1, backupExpired), "expire backup with both manifest files");
        TEST_RESULT_BOOL(
            (strLstSize(storageListNP(storageTest, full1Path)) && strLstExistsZ(storageListNP(storageTest, full1Path), "bogus")),
            true, "  full1 - only manifest files removed");

        TEST_RESULT_VOID(expireBackup(infoBackup, full2, backupExpired), "expire backup with no manifest - does not error");

        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelListNP(infoBackup), ", ")),
                "20181119-152800F_20181119-152152D, 20181119-152800F_20181119-152155I, 20181119-152900F, "
                "20181119-152900F_20181119-152600D",
            "only backups passed to expireBackup are removed from backup:current");
    }

    // *****************************************************************************************************************************
    if (testBegin("expireFullBackup()"))
    {
        // Create backup.info
        storagePutNP(storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(backupInfoBase));

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(storageTest, backupInfoFileName, false, cipherTypeNone, NULL),
            "get backup.info");

        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 0, "retention-full not set");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), "
            "set option 'repo1-retention-full' to the maximum.");

        //--------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--repo1-retention-full=2");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 1, "retention-full=2 - one full backup expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 5, "  current backups reduced by 1 full - no dependencies");
        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelListNP(infoBackup), ", ")),
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
            strPtr(strLstJoin(infoBackupDataLabelListNP(infoBackup), ", ")),
            "20181119-152900F, 20181119-152900F_20181119-152600D", "  remaining backups correct");
        harnessLogResult(
            "P00   INFO: expire full backup set: 20181119-152800F, 20181119-152800F_20181119-152152D, "
            "20181119-152800F_20181119-152155I");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(expireFullBackup(infoBackup), 0, "retention-full=1 - not enough backups to expire any");
        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelListNP(infoBackup), ", ")),
            "20181119-152900F, 20181119-152900F_20181119-152600D", "  remaining backups correct");
    }

    // *****************************************************************************************************************************
    if (testBegin("expireDiffBackup()"))
    {
        // Create backup.info
        storagePutNP(storageNewWriteNP(storageTest, backupInfoFileName), harnessInfoChecksum(backupInfoBase));

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(storageTest, backupInfoFileName, false, cipherTypeNone, NULL),
            "get backup.info");

        // Load Parameters
        StringList *argList = strLstDup(argListAvoidWarn);
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 0, "retention-diff not set - nothing expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 6, "  current backups not expired");

        //--------------------------------------------------------------------------------------------------------------------------
        // Add retention-diff
        StringList *argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "--repo1-retention-diff=6");
        harnessCfgLoad(strLstSize(argListTemp), strLstPtr(argListTemp));

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 0, "retention-diff set - too soon to expire");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 6, "  current backups not expired");

        //--------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--repo1-retention-diff=2");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 2, "retention-diff set - full considered in diff");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 4, "  current backups reduced by 1 diff and dependent increment");
        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelListNP(infoBackup), ", ")),
            "20181119-152138F, 20181119-152800F, 20181119-152900F, 20181119-152900F_20181119-152600D",
            "  remaining backups correct");
        harnessLogResult(
            "P00   INFO: expire diff backup set: 20181119-152800F_20181119-152152D, 20181119-152800F_20181119-152155I");

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 0, "retention-diff=2 but no more to expire");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 4, "  current backups not reduced");

        //--------------------------------------------------------------------------------------------------------------------------
        // Create backup.info with two diff - oldest to be expired - no "set:"
        storagePutNP(
            storageNewWriteNP(storageTest, backupInfoFileName),
            harnessInfoChecksumZ(
            "[backup:current]\n"
            "20181119-152800F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","
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
            "20181119-152800F_20181119-152155D={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000004\","
            "\"backup-archive-stop\":\"000000010000000000000004\",\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152800F\",\"backup-reference\":[\"20181119-152800F\"],"
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
                "\"db-version\":\"9.4\"}"));
        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(storageTest, backupInfoFileName, false, cipherTypeNone, NULL),
            "get backup.info");

        // Load parameters
        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-diff=1");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 1, "retention-diff set - only oldest diff expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "  current backups reduced by one");
        TEST_RESULT_STR(
            strPtr(strLstJoin(infoBackupDataLabelListNP(infoBackup), ", ")),
            "20181119-152800F, 20181119-152800F_20181119-152155D",
            "  remaining backups correct");
        harnessLogResult(
            "P00   INFO: expire diff backup 20181119-152800F_20181119-152152D");
    }

    // *****************************************************************************************************************************
    if (testBegin("removeExpiredBackup()"))
    {
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

        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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
    if (testBegin("removeExpiredArchive() & cmdExpire()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(storageTest, backupInfoFileName, false, cipherTypeNone, NULL),
            "get backup.info");

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "archive retention not set");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), "
            "set option 'repo1-retention-full' to the maximum.\n"
            "P00   INFO: option 'repo1-retention-archive' is not set - archive logs will not be expired");

        //--------------------------------------------------------------------------------------------------------------------------
        // Set archive retention, archive retention type default but no current backups - code path test
        strLstAddZ(argList, "--repo1-retention-archive=4");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "archive retention set, retention type default, no current backups");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), "
            "set option 'repo1-retention-full' to the maximum.");

        //--------------------------------------------------------------------------------------------------------------------------
        // Create backup.info with current backups
        storagePutNP(storageNewWriteNP(storageTest, backupInfoFileName),
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
                "20181119-152800F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000020000000000000002\",\"backup-archive-stop\":\"000000020000000000000002\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152800F_20181119-152152D={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000020000000000000004\","
                "\"backup-archive-stop\":\"000000020000000000000005\",\"backup-info-repo-size\":2369186,"
                "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
                "\"backup-prior\":\"20181119-152800F\",\"backup-reference\":[\"20181119-152800F\"],"
                "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
                "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152800F_20181119-152155I={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000020000000000000007\","
                "\"backup-archive-stop\":\"000000020000000000000007\",\"backup-info-repo-size\":2369186,"
                "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
                "\"backup-prior\":\"20181119-152800F_20181119-152152D\","
                "\"backup-reference\":[\"20181119-152800F\",\"20181119-152800F_20181119-152152D\"],"
                "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"incr\","
                "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152800F_20181119-152252D={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000020000000000000009\","
                "\"backup-archive-stop\":\"000000020000000000000009\",\"backup-info-repo-size\":2369186,"
                "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
                "\"backup-prior\":\"20181119-152800F\",\"backup-reference\":[\"20181119-152800F\"],"
                "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
                "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152900F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000010000000000000003\",\"backup-archive-stop\":\"000000010000000000000004\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152900F_20181119-152500I={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000006\","
                "\"backup-archive-stop\":\"000000010000000000000006\",\"backup-info-repo-size\":2369186,"
                "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
                "\"backup-prior\":\"20181119-152900F\",\"backup-reference\":[\"20181119-152900F\"],"
                "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
                "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "\n"
                "[db]\n"
                "db-catalog-version=201707211\n"
                "db-control-version=1002\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"10\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                    "\"db-version\":\"9.4\"}\n"
                "2={\"db-catalog-version\":201707211,\"db-control-version\":1002,\"db-system-id\":6626363367545678089,"
                    "\"db-version\":\"10\"}\n"));

        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(storageTest, backupInfoFileName, false, cipherTypeNone, NULL),
            "get backup.info");

        storagePutNP(
            storageNewWriteNP(storageTest, archiveInfoFileName),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"10\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}"));

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "no archive on disk");

        //--------------------------------------------------------------------------------------------------------------------------
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000100000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000200000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "10-2", "0000000100000000");

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "archive retention type = full (default), repo1-retention-archive=4");

        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(2, 10, "0000000100000000")),
            "  only 9.4-1/0000000100000000/000000010000000000000001 removed");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(1, 10, "0000000200000000")),
            "  none removed from 9.4-1/0000000200000000 - crossing timelines to play through PITR");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(3, 10, "0000000100000000")),
            "  000000010000000000000001 and 000000010000000000000002 removed from 10-2/0000000100000000");
        harnessLogResult(
            "P00   INFO: full backup total < 4 - using oldest full backup for 9.4-1 archive retention\n"
            "P00   INFO: full backup total < 4 - using oldest full backup for 10-2 archive retention");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=2");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "archive retention type = full (default), repo1-retention-archive=2");

        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(2, 2, "0000000100000000")),
            "  only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(2, 10, "0000000200000000")),
            "  only 9.4-1/0000000200000000/000000010000000000000001 removed from major wal 2");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(3, 10, "0000000100000000")),
            "  none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "archive retention type = full (default), repo1-retention-archive=1");

        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(2, 2, "0000000100000000")),
            "  only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(2, 10, "0000000200000000")),
            "  nothing removed from 9.4-1/0000000200000000 major wal 2 - each archiveId must have one backup to play through PITR");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(3, 10, "0000000100000000")),
            "  none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=2");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        strLstAddZ(argList, "--repo1-retention-diff=2");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup),
            "full counts as differential and incremental associated with differential expires");

        String *result = strNew("");
        strCatFmt(
            result,
            "%s, %s, %s, %s",
            strPtr(archiveExpectList(2, 2, "0000000200000000")),
            strPtr(archiveExpectList(4, 5, "0000000200000000")),
            strPtr(archiveExpectList(7, 7, "0000000200000000")),
            strPtr(archiveExpectList(9, 10, "0000000200000000")));

        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(2, 2, "0000000100000000")),
            "  only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc), ", ")),
            strPtr(result),
            "  all in-between removed from 9.4-1/0000000200000000 major wal 2 - last backup able to play through PITR");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(3, 10, "0000000100000000")),
            "  none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=4");
        strLstAddZ(argList, "--repo1-retention-archive-type=incr");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Regenerate archive
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000200000000");

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "differential and full count as an incremental");

        result = strNew("");
        strCatFmt(
            result,
            "%s, %s, %s",
            strPtr(archiveExpectList(2, 2, "0000000200000000")),
            strPtr(archiveExpectList(4, 5, "0000000200000000")),
            strPtr(archiveExpectList(7, 10, "0000000200000000")));

        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(2, 2, "0000000100000000")),
            "  only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc), ", ")),
            strPtr(result),
            "  incremental and after remain in 9.4-1/0000000200000000 major wal 2");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(3, 10, "0000000100000000")),
            "  none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=2");
        strLstAddZ(argList, "--repo1-retention-diff=3");
        strLstAddZ(argList, "--repo1-retention-archive=2");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdExpire(), "expire last backup in archive sub path and remove sub path");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("%s/%s", strPtr(archiveStanzaPath), "9.4-1/0000000100000000")),
            false, "  archive sub path removed");
        harnessLogResult("P00   INFO: expire full backup 20181119-152138F");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdExpire(), "expire last backup in archive path and remove path");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("%s/%s", strPtr(archiveStanzaPath), "9.4-1")),
            false, "  archive path removed");

        harnessLogResult(strPtr(strNewFmt(
            "P00   INFO: expire full backup set: 20181119-152800F, 20181119-152800F_20181119-152152D, "
            "20181119-152800F_20181119-152155I, 20181119-152800F_20181119-152252D\n"
            "P00   INFO: remove archive path: %s/%s/9.4-1", testPath(), strPtr(archiveStanzaPath))));

        TEST_ASSIGN(
            infoBackup,
            infoBackupNew(storageTest, backupInfoFileName, false, cipherTypeNone, NULL),
            "  get backup.info");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "  backup.info updated on disk");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(infoBackupDataLabelListNP(infoBackup), sortOrderAsc), ", ")),
            "20181119-152900F, 20181119-152900F_20181119-152500I", "  remaining current backups correct");

        //--------------------------------------------------------------------------------------------------------------------------
        storagePutNP(storageNewWriteNP(storageTest, backupInfoFileName),
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
                "20181119-152800F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152900F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000010000000000000004\",\"backup-archive-stop\":\"000000010000000000000004\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "\n"
                "[db]\n"
                "db-catalog-version=201707211\n"
                "db-control-version=1002\n"
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

        archiveGenerate(storageTest, archiveStanzaPath, 1, 5, "9.4-1", "0000000100000000");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=2");
        strLstAddZ(argList, "--repo1-retention-archive-type=full");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup), "backup selected for retention does not have archive-start so do nothing");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(1, 5, "0000000100000000")),
            "  nothing removed from 9.4-1/0000000100000000");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=4");
        strLstAddZ(argList, "--repo1-retention-archive-type=incr");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup), "full count as incr but not enough backups, retention set to first full");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", ")),
            strPtr(archiveExpectList(2, 5, "0000000100000000")),
            "  only removed archive prior to first full");
        harnessLogResult(
            "P00   INFO: full backup total < 4 - using oldest full backup for 9.4-1 archive retention");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        strLstAddZ(argList, "--repo1-retention-archive-type=full");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        harnessLogLevelSet(logLevelDetail);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup), "backup earlier than selected for retention does not have archive-start");
        harnessLogResult(
            "P00 DETAIL: archive retention on backup 20181119-152138F, archiveId = 9.4-1, start = 000000010000000000000002,"
            " stop = 000000010000000000000002\n"
            "P00 DETAIL: archive retention on backup 20181119-152900F, archiveId = 9.4-1, start = 000000010000000000000004\n"
            "P00 DETAIL: remove archive: archiveId = 9.4-1, start = 000000010000000000000003, stop = 000000010000000000000003");

        harnessLogLevelReset();
    }

    // *****************************************************************************************************************************
    if (testBegin("sortArchiveId()"))
    {
        StringList *list = strLstNew();

        strLstAddZ(list, "10-4");
        strLstAddZ(list, "11-10");
        strLstAddZ(list, "9.6-1");

        TEST_RESULT_STR(strPtr(strLstJoin(sortArchiveId(list, sortOrderAsc), ", ")), "9.6-1, 10-4, 11-10", "sort ascending");

        strLstAddZ(list, "9.4-2");
        TEST_RESULT_STR(
            strPtr(strLstJoin(sortArchiveId(list, sortOrderDesc), ", ")), "11-10, 10-4, 9.4-2, 9.6-1", "sort descending");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
