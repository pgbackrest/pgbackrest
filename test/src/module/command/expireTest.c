/***********************************************************************************************************************************
Test Expire Command
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
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

        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/%s/%s/%s", strPtr(archiveStanzaPath), archiveId, majorWal, strPtr(wal))),
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
    strLstAddZ(argListBase, "--stanza=db");
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));

    StringList *argListAvoidWarn = strLstDup(argListBase);
    strLstAddZ(argListAvoidWarn, "--repo1-retention-full=1");  // avoid warning

    const Buffer *backupInfoBase = harnessInfoChecksumZ
    (
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
        "\"backup-archive-start\":\"000000010000000000000004\",\"backup-archive-stop\":\"000000010000000000000004\","
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
        "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "20181119-152800F_20181119-152152D={"
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000005\","
        "\"backup-archive-stop\":\"000000010000000000000005\",\"backup-info-repo-size\":2369186,"
        "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
        "\"backup-prior\":\"20181119-152800F\",\"backup-reference\":[\"20181119-152800F\"],"
        "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "20181119-152800F_20181119-152155I={"
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000006\","
        "\"backup-archive-stop\":\"000000010000000000000006\",\"backup-info-repo-size\":2369186,"
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
        "\"backup-prior\":\"20181119-152900F\",\"backup-reference\":[\"20181119-152900F\"],"
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
            "\"db-version\":\"9.4\"}"
    );

    // *****************************************************************************************************************************
    if (testBegin("expireBackup()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("manifest file removal");

        // Create backup.info
        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(backupInfoBase)), "get backup.info");

        // Create backup directories and manifest files
        String *full1 = strNew("20181119-152138F");
        String *full2 = strNew("20181119-152800F");
        String *full1Path = strNewFmt("%s/%s", strPtr(backupStanzaPath), strPtr(full1));
        String *full2Path = strNewFmt("%s/%s", strPtr(backupStanzaPath), strPtr(full2));

        // Load Parameters
        StringList *argList = strLstDup(argListAvoidWarn);
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("%s/%s", strPtr(full1Path), BACKUP_MANIFEST_FILE)), BUFSTRDEF(BOGUS_STR)),
                "full1 put manifest");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest, strNewFmt("%s/%s", strPtr(full1Path), BACKUP_MANIFEST_FILE ".copy")), BUFSTRDEF(BOGUS_STR)),
            "full1 put manifest copy");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/%s", strPtr(full1Path), "bogus")),
                BUFSTRDEF(BOGUS_STR)), "full1 put extra file");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, full2Path), "full2 empty");


        TEST_RESULT_VOID(expireBackup(infoBackup, full1), "expire backup with both manifest files");
        TEST_RESULT_BOOL(
            (strLstSize(storageListP(storageTest, full1Path)) && strLstExistsZ(storageListP(storageTest, full1Path), "bogus")),
            true, "full1 - only manifest files removed");

        TEST_RESULT_VOID(expireBackup(infoBackup, full2), "expire backup with no manifest - does not error");

        TEST_RESULT_STR_Z(
            strLstJoin(infoBackupDataLabelList(infoBackup, NULL), ", "),
            "20181119-152900F, 20181119-152900F_20181119-152600D",
            "only backups in set passed to expireBackup are removed from backup:current (result is sorted)");
    }

    // *****************************************************************************************************************************
    if (testBegin("expireFullBackup()"))
    {
        // Create backup.info
        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(backupInfoBase)), "get backup.info");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-full not set - nothing expired");

        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 0, "retention-full not set");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), "
            "set option 'repo1-retention-full' to the maximum.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-full set - full backup no dependencies expired");

        strLstAddZ(argList, "--repo1-retention-full=2");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 1, "retention-full=2 - one full backup expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 5, "current backups reduced by 1 full - no dependencies");
        TEST_RESULT_STR_Z(
            strLstJoin(infoBackupDataLabelList(infoBackup, NULL), ", "),
            "20181119-152800F, 20181119-152800F_20181119-152152D, 20181119-152800F_20181119-152155I"
                ", 20181119-152900F, 20181119-152900F_20181119-152600D",
            "remaining backups correct");
        harnessLogResult("P00   INFO: expire full backup 20181119-152138F");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-full set - full backup with dependencies expired");

        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 3, "retention-full=1 - one full backup and dependencies expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "current backups reduced by 1 full and dependencies");
        TEST_RESULT_STR_Z(
            strLstJoin(infoBackupDataLabelList(infoBackup, NULL), ", "), "20181119-152900F, 20181119-152900F_20181119-152600D",
            "remaining backups correct");
        harnessLogResult(
            "P00   INFO: expire full backup set: 20181119-152800F, 20181119-152800F_20181119-152152D, "
            "20181119-152800F_20181119-152155I");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-full set - no backups expired");

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 0, "retention-full=1 - not enough backups to expire any");
        TEST_RESULT_STR_Z(
            strLstJoin(infoBackupDataLabelList(infoBackup, NULL), ", "), "20181119-152900F, 20181119-152900F_20181119-152600D",
            "  remaining backups correct");
    }

    // *****************************************************************************************************************************
    if (testBegin("expireDiffBackup()"))
    {
        // Create backup.info
        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(backupInfoBase)), "get backup.info");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-diff not set");

        // Load Parameters
        StringList *argList = strLstDup(argListAvoidWarn);
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 0, "retention-diff not set - nothing expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 6, "current backups not expired");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-diff set - nothing yet to expire");

        // Add retention-diff
        StringList *argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "--repo1-retention-diff=6");
        harnessCfgLoad(cfgCmdExpire, argListTemp);

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 0, "retention-diff set - too soon to expire");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 6, "current backups not expired");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-diff set - diff and dependent incr expired");

        strLstAddZ(argList, "--repo1-retention-diff=2");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 2, "retention-diff=2 - full considered in diff");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 4, "current backups reduced by 1 diff and dependent increment");
        TEST_RESULT_STR_Z(
            strLstJoin(infoBackupDataLabelList(infoBackup, NULL), ", "),
            "20181119-152138F, 20181119-152800F, 20181119-152900F, 20181119-152900F_20181119-152600D",
            "remaining backups correct");
        harnessLogResult(
            "P00   INFO: expire diff backup set: 20181119-152800F_20181119-152152D, 20181119-152800F_20181119-152155I");

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 0, "retention-diff=2 but no more to expire");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 4, "current backups not reduced");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-diff=1");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(backupInfoBase)), "get backup.info");
        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 2, "retention-diff set to 1 - full considered in diff");
        TEST_RESULT_STR_Z(
            strLstJoin(infoBackupDataLabelList(infoBackup, NULL), ", "),
            "20181119-152138F, 20181119-152800F, 20181119-152900F, 20181119-152900F_20181119-152600D",
            "  remaining backups correct");
        harnessLogResult(
            "P00   INFO: expire diff backup set: 20181119-152800F_20181119-152152D, 20181119-152800F_20181119-152155I");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-diff set - diff with no dependents expired");

        // Create backup.info with two diff - oldest to be expired - no "set:"
        const Buffer *backupInfoContent = harnessInfoChecksumZ
        (
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
                "\"db-version\":\"9.4\"}"
        );

        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(backupInfoContent)), "get backup.info");

        // Load parameters
        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-diff=1");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireDiffBackup(infoBackup), 1, "retention-diff set - only oldest diff expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "current backups reduced by one");
        TEST_RESULT_STR_Z(
            strLstJoin(infoBackupDataLabelList(infoBackup, NULL), ", "), "20181119-152800F, 20181119-152800F_20181119-152155D",
            "remaining backups correct");
        harnessLogResult(
            "P00   INFO: expire diff backup 20181119-152800F_20181119-152152D");
    }

    // *****************************************************************************************************************************
    if (testBegin("removeExpiredBackup()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove expired backup from disk - backup not in current backup");

        // Create backup.info
        storagePutP(
            storageNewWriteP(storageTest, backupInfoFileName),
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
        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, backupInfoFileName, cipherTypeNone, NULL), "get backup.info");

        // Create backup directories, manifest files and other path/file
        String *full = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152100F");
        String *diff = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152100F_20181119-152152D");
        String *otherPath = strNewFmt("%s/%s", strPtr(backupStanzaPath), "bogus");
        String *otherFile = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181118-152100F_20181119-152152D.save");
        String *full1 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152138F");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/%s", strPtr(full), "bogus")),
                BUFSTRDEF(BOGUS_STR)), "put file");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/%s", strPtr(full1), "somefile")),
                BUFSTRDEF(BOGUS_STR)), "put file");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, diff), "empty backup directory must not error on delete");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, otherPath), "other path must not be removed");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, otherFile),
                BUFSTRDEF(BOGUS_STR)), "directory look-alike file must not be removed");

        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(removeExpiredBackup(infoBackup, NULL), "remove backups not in backup.info current");

        harnessLogResult(
            "P00   INFO: remove expired backup 20181119-152100F_20181119-152152D\n"
            "P00   INFO: remove expired backup 20181119-152100F");

        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(storageListP(storageTest, backupStanzaPath), sortOrderAsc), ", "),
            "20181118-152100F_20181119-152152D.save, 20181119-152138F, backup.info, bogus",
            "remaining file/directories correct");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove expired backup from disk - no current backups");

        // Create backup.info without current backups
        const Buffer *backupInfoContent = harnessInfoChecksumZ
        (
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6625592122879095702\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                "\"db-version\":\"9.4\"}"
        );

        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(backupInfoContent)), "get backup.info");

        TEST_RESULT_VOID(removeExpiredBackup(infoBackup, NULL), "remove backups - backup.info current empty");

        harnessLogResult("P00   INFO: remove expired backup 20181119-152138F");
        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(storageListP(storageTest, backupStanzaPath), sortOrderAsc), ", "),
            "20181118-152100F_20181119-152152D.save, backup.info, bogus", "remaining file/directories correct");
    }

    // *****************************************************************************************************************************
    if (testBegin("removeExpiredArchive() & cmdExpire()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive not set");

        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdExpire, argList);

        // Create backup.info without current backups
        const Buffer *backupInfoContent = harnessInfoChecksumZ
        (
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6625592122879095702\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                "\"db-version\":\"9.4\"}"
        );

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(backupInfoContent)), "get backup.info");

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "archive retention not set");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), "
            "set option 'repo1-retention-full' to the maximum.\n"
            "P00   INFO: option 'repo1-retention-archive' is not set - archive logs will not be expired");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set - no current backups");

        // Set archive retention, archive retention type default but no current backups - code path test
        strLstAddZ(argList, "--repo1-retention-archive=4");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "archive retention set, retention type default, no current backups");
        harnessLogResult(
            "P00   WARN: option repo1-retention-full is not set, the repository may run out of space\n"
            "            HINT: to retain full backups indefinitely (without warning), "
            "set option 'repo1-retention-full' to the maximum.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set - no archive on disk");

        // Create backup.info with current backups spread over different timelines
        storagePutP(storageNewWriteP(storageTest, backupInfoFileName),
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

        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, backupInfoFileName, cipherTypeNone, NULL), "get backup.info");

        storagePutP(
            storageNewWriteP(storageTest, archiveInfoFileName),
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
        TEST_TITLE("retention-archive set - remove archives across timelines");

        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000100000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000200000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "10-2", "0000000100000000");

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "archive retention type = full (default), repo1-retention-archive=4");

        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(2, 10, "0000000100000000"), "only 9.4-1/0000000100000000/000000010000000000000001 removed");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc), ", "),
            archiveExpectList(1, 10, "0000000200000000"),
            "none removed from 9.4-1/0000000200000000 - crossing timelines to play through PITR");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(3, 10, "0000000100000000"),
            "000000010000000000000001 and 000000010000000000000002 removed from 10-2/0000000100000000");
        harnessLogResult(
            "P00   INFO: full backup total < 4 - using oldest full backup for 9.4-1 archive retention\n"
            "P00   INFO: full backup total < 4 - using oldest full backup for 10-2 archive retention");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set - latest archive not expired");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=2");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "archive retention type = full (default), repo1-retention-archive=2");

        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(2, 2, "0000000100000000"),
            "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc), ", "),
            archiveExpectList(2, 10, "0000000200000000"),
            "only 9.4-1/0000000200000000/000000010000000000000001 removed from major wal 2");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(3, 10, "0000000100000000"),
            "none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set to lowest - keep PITR for each archiveId");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup), "archive retention type = full (default), repo1-retention-archive=1");

        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(2, 2, "0000000100000000"),
            "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc), ", "),
            archiveExpectList(2, 10, "0000000200000000"),
            "nothing removed from 9.4-1/0000000200000000 major wal 2 - each archiveId must have one backup to play through PITR");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(3, 10, "0000000100000000"), "none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive, retention-archive-type=diff, retention-diff set");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=2");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        strLstAddZ(argList, "--repo1-retention-diff=2");
        harnessCfgLoad(cfgCmdExpire, argList);

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
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(2, 2, "0000000100000000"),
            "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc), ", "),
            result, "all in-between removed from 9.4-1/0000000200000000 major wal 2 - last backup able to play through PITR");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(3, 10, "0000000100000000"), "none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive, retention-archive-type=incr");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=4");
        strLstAddZ(argList, "--repo1-retention-archive-type=incr");
        harnessCfgLoad(cfgCmdExpire, argList);

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
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(2, 2, "0000000100000000"),
            "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc), ", "),
            result, "incremental and after remain in 9.4-1/0000000200000000 major wal 2");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(3, 10, "0000000100000000"), "none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - dry run");

        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=2");
        strLstAddZ(argList, "--repo1-retention-diff=3");
        strLstAddZ(argList, "--repo1-retention-archive=2");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        strLstAddZ(argList, "--dry-run");
        harnessCfgLoad(cfgCmdExpire, argList);

        // Write backup.manifest so infoBackup reconstruct produces same results as backup.info on disk
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152152D/" BACKUP_MANIFEST_FILE,
            strPtr(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152155I/" BACKUP_MANIFEST_FILE,
            strPtr(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE,
            strPtr(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F_20181119-152500I/" BACKUP_MANIFEST_FILE,
            strPtr(backupStanzaPath))), BUFSTRDEF("tmp"));

        TEST_RESULT_VOID(cmdExpire(), "expire (dry-run) do not remove last backup in archive sub path or sub path");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("%s/%s", strPtr(archiveStanzaPath), "9.4-1/0000000100000000")),
            true, "archive sub path not removed");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))),
            true, "backup not removed");
        harnessLogResult(
            "P00   INFO: [DRY-RUN] expire full backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152138F");

        // Save a copy of the info files for a later test
        storageCopy(
            storageNewReadP(storageTest, backupInfoFileName),
            storageNewWriteP(storageTest, strNewFmt("%s%s", strPtr(backupInfoFileName), ".save")));
        storageCopy(
            storageNewReadP(storageTest, archiveInfoFileName),
            storageNewWriteP(storageTest, strNewFmt("%s%s", strPtr(archiveInfoFileName), ".save")));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire via backup command");

        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=2");
        strLstAddZ(argList, "--repo1-retention-diff=3");
        strLstAddZ(argList, "--repo1-retention-archive=2");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_RESULT_VOID(cmdExpire(), "via backup command: expire last backup in archive sub path and remove sub path");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("%s/%s", strPtr(archiveStanzaPath), "9.4-1/0000000100000000")),
            false, "archive sub path removed");
        harnessLogResult(
            "P00   INFO: expire full backup 20181119-152138F\n"
            "P00   INFO: remove expired backup 20181119-152138F");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - no dry run");

        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=2");
        strLstAddZ(argList, "--repo1-retention-diff=3");
        strLstAddZ(argList, "--repo1-retention-archive=2");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        harnessCfgLoad(cfgCmdExpire, argList);

        // Restore info files from a previous test
        storageCopy(
            storageNewReadP(storageTest, strNewFmt("%s%s", strPtr(backupInfoFileName), ".save")),
            storageNewWriteP(storageTest, backupInfoFileName));
        storageCopy(
            storageNewReadP(storageTest,  strNewFmt("%s%s", strPtr(archiveInfoFileName), ".save")),
            storageNewWriteP(storageTest, archiveInfoFileName));

        // Write out manifest and archive that will be removed
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        archiveGenerate(storageTest, archiveStanzaPath, 2, 2, "9.4-1", "0000000100000000");

        TEST_RESULT_VOID(cmdExpire(), "expire last backup in archive sub path and remove sub path");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("%s/%s", strPtr(archiveStanzaPath), "9.4-1/0000000100000000")),
            false, "archive sub path removed");
        harnessLogResult(
            "P00   INFO: expire full backup 20181119-152138F\n"
            "P00   INFO: remove expired backup 20181119-152138F");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - dry run: archive and backups not removed");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        strLstAddZ(argList, "--dry-run");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire (dry-run) - log expired backups and archive path to remove");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("%s/%s", strPtr(archiveStanzaPath), "9.4-1")),
            true, "archive path not removed");
        TEST_RESULT_BOOL(
            (storageExistsP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152152D/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152155I/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath)))),
            true, "backup not removed");
        harnessLogResult(strPtr(strNewFmt(
            "P00   INFO: [DRY-RUN] expire full backup set: 20181119-152800F, 20181119-152800F_20181119-152152D, "
            "20181119-152800F_20181119-152155I, 20181119-152800F_20181119-152252D\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152800F_20181119-152252D\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152800F\n"
            "P00   INFO: [DRY-RUN] remove archive path: %s/%s/9.4-1", testPath(), strPtr(archiveStanzaPath))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire via backup command - archive and backups removed");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_RESULT_VOID(cmdExpire(), "via backup command: expire backups and remove archive path");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("%s/%s", strPtr(archiveStanzaPath), "9.4-1")),
            false, "archive path removed");

        harnessLogResult(strPtr(strNewFmt(
            "P00   INFO: expire full backup set: 20181119-152800F, 20181119-152800F_20181119-152152D, "
            "20181119-152800F_20181119-152155I, 20181119-152800F_20181119-152252D\n"
            "P00   INFO: remove expired backup 20181119-152800F_20181119-152252D\n"
            "P00   INFO: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00   INFO: remove expired backup 20181119-152800F\n"
            "P00   INFO: remove archive path: %s/%s/9.4-1", testPath(), strPtr(archiveStanzaPath))));

        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, backupInfoFileName, cipherTypeNone, NULL), "get backup.info");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "backup.info updated on disk");
        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderAsc), ", "),
            "20181119-152900F, 20181119-152900F_20181119-152500I", "remaining current backups correct");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - archive removed");

        archiveGenerate(storageTest, archiveStanzaPath, 1, 1, "9.4-1", "0000000100000000");
        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire remove archive path");
        harnessLogResult(
            strPtr(strNewFmt("P00   INFO: remove archive path: %s/%s/9.4-1", testPath(), strPtr(archiveStanzaPath))));

        //--------------------------------------------------------------------------------------------------------------------------
        storagePutP(storageNewWriteP(storageTest, backupInfoFileName),
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
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"10\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                    "\"db-version\":\"9.4\"}\n"
                "2={\"db-catalog-version\":201707211,\"db-control-version\":1002,\"db-system-id\":6626363367545678089,"
                    "\"db-version\":\"10\"}\n"));

        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, backupInfoFileName, cipherTypeNone, NULL), "get backup.info");

        archiveGenerate(storageTest, archiveStanzaPath, 1, 5, "9.4-1", "0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention backup no archive-start");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=2");
        strLstAddZ(argList, "--repo1-retention-archive-type=full");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup), "backup selected for retention does not have archive-start so do nothing");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(1, 5, "0000000100000000"), "nothing removed from 9.4-1/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive-type=incr");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=4");
        strLstAddZ(argList, "--repo1-retention-archive-type=incr");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup), "full count as incr but not enough backups, retention set to first full");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(2, 5, "0000000100000000"), "only removed archive prior to first full");
        harnessLogResult(
            "P00   INFO: full backup total < 4 - using oldest full backup for 9.4-1 archive retention");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("prior backup has no archive-start");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        strLstAddZ(argList, "--repo1-retention-archive-type=full");
        harnessCfgLoad(cfgCmdExpire, argList);
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
    if (testBegin("info files mismatch"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive.info has only current db with different db history id as backup.info");

        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=2");
        harnessCfgLoad(cfgCmdExpire, argList);

        storagePutP(storageNewWriteP(storageTest, backupInfoFileName),
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
                "\"backup-archive-start\":\"000000010000000000000004\",\"backup-archive-stop\":\"000000010000000000000004\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152900F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000010000000000000006\",\"backup-archive-stop\":\"000000010000000000000006\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
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
                "1={\"db-catalog-version\":201409291,\"db-control-version\":1002,\"db-system-id\":6625592122879095702,"
                    "\"db-version\":\"10\"}\n"
                "2={\"db-catalog-version\":201707211,\"db-control-version\":1002,\"db-system-id\":6626363367545678089,"
                    "\"db-version\":\"10\"}\n"));

        // Write backup.manifest so infoBackup reconstruct produces same results as backup.info on disk and removeExpiredBackup
        // will find backup directories to remove
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))),
            BUFSTRDEF("tmp"));

        storagePutP(
            storageNewWriteP(storageTest, archiveInfoFileName),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=1\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"10\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6626363367545678089,\"db-version\":\"10\"}"));

        // Create 10-1 and 10-2 although 10-2 is not realistic since the archive.info knows nothing about it - it is just to
        // confirm that nothing from disk is removed and it will also be used for the next test.
        archiveGenerate(storageTest, archiveStanzaPath, 1, 7, "10-1", "0000000100000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 7, "10-2", "0000000100000000");

        TEST_ERROR(cmdExpire(), FormatError, "archive expiration cannot continue - archive and backup history lists do not match");
        harnessLogResult(
            "P00   INFO: expire full backup 20181119-152138F\n"
            "P00   INFO: remove expired backup 20181119-152138F");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-1", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(1, 7, "0000000100000000"), "none removed from 10-1/0000000100000000");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(1, 7, "0000000100000000"), "none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive.info old history db system id not the same as backup.info");

        storagePutP(
            storageNewWriteP(storageTest, archiveInfoFileName),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"10\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6626363367545671234,\"db-version\":\"10\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}"));

        TEST_ERROR(cmdExpire(), FormatError, "archive expiration cannot continue - archive and backup history lists do not match");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive.info old history db version not the same as backup.info");

        storagePutP(
            storageNewWriteP(storageTest, archiveInfoFileName),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"10\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}"));

        TEST_ERROR(cmdExpire(), FormatError, "archive expiration cannot continue - archive and backup history lists do not match");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive.info has only current db with same db history id as backup.info");

        storagePutP(storageNewWriteP(storageTest, backupInfoFileName),
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
                "\"backup-archive-start\":\"000000010000000000000004\",\"backup-archive-stop\":\"000000010000000000000004\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152900F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000010000000000000006\",\"backup-archive-stop\":\"000000010000000000000006\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
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
                "1={\"db-catalog-version\":201409291,\"db-control-version\":1002,\"db-system-id\":6625592122879095702,"
                    "\"db-version\":\"10\"}\n"
                "2={\"db-catalog-version\":201707211,\"db-control-version\":1002,\"db-system-id\":6626363367545678089,"
                    "\"db-version\":\"10\"}\n"));

        // Write backup.manifest so infoBackup reconstruct produces same results as backup.info on disk and removeExpiredBackup
        // will find backup directories to remove
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath))),
            BUFSTRDEF("tmp"));

        storagePutP(
            storageNewWriteP(storageTest, archiveInfoFileName),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"10\"\n"
                "\n"
                "[db:history]\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}"));

        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");
        harnessCfgLoad(cfgCmdExpire, argList);

        // Here, although backup 20181119-152138F of 10-1 will be expired, the WAL in 10-1 will not since the archive.info
        // does not know about that dir. Again, not really realistic since if it is on disk and reconstructed it would have. So
        // here we are testing that things on disk that we are not aware of are not touched.
        TEST_RESULT_VOID(cmdExpire(), "Expire archive that archive.info is aware of");

        harnessLogResult(
            "P00   INFO: expire full backup 20181119-152138F\n"
            "P00   INFO: expire full backup 20181119-152800F\n"
            "P00   INFO: remove expired backup 20181119-152800F\n"
            "P00   INFO: remove expired backup 20181119-152138F");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-1", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(1, 7, "0000000100000000"), "none removed from 10-1/0000000100000000");
        TEST_RESULT_STR(
            strLstJoin(strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strPtr(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc), ", "),
            archiveExpectList(6, 7, "0000000100000000"),
            "all prior to 000000010000000000000006 removed from 10-2/0000000100000000");
    }

    // *****************************************************************************************************************************
    if (testBegin("expireAdhocBackup()"))
    {

// CSHANG Test for:
// - attempt to supply more than on --set
// - last backup removed (no resumable after or maybe a FULL resumable) MUST check to see what happens with archive - should be nothing
// - backup removed is somewhere in the middle - again what happens to archives
// - after upgrade and prio db-1 backup removed - archive for all db-1 should also be removed
// NOTE: For archive testing (meaning from running cmdExpire), will need an archive.info file on disk
//
//
// F1, D1, I1, D2, F2, Aborted I2 (F2 dependent)
// Expire D1 - F1, D2, F2 and aborted I2 remain
// Expire F2 (latest and aborted) - F1, D2 remain, confirm latest updated to D2
// Expire F1 - error

        // Create backup.info
        storagePutP(storageNewWriteP(storageTest, backupInfoFileName),
            harnessInfoChecksumZ(
                "[backup:current]\n"
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
                "\"backup-archive-start\":\"000000010000000000000001\",\"backup-archive-stop\":\"000000010000000000000004\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "\n"
                "[db]\n"
                "db-catalog-version=201909212\n"
                "db-control-version=1201\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"12\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                    "\"db-version\":\"9.4\"}\n"
                "2={\"db-catalog-version\":201909212,\"db-control-version\":1201,\"db-system-id\":6626363367545678089,"
                    "\"db-version\":\"12\"}\n"));

        // Add backup directories with manifest file including a resumable backup dependent on last backup
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE,
            strPtr(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152152D/" BACKUP_MANIFEST_FILE,
            strPtr(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152155I/" BACKUP_MANIFEST_FILE,
            strPtr(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE,
            strPtr(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE,
            strPtr(backupStanzaPath))), BUFSTRDEF("tmp"));
        // Resumable backup
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F_20181119-153000I/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
            strPtr(backupStanzaPath))),
            harnessInfoChecksumZ(
                "[backup]\n"
                "backup-label=20181119-152900F_20181119-153000I\n"
                "backup-prior=\"20181119-152900F\"\n"
                "backup-timestamp-copy-start=0\n"
                "backup-timestamp-start=0\n"
                "backup-timestamp-stop=0\n"
                "backup-type=\"incr\"\n"
                "\n"
                "[backup:db]\n"
                "db-catalog-version=201909212\n"
                "db-control-version=1201\n"
                "db-id=2\n"
                "db-system-id=6626363367545678089\n"
                "db-version=\"12\"\n"
                "\n"
                "[backup:option]\n"
                "option-archive-check=false\n"
                "option-archive-copy=false\n"
                "option-checksum-page=false\n"
                "option-compress=false\n"
                "option-compress-type=\"none\"\n"
                "option-hardlink=false\n"
                "option-online=false\n"
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"{[path]}/pg\",\"type\":\"path\"}\n"
                "\n"
                "[db]\n"
                "postgres={\"db-id\":12980,\"db-last-system-id\":12979}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"size\":3,\"timestamp\":1565282100}\n"
                "\n"
                "[target:file:default]\n"
                "group=\"postgres\"\n"
                "master=false\n"
                "mode=\"0600\"\n"
                "user=\"postgres\"\n"
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "\n"
                "[target:path:default]\n"
                "group=\"postgres\"\n"
                "mode=\"0700\"\n"
                "user=\"postgres\"\n"));

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, backupInfoFileName, cipherTypeNone, NULL), "get backup.info");

        // Create "latest" symlink
        const String *latestLink = storagePathP(storageTest, strNewFmt("%s/" BACKUP_LINK_LATEST, strPtr(backupStanzaPath)));
        String *backupLabel = strNewFmt("%s/%s/" BACKUP_MANIFEST_FILE, strPtr(backupStanzaPath), strPtr(infoBackupData(infoBackup,
            infoBackupDataTotal(infoBackup) - 1).backupLabel));
        THROW_ON_SYS_ERROR_FMT(
            symlink(strPtr(backupLabel), strPtr(latestLink)) == -1, FileOpenError, "unable to create symlink '%s' to '%s'",
            strPtr(latestLink), strPtr(backupLabel));

        // Create archive directories and generate archive
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000200000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 5, "12-2", "0000000100000000");
    }

    // *****************************************************************************************************************************
    if (testBegin("archiveIdComparator()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archiveId comparator sorting");

        StringList *list = strLstNewParam(archiveIdComparator);

        strLstAddZ(list, "10-4");
        strLstAddZ(list, "11-10");
        strLstAddZ(list, "9.6-1");

        TEST_RESULT_STR_Z(strLstJoin(strLstSort(list, sortOrderAsc), ", "), "9.6-1, 10-4, 11-10", "sort ascending");

        strLstAddZ(list, "9.4-2");
        TEST_RESULT_STR_Z(strLstJoin(strLstSort(list, sortOrderDesc), ", "), "11-10, 10-4, 9.4-2, 9.6-1", "sort descending");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
