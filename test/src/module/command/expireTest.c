/***********************************************************************************************************************************
Test Expire Command
***********************************************************************************************************************************/
#include <unistd.h>

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
            storageNewWriteP(storageTest, strNewFmt("%s/%s/%s/%s", strZ(archiveStanzaPath), archiveId, majorWal, strZ(wal))),
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

        strCatFmt(result, "%s\n", strZ(wal));
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

    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    String *backupStanzaPath = strNew("repo/backup/db");
    String *backupInfoFileName = strNewFmt("%s/backup.info", strZ(backupStanzaPath));
    String *archiveStanzaPath = strNew("repo/archive/db");
    String *archiveInfoFileName = strNewFmt("%s/archive.info", strZ(archiveStanzaPath));

    StringList *argListBase = strLstNew();
    strLstAddZ(argListBase, "--stanza=db");
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));

    StringList *argListAvoidWarn = strLstDup(argListBase);
    strLstAddZ(argListAvoidWarn, "--repo1-retention-full=1");  // avoid warning

    uint64_t timeNow = (uint64_t)time(NULL); // time in seconds since Epoch

    String *backupInfoContent = strNewFmt(
        "[backup:current]\n"
        "20181119-152138F={"
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
        "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
        "\"backup-timestamp-start\":%" PRIu64 ",\"backup-timestamp-stop\":%" PRIu64 ",\"backup-type\":\"full\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "20181119-152800F={"
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
        "\"backup-archive-start\":\"000000010000000000000004\",\"backup-archive-stop\":\"000000010000000000000004\","
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
        "\"backup-timestamp-start\":%" PRIu64 ",\"backup-timestamp-stop\":%" PRIu64 ",\"backup-type\":\"full\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "20181119-152800F_20181119-152152D={"
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000006\","
        "\"backup-archive-stop\":\"000000010000000000000006\",\"backup-info-repo-size\":2369186,"
        "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
        "\"backup-prior\":\"20181119-152800F\",\"backup-reference\":[\"20181119-152800F\"],"
        "\"backup-timestamp-start\":%" PRIu64 ",\"backup-timestamp-stop\":%" PRIu64 ",\"backup-type\":\"diff\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "20181119-152800F_20181119-152155I={"
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000007\","
        "\"backup-archive-stop\":\"000000010000000000000007\",\"backup-info-repo-size\":2369186,"
        "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
        "\"backup-prior\":\"20181119-152800F_20181119-152152D\","
        "\"backup-reference\":[\"20181119-152800F\",\"20181119-152800F_20181119-152152D\"],"
        "\"backup-timestamp-start\":%" PRIu64 ",\"backup-timestamp-stop\":%" PRIu64 ",\"backup-type\":\"incr\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "20181119-152900F={"
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
        "\"backup-archive-start\":\"000000010000000000000009\",\"backup-archive-stop\":\"000000010000000000000009\","
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
        "\"backup-timestamp-start\":%" PRIu64 ",\"backup-timestamp-stop\":%" PRIu64 ",\"backup-type\":\"full\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "20181119-152900F_20181119-152600D={"
        "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000011\","
        "\"backup-archive-stop\":\"000000010000000000000011\",\"backup-info-repo-size\":2369186,"
        "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
        "\"backup-prior\":\"20181119-152900F\",\"backup-reference\":[\"20181119-152900F\"],"
        "\"backup-timestamp-start\":%" PRIu64 ",\"backup-timestamp-stop\":%" PRIu64 ",\"backup-type\":\"diff\","
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
            "\"db-version\":\"9.4\"}", timeNow - (41 * SEC_PER_DAY), timeNow - (40 * SEC_PER_DAY), timeNow - (30 * SEC_PER_DAY),
        timeNow - (30 * SEC_PER_DAY), timeNow - (25 * SEC_PER_DAY), timeNow - (25 * SEC_PER_DAY), timeNow - (20 * SEC_PER_DAY),
        timeNow - (20 * SEC_PER_DAY), timeNow - (10 * SEC_PER_DAY), timeNow - (10 * SEC_PER_DAY), timeNow - (5 * SEC_PER_DAY),
        timeNow - (5 * SEC_PER_DAY));

    const Buffer *backupInfoBase = harnessInfoChecksumZ(strZ(backupInfoContent));

    // Sleep the remainder of the current second. If cmdExpire() gets the same time as timeNow then expiration won't work as
    // expected in the tests.
    sleepMSec(MSEC_PER_SEC - (timeMSec() % MSEC_PER_SEC));

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
        String *full1Path = strNewFmt("%s/%s", strZ(backupStanzaPath), strZ(full1));
        String *full2Path = strNewFmt("%s/%s", strZ(backupStanzaPath), strZ(full2));

        // Load Parameters
        StringList *argList = strLstDup(argListAvoidWarn);
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("%s/%s", strZ(full1Path), BACKUP_MANIFEST_FILE)), BUFSTRDEF(BOGUS_STR)),
                "full1 put manifest");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest, strNewFmt("%s/%s", strZ(full1Path), BACKUP_MANIFEST_FILE ".copy")), BUFSTRDEF(BOGUS_STR)),
            "full1 put manifest copy");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/%s", strZ(full1Path), "bogus")),
                BUFSTRDEF(BOGUS_STR)), "full1 put extra file");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, full2Path), "full2 empty");


        TEST_RESULT_VOID(expireBackup(infoBackup, full1), "expire backup with both manifest files");
        TEST_RESULT_BOOL(
            (strLstSize(storageListP(storageTest, full1Path)) && strLstExistsZ(storageListP(storageTest, full1Path), "bogus")),
            true, "full1 - only manifest files removed");

        TEST_RESULT_VOID(expireBackup(infoBackup, full2), "expire backup with no manifest - does not error");

        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL),
            "20181119-152900F\n20181119-152900F_20181119-152600D\n",
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
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=count', the repository may run out"
            " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
            " maximum.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-full set - full backup no dependencies expired");

        strLstAddZ(argList, "--repo1-retention-full=2");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 1, "retention-full=2 - one full backup expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 5, "current backups reduced by 1 full - no dependencies");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL),
            "20181119-152800F\n20181119-152800F_20181119-152152D\n20181119-152800F_20181119-152155I\n20181119-152900F\n"
                "20181119-152900F_20181119-152600D\n",
            "remaining backups correct");
        harnessLogResult("P00   INFO: expire full backup 20181119-152138F");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-full set - full backup with dependencies expired");

        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 3, "retention-full=1 - one full backup and dependencies expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "current backups reduced by 1 full and dependencies");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL), "20181119-152900F\n20181119-152900F_20181119-152600D\n",
            "remaining backups correct");
        harnessLogResult(
            "P00   INFO: expire full backup set: 20181119-152800F, 20181119-152800F_20181119-152152D, "
            "20181119-152800F_20181119-152155I");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-full set - no backups expired");

        TEST_RESULT_UINT(expireFullBackup(infoBackup), 0, "retention-full=1 - not enough backups to expire any");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL), "20181119-152900F\n20181119-152900F_20181119-152600D\n",
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
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL),
            "20181119-152138F\n20181119-152800F\n20181119-152900F\n20181119-152900F_20181119-152600D\n",
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
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL),
            "20181119-152138F\n20181119-152800F\n20181119-152900F\n20181119-152900F_20181119-152600D\n",
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
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL), "20181119-152800F\n20181119-152800F_20181119-152155D\n",
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
        String *full = strNewFmt("%s/%s", strZ(backupStanzaPath), "20181119-152100F");
        String *diff = strNewFmt("%s/%s", strZ(backupStanzaPath), "20181119-152100F_20181119-152152D");
        String *otherPath = strNewFmt("%s/%s", strZ(backupStanzaPath), "bogus");
        String *otherFile = strNewFmt("%s/%s", strZ(backupStanzaPath), "20181118-152100F_20181119-152152D.save");
        String *full1 = strNewFmt("%s/%s", strZ(backupStanzaPath), "20181119-152138F");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/%s", strZ(full), "bogus")),
                BUFSTRDEF(BOGUS_STR)), "put file");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/%s", strZ(full1), "somefile")),
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

        TEST_RESULT_STRLST_Z(
            strLstSort(storageListP(storageTest, backupStanzaPath), sortOrderAsc),
            "20181118-152100F_20181119-152152D.save\n20181119-152138F\nbackup.info\nbogus\n",
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
        TEST_RESULT_STRLST_Z(
            strLstSort(storageListP(storageTest, backupStanzaPath), sortOrderAsc),
            "20181118-152100F_20181119-152152D.save\nbackup.info\nbogus\n", "remaining file/directories correct");
    }

    // *****************************************************************************************************************************
    if (testBegin("removeExpiredArchive() & cmdExpire()"))
    {
        TEST_TITLE("check repo local");

        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--repo1-retention-full=1");  // avoid warning
        strLstAddZ(argList, "--repo1-host=/repo/not/local");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_ERROR_FMT(
            cmdExpire(), HostInvalidError, "expire command must be run on the repository host");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check stop file");

        argList = strLstDup(argListAvoidWarn);
        harnessCfgLoad(cfgCmdExpire, argList);

        // Create the stop file
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_ERROR_FMT(cmdExpire(), StopError, "stop file exists for stanza db");
        TEST_RESULT_VOID(
            storageRemoveP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), "remove the stop file");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive not set");

        // Load Parameters
        argList = strLstDup(argListBase);
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

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, false), "archive retention not set");
        harnessLogResult(
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=count', the repository may run out"
            " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
            " maximum.\n"
            "P00   INFO: option 'repo1-retention-archive' is not set - archive logs will not be expired");

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, true), "archive retention not set - retention-full-type=time");
        harnessLogResult("P00   INFO: time-based archive retention not met - archive logs will not be expired");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set - no current backups");

        // Set archive retention, archive retention type default but no current backups - code path test
        strLstAddZ(argList, "--repo1-retention-archive=4");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, false), "archive retention set, retention type default, no current backups");
        harnessLogResult(
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=count', the repository may run out"
            " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
            " maximum.");

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

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, true), "no archive on disk");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set - remove archives across timelines");

        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000100000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000200000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "10-2", "0000000100000000");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=3");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, false), "archive retention type = full (default), repo1-retention-archive=3");

        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc),
            archiveExpectList(2, 10, "0000000100000000"), "only 9.4-1/0000000100000000/000000010000000000000001 removed");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc),
            archiveExpectList(1, 10, "0000000200000000"),
            "none removed from 9.4-1/0000000200000000 - crossing timelines to play through PITR");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc),
            archiveExpectList(3, 10, "0000000100000000"),
            "000000010000000000000001 and 000000010000000000000002 removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set - latest archive not expired");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=2");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, false), "archive retention type = full (default), repo1-retention-archive=2");

        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc),
            archiveExpectList(2, 2, "0000000100000000"),
            "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc),
            archiveExpectList(2, 10, "0000000200000000"),
            "only 9.4-1/0000000200000000/000000010000000000000001 removed from major wal 2");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc),
            archiveExpectList(3, 10, "0000000100000000"),
            "none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set to lowest - keep PITR for each archiveId");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, false), "archive retention type = full (default), repo1-retention-archive=1");

        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc),
            archiveExpectList(2, 2, "0000000100000000"),
            "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc),
            archiveExpectList(2, 10, "0000000200000000"),
            "nothing removed from 9.4-1/0000000200000000 major wal 2 - each archiveId must have one backup to play through PITR");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc),
            archiveExpectList(3, 10, "0000000100000000"), "none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive, retention-archive-type=diff, retention-diff set");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=2");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        strLstAddZ(argList, "--repo1-retention-diff=2");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup, false),
            "full counts as differential and incremental associated with differential expires");

        String *result = strNew("");
        strCatFmt(
            result,
            "%s%s%s%s",
            strZ(archiveExpectList(2, 2, "0000000200000000")), strZ(archiveExpectList(4, 5, "0000000200000000")),
            strZ(archiveExpectList(7, 7, "0000000200000000")), strZ(archiveExpectList(9, 10, "0000000200000000")));

        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc),
            archiveExpectList(2, 2, "0000000100000000"),
            "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc),
            result, "all in-between removed from 9.4-1/0000000200000000 major wal 2 - last backup able to play through PITR");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc),
            archiveExpectList(3, 10, "0000000100000000"), "none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive, retention-archive-type=incr");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=4");
        strLstAddZ(argList, "--repo1-retention-archive-type=incr");
        harnessCfgLoad(cfgCmdExpire, argList);

        // Regenerate archive
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000200000000");

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, false), "differential and full count as an incremental");

        result = strNew("");
        strCatFmt(
            result,
            "%s%s%s",
            strZ(archiveExpectList(2, 2, "0000000200000000")), strZ(archiveExpectList(4, 5, "0000000200000000")),
            strZ(archiveExpectList(7, 10, "0000000200000000")));

        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc),
            archiveExpectList(2, 2, "0000000100000000"),
            "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000200000000")), sortOrderAsc),
            result, "incremental and after remain in 9.4-1/0000000200000000 major wal 2");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc),
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
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152152D/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152155I/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F_20181119-152500I/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));

        TEST_RESULT_VOID(cmdExpire(), "expire (dry-run) do not remove last backup in archive sub path or sub path");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("%s/%s", strZ(archiveStanzaPath), "9.4-1/0000000100000000")), true,
            "archive sub path not removed");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))), true,
            "backup not removed");
        harnessLogResult(
            "P00   INFO: [DRY-RUN] expire full backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152138F");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire via backup command");

        // Copy the repo to another repo
        TEST_SYSTEM_FMT("mkdir %s/repo2", testPath());
        TEST_SYSTEM_FMT("cp -r %s/repo/* %s/repo2/", testPath(), testPath());

        // Configure multi-repo and set the repo option to expire the second repo (non-default) files
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=2");
        strLstAddZ(argList, "--repo1-retention-diff=3");
        strLstAddZ(argList, "--repo1-retention-archive=2");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        hrnCfgArgKeyRawFmt(argList, cfgOptRepoPath, 2, "%s/repo2", testPath());
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 2, "2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionDiff, 2, "3");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionArchive, 2, "2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionArchiveType, 2, "diff");
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_RESULT_VOID(cmdExpire(), "via backup command: expire last backup in archive sub path and remove sub path");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, STRDEF("repo2/archive/db/9.4-1/0000000100000000")), false,
            "archive sub path removed repo2");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("%s/9.4-1/0000000100000000", strZ(archiveStanzaPath))), true,
            "archive sub path repo1 not removed");

        String *backupLabel = strNew("20181119-152138F");
        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageTest, STRDEF("repo2/backup/db/backup.info"), cipherTypeNone, NULL),
            "get backup.info repo2");
        TEST_RESULT_BOOL(strLstExists(infoBackupDataLabelList(infoBackup, NULL), backupLabel), false, "backup removed from repo2");
        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, backupInfoFileName, cipherTypeNone, NULL), "get backup.info repo1");
        TEST_RESULT_BOOL(strLstExists(infoBackupDataLabelList(infoBackup, NULL), backupLabel), true, "backup exists repo1");

        harnessLogResult(
            "P00   INFO: expire full backup 20181119-152138F\n"
            "P00   INFO: remove expired backup 20181119-152138F");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command requires repo option");

        argList = strLstDup(argListBase);
        hrnCfgArgKeyRawFmt(argList, cfgOptRepoPath, 2, "%s/repo2", testPath());
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 2, "3");

        TEST_ERROR_FMT(
            harnessCfgLoad(cfgCmdExpire, argList), OptionRequiredError, "expire command requires option: repo\n"
            "HINT: this command requires a specific repository to operate on");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - no dry run");

        // Add to previous list and specify repo
        strLstAddZ(argList, "--repo1-retention-full=2");
        strLstAddZ(argList, "--repo1-retention-diff=3");
        strLstAddZ(argList, "--repo1-retention-archive=2");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire last backup in archive sub path and remove sub path");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("%s/%s", strZ(archiveStanzaPath), "9.4-1/0000000100000000")), false,
            "archive sub path removed");
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
            storagePathExistsP(storageTest, strNewFmt("%s/%s", strZ(archiveStanzaPath), "9.4-1")), true,
            "archive path not removed");
        TEST_RESULT_BOOL(
            (storageExistsP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152152D/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152155I/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath)))),
            true, "backup not removed");
        harnessLogResult(strZ(strNewFmt(
            "P00   INFO: [DRY-RUN] expire full backup set: 20181119-152800F, 20181119-152800F_20181119-152152D, "
            "20181119-152800F_20181119-152155I, 20181119-152800F_20181119-152252D\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152800F_20181119-152252D\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152800F\n"
            "P00   INFO: [DRY-RUN] remove archive path: %s/%s/9.4-1", testPath(), strZ(archiveStanzaPath))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire via backup command - archive and backups removed");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_RESULT_VOID(cmdExpire(), "via backup command: expire backups and remove archive path");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("%s/%s", strZ(archiveStanzaPath), "9.4-1")),
            false, "archive path removed");

        harnessLogResult(strZ(strNewFmt(
            "P00   INFO: expire full backup set: 20181119-152800F, 20181119-152800F_20181119-152152D, "
            "20181119-152800F_20181119-152155I, 20181119-152800F_20181119-152252D\n"
            "P00   INFO: remove expired backup 20181119-152800F_20181119-152252D\n"
            "P00   INFO: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00   INFO: remove expired backup 20181119-152800F\n"
            "P00   INFO: remove archive path: %s/%s/9.4-1", testPath(), strZ(archiveStanzaPath))));

        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, backupInfoFileName, cipherTypeNone, NULL), "get backup.info");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "backup.info updated on disk");
        TEST_RESULT_STRLST_Z(
            strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderAsc),
            "20181119-152900F\n20181119-152900F_20181119-152500I\n", "remaining current backups correct");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - archive removed");

        archiveGenerate(storageTest, archiveStanzaPath, 1, 1, "9.4-1", "0000000100000000");
        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire remove archive path");
        harnessLogResult(strZ(strNewFmt("P00   INFO: remove archive path: %s/%s/9.4-1", testPath(), strZ(archiveStanzaPath))));

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
            removeExpiredArchive(infoBackup, false), "backup selected for retention does not have archive-start so do nothing");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc),
            archiveExpectList(1, 5, "0000000100000000"), "nothing removed from 9.4-1/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("prior backup has no archive-start");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--repo1-retention-archive=1");
        strLstAddZ(argList, "--repo1-retention-archive-type=full");
        harnessCfgLoad(cfgCmdExpire, argList);
        harnessLogLevelSet(logLevelDetail);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup, false), "backup earlier than selected for retention does not have archive-start");
        harnessLogResult(
            "P00 DETAIL: archive retention on backup 20181119-152138F, archiveId = 9.4-1, start = 000000010000000000000002,"
            " stop = 000000010000000000000002\n"
            "P00 DETAIL: archive retention on backup 20181119-152900F, archiveId = 9.4-1, start = 000000010000000000000004\n"
            "P00 DETAIL: remove archive: archiveId = 9.4-1, start = 000000010000000000000001, stop = 000000010000000000000001\n"
            "P00 DETAIL: remove archive: archiveId = 9.4-1, start = 000000010000000000000003, stop = 000000010000000000000003");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire history files - dry run");

        // Load Parameters
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=2");
        strLstAddZ(argList, "--dry-run");
        harnessCfgLoad(cfgCmdExpire, argList);

        // Create backup.info and archives spread over different timelines
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
                "20181119-152900F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000030000000000000006\",\"backup-archive-stop\":\"000000030000000000000006\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152900F_20181119-152500I={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000030000000000000008\","
                "\"backup-archive-stop\":\"000000030000000000000008\",\"backup-info-repo-size\":2369186,"
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
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
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
                "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}"));

        storagePathRemoveP(storageTest, strNewFmt("%s/10-2/0000000100000000", strZ(archiveStanzaPath)), .recurse=true);
        archiveGenerate(storageTest, archiveStanzaPath, 2, 2, "9.4-1", "0000000100000000");
        archiveGenerate(storageTest, archiveStanzaPath, 6, 10, "10-2", "0000000300000000");

        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/10-2/00000002.history", strZ(archiveStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/10-2/00000003.history", strZ(archiveStanzaPath))), BUFSTRDEF("tmp"));

        TEST_RESULT_VOID(cmdExpire(), "expire (dry-run) do not remove 00000002.history file");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNewFmt("%s/10-2/00000002.history", strZ(archiveStanzaPath))), true,
            "history file not removed");

        harnessLogResult(
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152138F, archiveId = 9.4-1, "
                "start = 000000010000000000000002\n"
            "P00 DETAIL: [DRY-RUN] no archive to remove, archiveId = 9.4-1\n"
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152900F, archiveId = 10-2, "
                "start = 000000030000000000000006\n"
            "P00 DETAIL: [DRY-RUN] no archive to remove, archiveId = 10-2\n"
            "P00 DETAIL: [DRY-RUN] remove history file: archiveId = 10-2, file = 00000002.history");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire history files - no dry run");

        // Load Parameters
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=2");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire remove 00000002.history file");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNewFmt("%s/10-2/00000002.history", strZ(archiveStanzaPath))), false,
            "00000002.history file removed");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNewFmt("%s/10-2/00000003.history", strZ(archiveStanzaPath))), true,
            "00000003.history file not removed");

        harnessLogResult(
            "P00 DETAIL: archive retention on backup 20181119-152138F, archiveId = 9.4-1, start = 000000010000000000000002\n"
            "P00 DETAIL: no archive to remove, archiveId = 9.4-1\n"
            "P00 DETAIL: archive retention on backup 20181119-152900F, archiveId = 10-2, start = 000000030000000000000006\n"
            "P00 DETAIL: no archive to remove, archiveId = 10-2\n"
            "P00 DETAIL: remove history file: archiveId = 10-2, file = 00000002.history");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire history files via backup command");

        // Load Parameters
        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=2");
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        harnessCfgLoad(cfgCmdBackup, argList);

        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/10-2/00000002.history", strZ(archiveStanzaPath))), BUFSTRDEF("tmp"));

        TEST_RESULT_VOID(cmdExpire(), "expire history files via backup command");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNewFmt("%s/10-2/00000002.history", strZ(archiveStanzaPath))), false,
            "00000002.history file removed again");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNewFmt("%s/10-2/00000003.history", strZ(archiveStanzaPath))), true,
            "00000003.history file not removed");

        harnessLogResult(
            "P00 DETAIL: archive retention on backup 20181119-152138F, archiveId = 9.4-1, start = 000000010000000000000002\n"
            "P00 DETAIL: no archive to remove, archiveId = 9.4-1\n"
            "P00 DETAIL: archive retention on backup 20181119-152900F, archiveId = 10-2, start = 000000030000000000000006\n"
            "P00 DETAIL: no archive to remove, archiveId = 10-2\n"
            "P00 DETAIL: remove history file: archiveId = 10-2, file = 00000002.history");

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
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
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
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "10-1", "0000000100000000")), sortOrderAsc),
            archiveExpectList(1, 7, "0000000100000000"), "none removed from 10-1/0000000100000000");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc),
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
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
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
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "10-1", "0000000100000000")), sortOrderAsc),
            archiveExpectList(1, 7, "0000000100000000"), "none removed from 10-1/0000000100000000");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "10-2", "0000000100000000")), sortOrderAsc),
            archiveExpectList(6, 7, "0000000100000000"),
            "all prior to 000000010000000000000006 removed from 10-2/0000000100000000");
    }

    // *****************************************************************************************************************************
    if (testBegin("expireAdhocBackup()"))
    {
        // Create backup.info
        storagePutP(storageNewWriteP(storageTest, backupInfoFileName),
            harnessInfoChecksumZ(
                "[backup:current]\n"
                "20181119-152138F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000020000000000000001\",\"backup-archive-stop\":\"000000020000000000000001\","
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
                "20181119-152850F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000004\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152900F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000010000000000000006\",\"backup-archive-stop\":\"000000010000000000000007\","
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
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152152D/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152155I/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152850F/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        // Resumable backup
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F_20181119-153000I/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
            strZ(backupStanzaPath))),
            harnessInfoChecksumZ(
                "[backup]\n"
                "backup-archive-start=\"000000010000000000000008\"\n"
                "backup-label=null\n"
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
        const String *latestLink = storagePathP(storageTest, strNewFmt("%s/latest", strZ(backupStanzaPath)));
        THROW_ON_SYS_ERROR_FMT(
            symlink(strZ(infoBackupData(infoBackup, infoBackupDataTotal(infoBackup) - 1).backupLabel), strZ(latestLink)) == -1,
            FileOpenError, "unable to create symlink '%s' to '%s'", strZ(latestLink),
            strZ(infoBackupData(infoBackup, infoBackupDataTotal(infoBackup) - 1).backupLabel));

        // Create archive info
        storagePutP(
            storageNewWriteP(storageTest, archiveInfoFileName),
            harnessInfoChecksumZ(
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"12\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6626363367545678089,\"db-version\":\"12\"}"));

        // Create archive directories and generate archive
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000200000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "12-2", "0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid backup label");

        TEST_RESULT_UINT(
            expireAdhocBackup(infoBackup, STRDEF("20201119-123456F_20201119-234567I")), 0,
            "label format OK but backup does not exist");
        harnessLogResult(
            "P00   WARN: backup 20201119-123456F_20201119-234567I does not exist\n"
            "            HINT: run the info command and confirm the backup is listed");

        TEST_ERROR(
            expireAdhocBackup(infoBackup, STRDEF(BOGUS_STR)), OptionInvalidValueError,
            "'" BOGUS_STR "' is not a valid backup label format");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire backup and dependent");

        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--set=20181119-152800F_20181119-152152D");
        harnessCfgLoad(cfgCmdExpire, argList);

        // Set the log level to detail so archive expiration messages are seen
        harnessLogLevelSet(logLevelDetail);

        TEST_RESULT_VOID(cmdExpire(), "adhoc expire only backup and dependent");
        TEST_RESULT_BOOL(
            (storageExistsP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152850F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152900F_20181119-153000I/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
                strZ(backupStanzaPath))) &&
            !storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152152D/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            !storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152155I/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath)))),
            true, "only adhoc and dependents removed - resumable and all other backups remain");
        TEST_RESULT_STR(storageInfoP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")).linkDestination,
            STRDEF("20181119-152900F"), "latest link not updated");
        harnessLogResult(
            "P00   INFO: expire adhoc backup set: 20181119-152800F_20181119-152152D, 20181119-152800F_20181119-152155I\n"
            "P00   INFO: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00 DETAIL: archive retention on backup 20181119-152138F, archiveId = 9.4-1, start = 000000020000000000000001,"
            " stop = 000000020000000000000001\n"
            "P00 DETAIL: archive retention on backup 20181119-152800F, archiveId = 9.4-1, start = 000000020000000000000002\n"
            "P00 DETAIL: no archive to remove, archiveId = 9.4-1\n"
            "P00 DETAIL: archive retention on backup 20181119-152850F, archiveId = 12-2, start = 000000010000000000000002,"
            " stop = 000000010000000000000004\n"
            "P00 DETAIL: archive retention on backup 20181119-152900F, archiveId = 12-2, start = 000000010000000000000006\n"
            "P00 DETAIL: remove archive: archiveId = 12-2, start = 000000010000000000000001, stop = 000000010000000000000001\n"
            "P00 DETAIL: remove archive: archiveId = 12-2, start = 000000010000000000000005, stop = 000000010000000000000005");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire full and archive (no dependents)");

        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--set=20181119-152138F");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "adhoc expire full backup");
        TEST_RESULT_BOOL(
            (storageExistsP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152850F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152900F_20181119-153000I/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
                strZ(backupStanzaPath))) &&
            !storageExistsP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath)))),
            true, "only adhoc full removed");
        harnessLogResult(
            "P00   INFO: expire adhoc backup 20181119-152138F\n"
            "P00   INFO: remove expired backup 20181119-152138F\n"
            "P00 DETAIL: archive retention on backup 20181119-152800F, archiveId = 9.4-1, start = 000000020000000000000002\n"
            "P00 DETAIL: remove archive: archiveId = 9.4-1, start = 000000020000000000000001, stop = 000000020000000000000001\n"
            "P00 DETAIL: archive retention on backup 20181119-152850F, archiveId = 12-2, start = 000000010000000000000002,"
            " stop = 000000010000000000000004\n"
            "P00 DETAIL: archive retention on backup 20181119-152900F, archiveId = 12-2, start = 000000010000000000000006\n"
            "P00 DETAIL: no archive to remove, archiveId = 12-2");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire latest and resumable");

        argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--set=20181119-152900F");
        harnessCfgLoad(cfgCmdExpire, argList);

        String *archiveRemaining = strNew("");
        strCatFmt(
            archiveRemaining, "%s%s", strZ(archiveExpectList(2, 4, "0000000100000000")),
            strZ(archiveExpectList(6, 10, "0000000100000000")));

        TEST_RESULT_VOID(cmdExpire(), "adhoc expire latest backup");
        TEST_RESULT_BOOL(
            (storageExistsP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            !storageExistsP(
                storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            !storageExistsP(
                storageTest, strNewFmt("%s/20181119-152900F_20181119-153000I/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
                strZ(backupStanzaPath)))),
            true, "latest and resumable removed");
        harnessLogResult(
            "P00   WARN: expiring latest backup 20181119-152900F - the ability to perform point-in-time-recovery (PITR) may be"
            " affected\n"
            "            HINT: non-default settings for 'repo1-retention-archive'/'repo1-retention-archive-type'"
            " (even in prior expires) can cause gaps in the WAL.\n"
            "P00   INFO: expire adhoc backup 20181119-152900F\n"
            "P00   INFO: remove expired backup 20181119-152900F_20181119-153000I\n"
            "P00   INFO: remove expired backup 20181119-152900F\n"
            "P00 DETAIL: archive retention on backup 20181119-152800F, archiveId = 9.4-1, start = 000000020000000000000002\n"
            "P00 DETAIL: no archive to remove, archiveId = 9.4-1\n"
            "P00 DETAIL: archive retention on backup 20181119-152850F, archiveId = 12-2, start = 000000010000000000000002\n"
            "P00 DETAIL: no archive to remove, archiveId = 12-2");
        TEST_RESULT_STR(storageInfoP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")).linkDestination,
            STRDEF("20181119-152850F"), "latest link updated");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "12-2", "0000000100000000")), sortOrderAsc),
            archiveRemaining,
            "no archives removed from latest except what was already removed");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on expire last full backup in current db-id");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--set=20181119-152850F");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_ERROR(
            cmdExpire(), BackupSetInvalidError,
            "full backup 20181119-152850F cannot be expired until another full backup has been created");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("allow adhoc expire on last full backup in prior db-id");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--set=20181119-152800F");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "adhoc expire last prior db-id backup");
        TEST_RESULT_BOOL(
            (storageExistsP(storageTest, strNewFmt("%s/20181119-152850F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            !storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))) &&
            !storageExistsP(
                storageTest, strNewFmt("%s/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath)))),
            true, "only last prior backup removed");
        harnessLogResult(
            strZ(strNewFmt(
            "P00   INFO: expire adhoc backup set: 20181119-152800F, 20181119-152800F_20181119-152252D\n"
            "P00   INFO: remove expired backup 20181119-152800F_20181119-152252D\n"
            "P00   INFO: remove expired backup 20181119-152800F\n"
            "P00   INFO: remove archive path: %s/repo/archive/db/9.4-1\n"
            "P00 DETAIL: archive retention on backup 20181119-152850F, archiveId = 12-2, start = 000000010000000000000002\n"
            "P00 DETAIL: no archive to remove, archiveId = 12-2", testPath())));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on expire last full backup on disk");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--set=20181119-152850F");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_ERROR(
            cmdExpire(), BackupSetInvalidError,
            "full backup 20181119-152850F cannot be expired until another full backup has been created");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("adhoc dry-run");

        // Create backup.info
        storagePutP(storageNewWriteP(storageTest, backupInfoFileName),
            harnessInfoChecksumZ(
                "[backup:current]\n"
                "20181119-152850F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000004\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152850F_20181119-152252D={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000006\","
                "\"backup-archive-stop\":\"000000010000000000000007\",\"backup-info-repo-size\":2369186,"
                "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
                "\"backup-prior\":\"20181119-152850F\",\"backup-reference\":[\"20181119-152850F\"],"
                "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
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

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--set=20181119-152850F_20181119-152252D");
        strLstAddZ(argList, "--dry-run");
        harnessCfgLoad(cfgCmdExpire, argList);

        // Load the backup info. Do not store a manifest file for the adhoc backup for code coverage
        TEST_ASSIGN(infoBackup, infoBackupLoadFile(storageTest, backupInfoFileName, cipherTypeNone, NULL), "get backup.info");

        // Create the manifest file to create the directory then remove the file for code coverage
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152850F_20181119-152252D/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storageRemoveP(
            storageTest, strNewFmt("%s/20181119-152850F_20181119-152252D/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath)));

        String *adhocBackupLabel = strNew("20181119-152850F_20181119-152252D");
        TEST_RESULT_UINT(expireAdhocBackup(infoBackup, adhocBackupLabel), 1, "adhoc expire last dependent backup");
        TEST_RESULT_VOID(removeExpiredBackup(infoBackup, adhocBackupLabel), "code coverage: removeExpireBackup with no manifests");
        harnessLogResult(
            "P00   WARN: [DRY-RUN] expiring latest backup 20181119-152850F_20181119-152252D - the ability to perform"
            " point-in-time-recovery (PITR) may be affected\n"
            "            HINT: non-default settings for 'repo1-retention-archive'/'repo1-retention-archive-type'"
            " (even in prior expires) can cause gaps in the WAL.\n"
            "P00   INFO: [DRY-RUN] expire adhoc backup 20181119-152850F_20181119-152252D\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152850F_20181119-152252D");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("resumable possibly based on adhoc expire backup");

        argList = strLstDup(argListAvoidWarn);
        strLstAddZ(argList, "--set=20181119-152850F_20181119-152252D");
        harnessCfgLoad(cfgCmdExpire, argList);

        // Create backup.info
        storagePutP(storageNewWriteP(storageTest, backupInfoFileName),
            harnessInfoChecksumZ(
                "[backup:current]\n"
                "20181119-152850F={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
                "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000004\","
                "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
                "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
                "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
                "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
                "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
                "20181119-152850F_20181119-152252D={"
                "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000006\","
                "\"backup-archive-stop\":\"000000010000000000000007\",\"backup-info-repo-size\":2369186,"
                "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
                "\"backup-prior\":\"20181119-152850F\",\"backup-reference\":[\"20181119-152850F\"],"
                "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
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

        // Adhoc backup and resumable backup manifests
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152850F_20181119-152252D/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152850F_20181200-152252D/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
            strZ(backupStanzaPath))),
            harnessInfoChecksumZ(
                "[backup]\n"
                "backup-archive-start=\"000000010000000000000009\"\n"
                "backup-label=null\n"
                "backup-prior=\"20181119-152850F\"\n"
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

        archiveGenerate(storageTest, archiveStanzaPath, 2, 10, "12-2", "0000000100000000");

        TEST_RESULT_VOID(cmdExpire(), "adhoc expire latest with resumable possibly based on it");
        harnessLogResult(
            "P00   WARN: expiring latest backup 20181119-152850F_20181119-152252D - the ability to perform point-in-time-recovery"
            " (PITR) may be affected\n"
            "            HINT: non-default settings for 'repo1-retention-archive'/'repo1-retention-archive-type'"
            " (even in prior expires) can cause gaps in the WAL.\n"
            "P00   INFO: expire adhoc backup 20181119-152850F_20181119-152252D\n"
            "P00   INFO: remove expired backup 20181119-152850F_20181119-152252D\n"
            "P00 DETAIL: archive retention on backup 20181119-152850F, archiveId = 12-2, start = 000000010000000000000002\n"
            "P00 DETAIL: no archive to remove, archiveId = 12-2");

        harnessLogLevelReset();
    }

    // *****************************************************************************************************************************
    if (testBegin("expireTimeBasedBackup()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no current backups");

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6625592122879095702\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                "\"db-version\":\"9.4\"}"))), "empty backup.info");

        TEST_RESULT_UINT(expireTimeBasedBackup(infoBackup, (time_t)(timeNow - (40 * SEC_PER_DAY))), 0, "no backups to expire");

        //--------------------------------------------------------------------------------------------------------------------------
        // Set up
        StringList *argListTime = strLstDup(argListBase);
        strLstAddZ(argListTime, "--repo1-retention-full-type=time");

        // Create backup.info and archive.info
        storagePutP(storageNewWriteP(storageTest, backupInfoFileName), backupInfoBase);
        storagePutP(
            storageNewWriteP(storageTest, archiveInfoFileName),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=1\n"
                "db-system-id=6625592122879095702\n"
                "db-version=\"9.4\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}"));

        // Write backup.manifest so infoBackup reconstruct produces same results as backup.info on disk
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152138F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152152D/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152800F_20181119-152155I/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F/" BACKUP_MANIFEST_FILE, strZ(backupStanzaPath))),
            BUFSTRDEF("tmp"));
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/20181119-152900F_20181119-152600D/" BACKUP_MANIFEST_FILE,
            strZ(backupStanzaPath))), BUFSTRDEF("tmp"));

        // Genreate archive for backups in backup.info
        archiveGenerate(storageTest, archiveStanzaPath, 1, 11, "9.4-1", "0000000100000000");

        // Set the log level to detail so archive expiration messages are seen
        harnessLogLevelSet(logLevelDetail);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("oldest backup not expired");

        StringList *argList = strLstDup(argListTime);
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(backupInfoBase)), "get backup.info");
        TEST_RESULT_VOID(cmdExpire(), "repo-retention-full not set for time-based");
        harnessLogResult(
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=time', the repository may run out"
            " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
            " maximum.\n"
            "P00   INFO: time-based archive retention not met - archive logs will not be expired");

        // Stop time equals retention time
        TEST_RESULT_UINT(
            expireTimeBasedBackup(infoBackup, (time_t)(timeNow - (40 * SEC_PER_DAY))), 0,
            "oldest backup stop time equals retention time");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL),
            "20181119-152138F\n20181119-152800F\n20181119-152800F_20181119-152152D\n20181119-152800F_20181119-152155I\n"
            "20181119-152900F\n20181119-152900F_20181119-152600D\n", "no backups expired");

        // Add a time period
        strLstAddZ(argList, "--repo1-retention-full=35");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "oldest backup older but other backups too young");
        TEST_RESULT_STRLST_STR(
            strLstSort(storageListP(
                storageTest, strNewFmt("%s/%s/%s", strZ(archiveStanzaPath), "9.4-1", "0000000100000000")), sortOrderAsc),
            archiveExpectList(1, 11, "0000000100000000"),
            "no archives expired");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL),
            "20181119-152138F\n20181119-152800F\n20181119-152800F_20181119-152152D\n20181119-152800F_20181119-152155I\n"
            "20181119-152900F\n20181119-152900F_20181119-152600D\n", "no backups expired");
        harnessLogResult("P00   INFO: time-based archive retention not met - archive logs will not be expired");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("oldest backup expired");

        argList = strLstDup(argListTime);
        strLstAddZ(argList, "--repo1-retention-full=30");
        strLstAddZ(argList, "--dry-run");
        harnessCfgLoad(cfgCmdExpire, argList);
        TEST_RESULT_VOID(cmdExpire(), "only oldest backup expired - dry-run");
        harnessLogResult(
            "P00   INFO: [DRY-RUN] expire time-based backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152138F\n"
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152800F, archiveId = 9.4-1,"
            " start = 000000010000000000000004\n"
            "P00 DETAIL: [DRY-RUN] remove archive: archiveId = 9.4-1, start = 000000010000000000000001,"
            " stop = 000000010000000000000003");

        strLstAddZ(argList, "--repo1-retention-archive=9999999");
        harnessCfgLoad(cfgCmdExpire, argList);
        TEST_RESULT_VOID(cmdExpire(), "only oldest backup expired - dry-run, retention-archive set to max, no archives expired");
        harnessLogResult(
            "P00   INFO: [DRY-RUN] expire time-based backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152138F");

        argList = strLstDup(argListTime);
        strLstAddZ(argList, "--repo1-retention-full=30");
        strLstAddZ(argList, "--repo1-retention-archive=1"); // 1-day: expire all non-essential archive prior to newest full backup
        strLstAddZ(argList, "--dry-run");
        harnessCfgLoad(cfgCmdExpire, argList);
        TEST_RESULT_VOID(cmdExpire(), "only oldest backup expired but retention archive set lower - dry-run");
        harnessLogResult(
            "P00   INFO: [DRY-RUN] expire time-based backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152138F\n"
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152800F, archiveId = 9.4-1,"
            " start = 000000010000000000000004, stop = 000000010000000000000004\n"
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152800F_20181119-152152D, archiveId = 9.4-1,"
            " start = 000000010000000000000006, stop = 000000010000000000000006\n"
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152800F_20181119-152155I, archiveId = 9.4-1,"
            " start = 000000010000000000000007, stop = 000000010000000000000007\n"
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152900F, archiveId = 9.4-1,"
            " start = 000000010000000000000009\n"
            "P00 DETAIL: [DRY-RUN] remove archive: archiveId = 9.4-1, start = 000000010000000000000001,"
            " stop = 000000010000000000000003\n"
            "P00 DETAIL: [DRY-RUN] remove archive: archiveId = 9.4-1, start = 000000010000000000000005,"
            " stop = 000000010000000000000005\n"
            "P00 DETAIL: [DRY-RUN] remove archive: archiveId = 9.4-1, start = 000000010000000000000008,"
            " stop = 000000010000000000000008");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repo1-retention-archive-type=diff");

        argList = strLstDup(argListTime);
        strLstAddZ(argList, "--repo1-retention-full=30");
        strLstAddZ(argList, "--repo1-retention-archive-type=diff");
        strLstAddZ(argList, "--repo1-retention-archive=1"); // 1-diff: expire all non-essential archive prior to newest diff backup
        strLstAddZ(argList, "--dry-run");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "only oldest backup expired, retention archive is DIFF - dry-run");
        harnessLogResult(
            "P00   WARN: [DRY-RUN] option 'repo1-retention-diff' is not set for 'repo1-retention-archive-type=diff'\n"
            "            HINT: to retain differential backups indefinitely (without warning), set option 'repo1-retention-diff'"
            " to the maximum.\n"
            "P00   INFO: [DRY-RUN] expire time-based backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] remove expired backup 20181119-152138F\n"
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152800F, archiveId = 9.4-1,"
            " start = 000000010000000000000004, stop = 000000010000000000000004\n"
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152800F_20181119-152152D, archiveId = 9.4-1,"
            " start = 000000010000000000000006, stop = 000000010000000000000006\n"
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152800F_20181119-152155I, archiveId = 9.4-1,"
            " start = 000000010000000000000007, stop = 000000010000000000000007\n"
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152900F, archiveId = 9.4-1,"
            " start = 000000010000000000000009, stop = 000000010000000000000009\n"
            "P00 DETAIL: [DRY-RUN] archive retention on backup 20181119-152900F_20181119-152600D, archiveId = 9.4-1,"
            " start = 000000010000000000000011\n"
            "P00 DETAIL: [DRY-RUN] remove archive: archiveId = 9.4-1, start = 000000010000000000000001,"
            " stop = 000000010000000000000003\n"
            "P00 DETAIL: [DRY-RUN] remove archive: archiveId = 9.4-1, start = 000000010000000000000005,"
            " stop = 000000010000000000000005\n"
            "P00 DETAIL: [DRY-RUN] remove archive: archiveId = 9.4-1, start = 000000010000000000000008,"
            " stop = 000000010000000000000008\n"
            "P00 DETAIL: [DRY-RUN] remove archive: archiveId = 9.4-1, start = 000000010000000000000010,"
            " stop = 000000010000000000000010");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire oldest full");

        argList = strLstDup(argListTime);
        strLstAddZ(argList, "--repo1-retention-full=25");
        harnessCfgLoad(cfgCmdExpire, argList);

        // Expire oldest from backup.info only, leaving the backup and archives on disk then save backup.info without oldest backup
        TEST_RESULT_UINT(expireTimeBasedBackup(infoBackup, (time_t)(timeNow - (25 * SEC_PER_DAY))), 1, "expire oldest backup");
        TEST_RESULT_VOID(
            infoBackupSaveFile(infoBackup, storageTest, backupInfoFileName, cipherTypeNone, NULL),
            "save backup.info without oldest");
        harnessLogResult("P00   INFO: expire time-based backup 20181119-152138F");
        TEST_RESULT_VOID(cmdExpire(), "only oldest backup expired");
        harnessLogResult(
            "P00   INFO: remove expired backup 20181119-152138F\n"
            "P00 DETAIL: archive retention on backup 20181119-152800F, archiveId = 9.4-1, start = 000000010000000000000004\n"
            "P00 DETAIL: remove archive: archiveId = 9.4-1, start = 000000010000000000000001, stop = 000000010000000000000003");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("newest backup - retention met but must keep one");

        argList = strLstDup(argListTime);
        strLstAddZ(argList, "--repo1-retention-full=1");
        harnessCfgLoad(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire all but newest");
        harnessLogResult(
            "P00   INFO: expire time-based backup set: 20181119-152800F, 20181119-152800F_20181119-152152D,"
            " 20181119-152800F_20181119-152155I\n"
            "P00   INFO: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00   INFO: remove expired backup 20181119-152800F\n"
            "P00 DETAIL: archive retention on backup 20181119-152900F, archiveId = 9.4-1, start = 000000010000000000000009\n"
            "P00 DETAIL: remove archive: archiveId = 9.4-1, start = 000000010000000000000004, stop = 000000010000000000000008");

        harnessLogLevelReset();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
