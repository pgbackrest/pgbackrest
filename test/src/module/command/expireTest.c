/***********************************************************************************************************************************
Test Expire Command
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/io/bufferRead.h"
#include "command/backup/common.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
void
archiveGenerate( // CSHANG Is it possible to use storageRepoWrite when calling this function?
    Storage *storageTest, const String *archiveStanzaPath, const unsigned int start, unsigned int end, const char *archiveId,
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
    String *result = strNew();

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
// CSHANG - try to avoid using storageTest - if use storageRepo, there are built in stuff for me
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);
// CSHANG - paths as constant - get rid of strings and use constants like INFO_BACKUP_PATH_FILE and INFO_ARCHIVE_PATH_FILE

    const String *archiveStanzaPath = STRDEF("repo/archive/db");

    StringList *argListBase = strLstNew();
    hrnCfgArgRawZ(argListBase, cfgOptStanza, "db");
    hrnCfgArgRawZ(argListBase, cfgOptRepoPath, TEST_PATH_REPO);

    // Initialize a configuration list that avoids the retention warning
    StringList *argListAvoidWarn = strLstDup(argListBase);
    hrnCfgArgRawZ(argListAvoidWarn, cfgOptRepoRetentionFull, "1");

    // Set time in seconds since Epoch
    uint64_t timeNow = (uint64_t)time(NULL);

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
        "\"backup-timestamp-start\":%" PRIu64 ",\"backup-timestamp-stop\":%" PRIu64 ",\"backup-type\":\"incr\","
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

        // Load Parameters
        StringList *argList = strLstDup(argListAvoidWarn);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        // Write out manifest files so they exist for full backup
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT);

        // Put extra file in 20181119-152138F backup directory
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F/" BOGUS_STR);
        TEST_RESULT_VOID(storagePathCreateP(storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/20181119-152800F")), "full2 empty");

        // Expire 20181119-152138F - only manifest files removed (extra file remains)
        TEST_RESULT_VOID(expireBackup(infoBackup, STRDEF("20181119-152138F"), 0), "expire backup with both manifest files");
        TEST_STORAGE_LIST(storageRepo(), STORAGE_REPO_BACKUP "/20181119-152138F", BOGUS_STR "\n",
            .comment = "file in backup remains - only manifest files removed");

        TEST_RESULT_VOID(
            expireBackup(infoBackup, STRDEF("20181119-152800F"), 0), "expire backup with no manifest - does not error");

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
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireFullBackup(infoBackup, 0), 0, "retention-full not set");
        TEST_RESULT_LOG(
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=count', the repository may run out"
                " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
                " maximum.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-full set - full backup no dependencies expired");

        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "2");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireFullBackup(infoBackup, 0), 1, "retention-full=2 - one full backup expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 5, "current backups reduced by 1 full - no dependencies");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL),
            "20181119-152800F\n20181119-152800F_20181119-152152D\n20181119-152800F_20181119-152155I\n20181119-152900F\n"
                "20181119-152900F_20181119-152600D\n",
            "remaining backups correct"); // CSHANG maybe remove this comment when comment becomes optional - CAN'T B/C empty line
        TEST_RESULT_LOG("P00   INFO: repo1: expire full backup 20181119-152138F");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-full set - full backup with dependencies expired");

        argList = strLstDup(argListBase);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireFullBackup(infoBackup, 0), 3, "retention-full=1 - one full backup and dependencies expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "current backups reduced by 1 full and dependencies");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL), "20181119-152900F\n20181119-152900F_20181119-152600D\n",
            "remaining backups correct"); // CSHANG maybe remove this comment when comment becomes optional
        TEST_RESULT_LOG(
            "P00   INFO: repo1: expire full backup set 20181119-152800F, 20181119-152800F_20181119-152152D,"
                " 20181119-152800F_20181119-152155I");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-full set - no backups expired");

        TEST_RESULT_UINT(expireFullBackup(infoBackup, 0), 0, "retention-full=1 - not enough backups to expire any");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL), "20181119-152900F\n20181119-152900F_20181119-152600D\n",
            "remaining backups correct"); // CSHANG maybe remove this comment when comment becomes optional
    }

    // *****************************************************************************************************************************
    if (testBegin("expireDiffBackup()"))
    {
        // Create backup.info
        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(backupInfoBase)), "get backup.info");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-diff not set - nothing expired");

        // Load Parameters
        StringList *argList = strLstDup(argListAvoidWarn);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireDiffBackup(infoBackup, 0), 0, "retention-diff not set - nothing expired"); // CSHANG maybe remove this comment when comment becomes optional
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 6, "current backups not expired"); // CSHANG maybe remove this comment when comment becomes optional

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-diff set - nothing yet to expire");

        // Add retention-diff
        StringList *argListTemp = strLstDup(argList);
        hrnCfgArgRawZ(argListTemp, cfgOptRepoRetentionDiff, "6");
        HRN_CFG_LOAD(cfgCmdExpire, argListTemp);

        TEST_RESULT_UINT(expireDiffBackup(infoBackup, 0), 0, "retention-diff set - too soon to expire"); // CSHANG maybe remove this comment when comment becomes optional
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 6, "current backups not expired"); // CSHANG maybe remove this comment when comment becomes optional

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-diff set - diff and dependent incr expired");

        hrnCfgArgRawZ(argList, cfgOptRepoRetentionDiff, "2");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireDiffBackup(infoBackup, 0), 2, "retention-diff=2 - full considered in diff");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 4, "current backups reduced by 2 - one diff and its dependent increment");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL),
            "20181119-152138F\n20181119-152800F\n20181119-152900F\n20181119-152900F_20181119-152600D\n",
            "remaining backups correct"); // CSHANG maybe remove this comment when comment becomes optional
        TEST_RESULT_LOG(
            "P00   INFO: repo1: expire diff backup set 20181119-152800F_20181119-152152D, 20181119-152800F_20181119-152155I");

        TEST_RESULT_UINT(expireDiffBackup(infoBackup, 0), 0, "retention-diff=2 but no more to expire");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 4, "current backups not reduced");

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
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionDiff, "1");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_UINT(expireDiffBackup(infoBackup, 0), 1, "retention-diff set - only oldest diff expired");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "current backups reduced by one");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL), "20181119-152800F\n20181119-152800F_20181119-152155D\n",
            "remaining backups correct"); // CSHANG maybe remove this comment when comment becomes optional
        TEST_RESULT_LOG("P00   INFO: repo1: expire diff backup 20181119-152800F_20181119-152152D");
    }

    // *****************************************************************************************************************************
    if (testBegin("removeExpiredBackup()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove expired backup from disk - backup not in current backup");

        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        // Create backup.info
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
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
                "\"db-version\":\"9.4\"}");

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL), "get backup.info");

        // Put some files in the backup directories
        HRN_STORAGE_PUT_Z(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152100F/" BOGUS_STR, BOGUS_STR);
        HRN_STORAGE_PUT_Z(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F/" BOGUS_STR "2", BOGUS_STR);
        HRN_STORAGE_PATH_CREATE(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152100F_20181119-152152D",
            .comment = "empty backup directory must not error on delete");
        HRN_STORAGE_PATH_CREATE(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/" BOGUS_STR, .comment = "other path must not be removed");
        HRN_STORAGE_PUT_Z(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181118-152100F_20181119-152152D.save", BOGUS_STR,
            .comment = "directory look-alike file must not be removed");

        TEST_RESULT_VOID(removeExpiredBackup(infoBackup, NULL, 0), "remove backups not in backup.info current");

        TEST_RESULT_LOG(
            "P00   INFO: repo1: remove expired backup 20181119-152100F_20181119-152152D\n"
            "P00   INFO: repo1: remove expired backup 20181119-152100F");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP,
            "20181118-152100F_20181119-152152D.save\n"
            "20181119-152138F/\n"
            "20181119-152138F/BOGUS2\n"
            BOGUS_STR "/\n"
            "backup.info\n");

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

        TEST_RESULT_VOID(removeExpiredBackup(infoBackup, NULL, 0), "remove backups - backup.info current empty");

        TEST_RESULT_LOG("P00   INFO: repo1: remove expired backup 20181119-152138F");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP,
            "20181118-152100F_20181119-152152D.save\n"
            BOGUS_STR "/\n"
            "backup.info\n");
    }

    // *****************************************************************************************************************************
    if (testBegin("removeExpiredArchive() & cmdExpire()"))
    {
        TEST_TITLE("check repo local");

        // Load Parameters
        StringList *argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptRepoHost, "/repo/not/local");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_ERROR(cmdExpire(), HostInvalidError, "expire command must be run on the repository host");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check stop file");

        argList = strLstDup(argListAvoidWarn);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        // Create the stop file
        HRN_STORAGE_PUT_EMPTY(storageLocalWrite(), strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))));

        TEST_ERROR(cmdExpire(), StopError, "stop file exists for stanza db");

        // Remove the stop file
        TEST_STORAGE_REMOVE(storageLocalWrite(), strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive not set");

        // Load Parameters
        argList = strLstDup(argListBase);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

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

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, false, 0), "archive retention not set");
        TEST_RESULT_LOG(
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=count', the repository may run out"
                " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
                " maximum.\n"
            "P00   INFO: option 'repo1-retention-archive' is not set - archive logs will not be expired");

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, true, 0), "archive retention not set - retention-full-type=time");
        TEST_RESULT_LOG("P00   INFO: repo1: time-based archive retention not met - archive logs will not be expired");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set - no current backups");

        // Set archive retention, archive retention type default but no current backups - code path test
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "4");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup, false, 0), "archive retention set, retention type default, no current backups");
        TEST_RESULT_LOG(
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=count', the repository may run out"
                " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
                " maximum.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set - no archive on disk");

        // Create backup.info with current backups spread over different timelines
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
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
                "\"db-version\":\"10\"}\n");

        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL), "get backup.info");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"10\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}");

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, true, 0), "no archive on disk");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set - remove archives across timelines");

        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000100000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000200000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "10-2", "0000000100000000");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "3");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup, false, 0), "archive retention type = full (default), repo1-retention-archive=3");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000", strZ(archiveExpectList(2, 10, "0000000100000000")),
            .comment = "only 9.4-1/0000000100000000/000000010000000000000001 removed");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000200000000", strZ(archiveExpectList(1, 10, "0000000200000000")),
            .comment = "none removed from 9.4-1/0000000200000000 - crossing timelines to play through PITR");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/10-2/0000000100000000", strZ(archiveExpectList(3, 10, "0000000100000000")),
            .comment = "000000010000000000000001 and 000000010000000000000002 removed from 10-2/0000000100000000");
        TEST_RESULT_LOG(
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000010000000000000001, stop = 000000010000000000000001\n"
            "P00   INFO: repo1: 10-2 remove archive, start = 000000010000000000000001, stop = 000000010000000000000002");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set - latest archive not expired");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "2");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup, false, 0), "archive retention type = full (default), repo1-retention-archive=2");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000", strZ(archiveExpectList(2, 2, "0000000100000000")),
            .comment = "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000200000000", strZ(archiveExpectList(2, 10, "0000000200000000")),
            .comment = "only 9.4-1/0000000200000000/000000010000000000000001 removed from major wal 2");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/10-2/0000000100000000", strZ(archiveExpectList(3, 10, "0000000100000000")),
            .comment = "none removed from 10-2/0000000100000000");
        // Only last 2 full backups and dependents are PITRable, first full backup is not
        TEST_RESULT_LOG(
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000010000000000000003, stop = 000000020000000000000001\n"
            "P00   INFO: repo1: 10-2 no archive to remove");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive set to lowest - keep PITR for each archiveId");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "1");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup, false, 0), "archive retention type = full (default), repo1-retention-archive=1");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000", strZ(archiveExpectList(2, 2, "0000000100000000")),
            .comment = "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000200000000", strZ(archiveExpectList(2, 10, "0000000200000000")),
            .comment = "nothing removed from 9.4-1/0000000200000000 major wal 2 - each archiveId must have one backup to play"
            " through PITR");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/10-2/0000000100000000", strZ(archiveExpectList(3, 10, "0000000100000000")),
            .comment = "none removed from 10-2/0000000100000000");
        TEST_RESULT_LOG(
            "P00   INFO: repo1: 9.4-1 no archive to remove\n"
            "P00   INFO: repo1: 10-2 no archive to remove");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive, retention-archive-type=diff, retention-diff set");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "2");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchiveType, "diff");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionDiff, "2");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup, false, 0),
            "full counts as differential and incremental associated with differential expires");

        String *result = strNewFmt(
            "%s%s%s%s", strZ(archiveExpectList(2, 2, "0000000200000000")), strZ(archiveExpectList(4, 5, "0000000200000000")),
            strZ(archiveExpectList(7, 7, "0000000200000000")), strZ(archiveExpectList(9, 10, "0000000200000000")));

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000", strZ(archiveExpectList(2, 2, "0000000100000000")),
            .comment = "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000200000000", strZ(result),
            .comment = "all in-between removed from 9.4-1/0000000200000000 major wal 2 - last backup able to play through PITR");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/10-2/0000000100000000", strZ(archiveExpectList(3, 10, "0000000100000000")),
            .comment = "none removed from 10-2/0000000100000000");
        TEST_RESULT_LOG(
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000020000000000000003, stop = 000000020000000000000003\n"
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000020000000000000006, stop = 000000020000000000000006\n"
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000020000000000000008, stop = 000000020000000000000008\n"
            "P00   INFO: repo1: 10-2 no archive to remove");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention-archive, retention-archive-type=incr");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "4");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchiveType, "incr");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        // Regenerate archive
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000200000000");

        TEST_RESULT_VOID(removeExpiredArchive(infoBackup, false, 0), "differential and full count as an incremental");

        result = strNewFmt(
            "%s%s%s", strZ(archiveExpectList(2, 2, "0000000200000000")), strZ(archiveExpectList(4, 5, "0000000200000000")),
            strZ(archiveExpectList(7, 10, "0000000200000000")));

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000", strZ(archiveExpectList(2, 2, "0000000100000000")),
            .comment = "only 9.4-1/0000000100000000/000000010000000000000002 remains in major wal 1");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000200000000", strZ(result),
            .comment = "incremental and after remain in 9.4-1/0000000200000000 major wal 2");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/10-2/0000000100000000", strZ(archiveExpectList(3, 10, "0000000100000000")),
            .comment = "none removed from 10-2/0000000100000000");
        TEST_RESULT_LOG(
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000020000000000000001, stop = 000000020000000000000001\n"
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000020000000000000003, stop = 000000020000000000000003\n"
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000020000000000000006, stop = 000000020000000000000006\n"
            "P00   INFO: repo1: 10-2 no archive to remove");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - dry run");

        // Write backup.manifest so infoBackup reconstruct produces same results as backup.info on disk
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F_20181119-152152D/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F_20181119-152155I/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F_20181119-152500I/" BACKUP_MANIFEST_FILE);

        argList = strLstDup(argListBase);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "2");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionDiff, "3");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "2");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchiveType, "diff");
        hrnCfgArgRawBool(argList, cfgOptDryRun, true);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire (dry-run) do not remove last backup in archive sub path or sub path");

        TEST_RESULT_BOOL(
            storagePathExistsP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000")), true,
            "archive sub path not removed because of dry-run, else would be removed");
        TEST_RESULT_BOOL(
            storageExistsP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/20181119-152138F/" BACKUP_MANIFEST_FILE)), true,
            "backup not removed because of dry-run, else would be removed");
        TEST_RESULT_LOG(
            "P00   INFO: [DRY-RUN] repo1: expire full backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 remove archive, start = 0000000100000000, stop = 0000000100000000\n"
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 remove archive, start = 000000020000000000000008, stop = 000000020000000000000008\n"
            "P00   INFO: [DRY-RUN] repo1: 10-2 no archive to remove");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire via backup command");

        // Copy the repo to another repo
        HRN_SYSTEM("mkdir " TEST_PATH "/repo2");
        HRN_SYSTEM("cp -r " TEST_PATH_REPO "/* " TEST_PATH "/repo2/");

        // Configure multi-repo and set the repo option to expire the second repo (non-default) files
        argList = strLstDup(argListBase);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionDiff, 1, "3");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionArchive, 1, "2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionArchiveType, 1, "diff");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 2, "2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionDiff, 2, "3");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionArchive, 2, "2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionArchiveType, 2, "diff");

        StringList *argList2 = strLstDup(argList);
        hrnCfgArgRawZ(argList2, cfgOptRepo, "2");
        hrnCfgArgRawZ(argList2, cfgOptPgPath, TEST_PATH_PG);
        HRN_CFG_LOAD(cfgCmdBackup, argList2);

        TEST_RESULT_VOID(cmdExpire(), "via backup command: expire last backup in archive sub path and remove sub path");

        TEST_RESULT_BOOL(
            storagePathExistsP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000")), false,
            "archive sub path removed repo2 (default)");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageRepoIdx(0), STRDEF(STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000")), true,
            "archive sub path repo1 not removed");

        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepoIdx(1), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL),
            "get backup.info repo2");
        TEST_RESULT_BOOL(
            strLstExists(infoBackupDataLabelList(infoBackup, NULL), STRDEF("20181119-152138F")), false,
            "backup removed from repo2");
        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepoIdx(0), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL),
            "get backup.info repo1");
        TEST_RESULT_BOOL(
            strLstExists(infoBackupDataLabelList(infoBackup, NULL), STRDEF("20181119-152138F")), true, "backup exists repo1");

        TEST_RESULT_LOG(
            "P00   INFO: repo2: expire full backup 20181119-152138F\n"
            "P00   INFO: repo2: remove expired backup 20181119-152138F\n"
            "P00   INFO: repo2: 9.4-1 remove archive, start = 0000000100000000, stop = 0000000100000000\n"
            "P00   INFO: repo2: 9.4-1 remove archive, start = 000000020000000000000008, stop = 000000020000000000000008\n"
            "P00   INFO: repo2: 10-2 no archive to remove");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - no dry run");

        // Add to previous list and specify repo
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire last backup in archive sub path and remove sub path");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000")), false,
            "archive sub path removed repo1");

        TEST_RESULT_LOG(
            "P00   INFO: repo1: expire full backup 20181119-152138F\n"
            "P00   INFO: repo1: remove expired backup 20181119-152138F\n"
            "P00   INFO: repo1: 9.4-1 remove archive, start = 0000000100000000, stop = 0000000100000000\n"
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000020000000000000008, stop = 000000020000000000000008\n"
            "P00   INFO: repo1: 10-2 no archive to remove");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - multi-repo errors, continue to next repo after error");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionArchive, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 2, "3");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionDiff, 2, "2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionArchive, 2, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionArchiveType, 2, "diff");

        harnessLogLevelSet(logLevelDetail);

        // Rename backup.info files on repo1 to cause error
        HRN_SYSTEM("mv " TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE " " TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE ".save");
        HRN_SYSTEM(
            "mv " TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE INFO_COPY_EXT " " TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE
            INFO_COPY_EXT ".save");

        // Rename archive.info file on repo2 to cause error
        HRN_SYSTEM(
            "mv " TEST_PATH "/repo2/archive/db/" INFO_ARCHIVE_FILE " " TEST_PATH "/repo2/archive/db/" INFO_ARCHIVE_FILE ".save");

        // Configure dry-run
        argList2 = strLstDup(argList);
        hrnCfgArgRawBool(argList2, cfgOptDryRun, true);
        HRN_CFG_LOAD(cfgCmdExpire, argList2);

        TEST_ERROR(
            cmdExpire(), CommandError, CFGCMD_EXPIRE " command encountered 2 error(s), check the log file for details");
        TEST_RESULT_LOG(
            "P00  ERROR: [055]: [DRY-RUN] repo1: unable to load info file '" TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE "' or '"
                         TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE INFO_COPY_EXT"':\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE
                         "' for read\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE
                         INFO_COPY_EXT "' for read\n"
            "            HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "            HINT: has a stanza-create been performed?\n"
            "P00   INFO: [DRY-RUN] repo2: expire diff backup set 20181119-152800F_20181119-152152D,"
                " 20181119-152800F_20181119-152155I\n"
            "P00   INFO: [DRY-RUN] repo2: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: [DRY-RUN] repo2: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00  ERROR: [055]: [DRY-RUN] repo2: unable to load info file '" TEST_PATH "/repo2/archive/db/" INFO_ARCHIVE_FILE
                         "' or '" TEST_PATH "/repo2/archive/db/" INFO_ARCHIVE_FILE INFO_COPY_EXT "':\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH "/repo2/archive/db/" INFO_ARCHIVE_FILE
                         "' for read\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH "/repo2/archive/db/" INFO_ARCHIVE_FILE
                         INFO_COPY_EXT "' for read\n"
            "            HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "            HINT: is archive_command configured correctly in postgresql.conf?\n"
            "            HINT: has a stanza-create been performed?\n"
            "            HINT: use --no-archive-check to disable archive checks during backup if you have an alternate"
                         " archiving scheme.");

        // Restore saved archive.info file
        HRN_SYSTEM(
            "mv " TEST_PATH_REPO "2/archive/db/" INFO_ARCHIVE_FILE ".save " TEST_PATH "/repo2/archive/db/" INFO_ARCHIVE_FILE);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - multi-repo, continue to next repo after error");

        TEST_ERROR(
            cmdExpire(), CommandError, CFGCMD_EXPIRE " command encountered 1 error(s), check the log file for details");
        TEST_RESULT_LOG(
            "P00  ERROR: [055]: [DRY-RUN] repo1: unable to load info file '" TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE "' or '"
                         TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE INFO_COPY_EXT"':\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE
                         "' for read\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE
                         INFO_COPY_EXT "' for read\n"
            "            HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "            HINT: has a stanza-create been performed?\n"
            "P00   INFO: [DRY-RUN] repo2: expire diff backup set 20181119-152800F_20181119-152152D,"
                " 20181119-152800F_20181119-152155I\n"
            "P00   INFO: [DRY-RUN] repo2: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: [DRY-RUN] repo2: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00 DETAIL: [DRY-RUN] repo2: 9.4-1 archive retention on backup 20181119-152800F, start = 000000020000000000000002,"
                " stop = 000000020000000000000002\n"
            "P00 DETAIL: [DRY-RUN] repo2: 9.4-1 archive retention on backup 20181119-152800F_20181119-152252D,"
                " start = 000000020000000000000009\n"
            "P00   INFO: [DRY-RUN] repo2: 9.4-1 remove archive, start = 000000020000000000000004,"
                " stop = 000000020000000000000007\n"
            "P00 DETAIL: [DRY-RUN] repo2: 10-2 archive retention on backup 20181119-152900F, start = 000000010000000000000003\n"
            "P00   INFO: [DRY-RUN] repo2: 10-2 no archive to remove");

        // Restore saved backup.info files
        HRN_SYSTEM("mv " TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE ".save " TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE);
        HRN_SYSTEM(
            "mv " TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE INFO_COPY_EXT ".save " TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE
            INFO_COPY_EXT);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - multi-repo, dry run: archive and backups not removed");

        TEST_RESULT_VOID(cmdExpire(), "expire (dry-run) - log expired backups and archive path to remove");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE, "10-2/\n9.4-1/\narchive.info\n", .noRecurse = true,
            .comment = "repo1: 9.4-1 archive path not removed");
        TEST_STORAGE_LIST(
            storageRepoIdx(1), STORAGE_REPO_ARCHIVE, "10-2/\n9.4-1/\narchive.info\n", .noRecurse = true,
            .comment = "repo2: 9.4-1 archive path not removed");
        TEST_STORAGE_LIST(
            storageRepoIdx(1), STORAGE_REPO_ARCHIVE "/9.4-1/",
            "0000000200000000/\n"
            "0000000200000000/000000020000000000000002-9baedd24b61aa15305732ac678c4e2c102435a09\n"
            "0000000200000000/000000020000000000000004-9baedd24b61aa15305732ac678c4e2c102435a09\n"
            "0000000200000000/000000020000000000000005-9baedd24b61aa15305732ac678c4e2c102435a09\n"
            "0000000200000000/000000020000000000000007-9baedd24b61aa15305732ac678c4e2c102435a09\n"
            "0000000200000000/000000020000000000000009-9baedd24b61aa15305732ac678c4e2c102435a09\n"
            "0000000200000000/000000020000000000000010-9baedd24b61aa15305732ac678c4e2c102435a09\n",
            .comment = "repo2: 9.4-1 nothing removed");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP,
            "20181119-152800F/\n"
            "20181119-152800F_20181119-152152D/\n"
            "20181119-152800F_20181119-152155I/\n"
            "20181119-152800F_20181119-152252D/\n"
            "20181119-152900F/\n"
            "20181119-152900F_20181119-152500I/\n"
            "backup.info\n"
            "backup.info.copy\n", .noRecurse = true, .comment = "repo1: backups not removed");
        TEST_STORAGE_LIST(
            storageRepoIdx(1), STORAGE_REPO_BACKUP,
            "20181119-152800F/\n"
            "20181119-152800F_20181119-152152D/\n"
            "20181119-152800F_20181119-152155I/\n"
            "20181119-152800F_20181119-152252D/\n"
            "20181119-152900F/\n"
            "20181119-152900F_20181119-152500I/\n"
            "backup.info\n"
            "backup.info.copy\n", .noRecurse = true, .comment = "repo2: backups not removed");

        TEST_RESULT_LOG(
            "P00   INFO: [DRY-RUN] repo1: expire full backup set 20181119-152800F, 20181119-152800F_20181119-152152D, "
                "20181119-152800F_20181119-152155I, 20181119-152800F_20181119-152252D\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup 20181119-152800F_20181119-152252D\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup 20181119-152800F\n"
            "P00   INFO: [DRY-RUN] repo1: remove archive path " TEST_PATH_REPO "/archive/db/9.4-1\n"
            "P00 DETAIL: [DRY-RUN] repo1: 10-2 archive retention on backup 20181119-152900F, start = 000000010000000000000003\n"
            "P00   INFO: [DRY-RUN] repo1: 10-2 no archive to remove\n"
            "P00   INFO: [DRY-RUN] repo2: expire diff backup set 20181119-152800F_20181119-152152D,"
                " 20181119-152800F_20181119-152155I\n"
            "P00   INFO: [DRY-RUN] repo2: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: [DRY-RUN] repo2: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00 DETAIL: [DRY-RUN] repo2: 9.4-1 archive retention on backup 20181119-152800F, start = 000000020000000000000002,"
                " stop = 000000020000000000000002\n"
            "P00 DETAIL: [DRY-RUN] repo2: 9.4-1 archive retention on backup 20181119-152800F_20181119-152252D,"
                " start = 000000020000000000000009\n"
            "P00   INFO: [DRY-RUN] repo2: 9.4-1 remove archive, start = 000000020000000000000004,"
                " stop = 000000020000000000000007\n"
            "P00 DETAIL: [DRY-RUN] repo2: 10-2 archive retention on backup 20181119-152900F, start = 000000010000000000000003\n"
            "P00   INFO: [DRY-RUN] repo2: 10-2 no archive to remove");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - multi-repo, archive and backups removed");

        // Rerun previous test without dry-run
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire backups and remove archive path");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE, "10-2/\narchive.info\n", .noRecurse = true,
            .comment = "repo1: retention-archive-type=full so 9.4-1 archive path removed");
        TEST_STORAGE_LIST(
            storageRepoIdx(1), STORAGE_REPO_ARCHIVE, "10-2/\n9.4-1/\narchive.info\n", .noRecurse = true,
            .comment = "repo2: retention-archive-type=diff so 9.4-1 archive path not removed");
        TEST_STORAGE_LIST(
            storageRepoIdx(1), STORAGE_REPO_ARCHIVE "/9.4-1/",
            "0000000200000000/\n"
            "0000000200000000/000000020000000000000002-9baedd24b61aa15305732ac678c4e2c102435a09\n"
            "0000000200000000/000000020000000000000009-9baedd24b61aa15305732ac678c4e2c102435a09\n"
            "0000000200000000/000000020000000000000010-9baedd24b61aa15305732ac678c4e2c102435a09\n",
            .comment = "repo2: 9.4-1 only archives not meeting retention for archive-retention-type=diff are removed");

        TEST_RESULT_LOG(
            "P00   INFO: repo1: expire full backup set 20181119-152800F, 20181119-152800F_20181119-152152D, "
                "20181119-152800F_20181119-152155I, 20181119-152800F_20181119-152252D\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F_20181119-152252D\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F\n"
            "P00   INFO: repo1: remove archive path " TEST_PATH_REPO "/archive/db/9.4-1\n"
            "P00 DETAIL: repo1: 10-2 archive retention on backup 20181119-152900F, start = 000000010000000000000003\n"
            "P00   INFO: repo1: 10-2 no archive to remove\n"
            "P00   INFO: repo2: expire diff backup set 20181119-152800F_20181119-152152D,"
                " 20181119-152800F_20181119-152155I\n"
            "P00   INFO: repo2: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: repo2: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00 DETAIL: repo2: 9.4-1 archive retention on backup 20181119-152800F, start = 000000020000000000000002,"
                " stop = 000000020000000000000002\n"
            "P00 DETAIL: repo2: 9.4-1 archive retention on backup 20181119-152800F_20181119-152252D,"
                " start = 000000020000000000000009\n"
            "P00   INFO: repo2: 9.4-1 remove archive, start = 000000020000000000000004, stop = 000000020000000000000007\n"
            "P00 DETAIL: repo2: 10-2 archive retention on backup 20181119-152900F, start = 000000010000000000000003\n"
            "P00   INFO: repo2: 10-2 no archive to remove");

        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL),
            "repo1: get backup.info");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 2, "repo1: backup.info updated on disk");
        TEST_RESULT_STRLST_Z(
            strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderAsc),
            "20181119-152900F\n20181119-152900F_20181119-152500I\n", "repo1: remaining current backups correct");

        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepoIdx(1), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL),
            "repo2: get backup.info");
        TEST_RESULT_UINT(infoBackupDataTotal(infoBackup), 4, "repo2: backup.info updated on disk");
        TEST_RESULT_STRLST_Z(
            strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderAsc),
            "20181119-152800F\n20181119-152800F_20181119-152252D\n20181119-152900F\n20181119-152900F_20181119-152500I\n",
            "repo2: remaining current backups correct");

        harnessLogLevelReset();

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - multi-repo, adhoc");

        // With multi-repo config from previous test, adhoc expire on backup that doesn't exist
        argList2 = strLstDup(argList);
        hrnCfgArgRawZ(argList2, cfgOptSet, "20201119-123456F_20201119-234567I");
        HRN_CFG_LOAD(cfgCmdExpire, argList2);

        TEST_RESULT_VOID(cmdExpire(), "label format OK but backup does not exist on any repo");
        TEST_RESULT_LOG(
            "P00   INFO: repo1: 10-2 no archive to remove\n"
            "P00   WARN: backup 20201119-123456F_20201119-234567I does not exist\n"
            "            HINT: run the info command and confirm the backup is listed\n"
            "P00   INFO: repo2: 9.4-1 no archive to remove\n"
            "P00   INFO: repo2: 10-2 no archive to remove");

        // Rerun on single repo
        hrnCfgArgRawZ(argList2, cfgOptRepo, "1");
        HRN_CFG_LOAD(cfgCmdExpire, argList2);

        TEST_RESULT_VOID(cmdExpire(), "label format OK but backup does not exist on requested repo");
        TEST_RESULT_LOG(
            "P00   WARN: backup 20201119-123456F_20201119-234567I does not exist\n"
            "            HINT: run the info command and confirm the backup is listed\n"
            "P00   INFO: repo1: 10-2 no archive to remove");

        // With multiple repos, adhoc expire backup only on one repo
        hrnCfgArgRawZ(argList, cfgOptSet, "20181119-152900F_20181119-152500I");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawBool(argList, cfgOptDryRun, true);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        // Incremental removed but no archives expired because the only remaining full backups must be able to play through PITR
        TEST_RESULT_VOID(cmdExpire(), "label format OK and expired on specified repo");
        TEST_RESULT_LOG(
            "P00   WARN: [DRY-RUN] repo1: expiring latest backup 20181119-152900F_20181119-152500I - the ability to perform"
                " point-in-time-recovery (PITR) may be affected\n"
            "            HINT: non-default settings for 'repo1-retention-archive'/'repo1-retention-archive-type' (even in prior"
                " expires) can cause gaps in the WAL.\n"
            "P00   INFO: [DRY-RUN] repo1: expire adhoc backup 20181119-152900F_20181119-152500I\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup 20181119-152900F_20181119-152500I\n"
            "P00   INFO: [DRY-RUN] repo1: 10-2 no archive to remove");

        // Incorrect backup label format provided
        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptSet, BOGUS_STR);

        HRN_CFG_LOAD(cfgCmdExpire, argList);
        TEST_ERROR(cmdExpire(), OptionInvalidValueError, "'" BOGUS_STR "' is not a valid backup label format");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire command - archive removed");

        archiveGenerate(storageTest, archiveStanzaPath, 1, 1, "9.4-1", "0000000100000000");
        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH_PG);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire remove archive path");
        TEST_RESULT_LOG(
            "P00   INFO: repo1: remove archive path " TEST_PATH_REPO "/archive/db/9.4-1\n"
            "P00   INFO: repo1: 10-2 no archive to remove");

        //--------------------------------------------------------------------------------------------------------------------------
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
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
                "\"db-version\":\"10\"}\n");

        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL), "get backup.info");

        archiveGenerate(storageTest, archiveStanzaPath, 1, 5, "9.4-1", "0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retention backup no archive-start");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "2");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchiveType, "full");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup, false, 0), "backup selected for retention does not have archive-start so do nothing");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000", strZ(archiveExpectList(1, 5, "0000000100000000")),
            .comment = "nothing removed from 9.4-1/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("prior backup has no archive-start");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchiveType, "full");
        HRN_CFG_LOAD(cfgCmdExpire, argList);
        harnessLogLevelSet(logLevelDetail);

        TEST_RESULT_VOID(
            removeExpiredArchive(infoBackup, false, 0), "backup earlier than selected for retention does not have archive-start");
        TEST_RESULT_LOG(
            "P00 DETAIL: repo1: 9.4-1 archive retention on backup 20181119-152138F, start = 000000010000000000000002,"
                " stop = 000000010000000000000002\n"
            "P00 DETAIL: repo1: 9.4-1 archive retention on backup 20181119-152900F, start = 000000010000000000000004\n"
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000010000000000000001, stop = 000000010000000000000001\n"
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000010000000000000003, stop = 000000010000000000000003");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire history files - dry run");

        // Create backup.info and archives spread over different timelines
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
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
                "\"db-version\":\"10\"}\n");

        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL), "get backup.info");

        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F/" BACKUP_MANIFEST_FILE);

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"10\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}");
// CSHANG REPLACE THE STORAGE PATH REMOVE with the macro
        storagePathRemoveP(storageTest, strNewFmt("%s/10-2/0000000100000000", strZ(archiveStanzaPath)), .recurse=true);
        archiveGenerate(storageTest, archiveStanzaPath, 2, 2, "9.4-1", "0000000100000000");
        archiveGenerate(storageTest, archiveStanzaPath, 6, 10, "10-2", "0000000300000000");

        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-2/00000002.history");
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-2/00000003.history");

        // Load Parameters
        argList = strLstDup(argListBase);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "2");
        hrnCfgArgRawBool(argList, cfgOptDryRun, true);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire (dry-run) do not remove 00000002.history file");

        TEST_RESULT_BOOL(
            storageExistsP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/10-2/00000002.history")), true, "history file not removed");
        TEST_RESULT_LOG(
            "P00 DETAIL: [DRY-RUN] repo1: 9.4-1 archive retention on backup 20181119-152138F, start = 000000010000000000000002\n"
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 no archive to remove\n"
            "P00 DETAIL: [DRY-RUN] repo1: 10-2 archive retention on backup 20181119-152900F, start = 000000030000000000000006\n"
            "P00   INFO: [DRY-RUN] repo1: 10-2 no archive to remove\n"
            "P00   INFO: [DRY-RUN] repo1: 10-2 remove history file 00000002.history");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire history files - no dry run");

        // Load Parameters
        argList = strLstDup(argListBase);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "2");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire remove 00000002.history file");

        TEST_RESULT_BOOL(
            storageExistsP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/10-2/00000002.history")), false, "history file removed");
        TEST_RESULT_BOOL(
            storageExistsP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/10-2/00000003.history")), true, "history file not removed");
        TEST_RESULT_LOG(
            "P00 DETAIL: repo1: 9.4-1 archive retention on backup 20181119-152138F, start = 000000010000000000000002\n"
            "P00   INFO: repo1: 9.4-1 no archive to remove\n"
            "P00 DETAIL: repo1: 10-2 archive retention on backup 20181119-152900F, start = 000000030000000000000006\n"
            "P00   INFO: repo1: 10-2 no archive to remove\n"
            "P00   INFO: repo1: 10-2 remove history file 00000002.history");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire history files via backup command");

        // Load Parameters
        argList = strLstDup(argListBase);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "2");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH_PG);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        // Restore the history file
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-2/00000002.history");

        TEST_RESULT_VOID(cmdExpire(), "expire history files via backup command");

        TEST_RESULT_BOOL(
            storageExistsP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/10-2/00000002.history")), false, "history file removed");
        TEST_RESULT_BOOL(
            storageExistsP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/10-2/00000003.history")), true, "history file not removed");
        TEST_RESULT_LOG(
            "P00 DETAIL: repo1: 9.4-1 archive retention on backup 20181119-152138F, start = 000000010000000000000002\n"
            "P00   INFO: repo1: 9.4-1 no archive to remove\n"
            "P00 DETAIL: repo1: 10-2 archive retention on backup 20181119-152900F, start = 000000030000000000000006\n"
            "P00   INFO: repo1: 10-2 no archive to remove\n"
            "P00   INFO: repo1: 10-2 remove history file 00000002.history");

        harnessLogLevelReset();

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire backup history manifests older than 20 days - dry run");

        // Get number of days since latest unexpired backup 20181119-152138F
        unsigned int historyRetentionDays = (unsigned int)((timeNow - 1542640898) / SEC_PER_DAY) + 20;

        // Add history manifests for unexpired backups
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2018/20181119-152138F.manifest.gz");
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2018/20181119-152900F.manifest.gz");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2018/20181119-152900F_20181119-152500I.manifest.gz");

        // Add 21 day-old full backup
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2018/20181029-152138F.manifest.gz");

        // Add 15 day-old incr backup
        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2018/20181029-152138F_20181104-152138I.manifest.gz");

        // Add 14 day-old full backup
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2018/20181105-152138F.manifest.gz");

        // Add one year old full backup
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2017/20171119-152138F.manifest.gz");

        // Load Parameters
        argList = strLstDup(argListBase);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "2");
        hrnCfgArgRawFmt(argList, cfgOptRepoRetentionHistory, "%u", historyRetentionDays);
        hrnCfgArgRawBool(argList, cfgOptDryRun, true);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP "/backup.history",
            "2017/\n"
            // Previous year history
            "2017/20171119-152138F.manifest.gz\n"
            "2018/\n"
            // 21 day-old history manifest
            "2018/20181029-152138F.manifest.gz\n"
            // 15 day-old history manifest
            "2018/20181029-152138F_20181104-152138I.manifest.gz\n"
            // 14 day-old history manifest
            "2018/20181105-152138F.manifest.gz\n"
            // Manifests for current backups
            "2018/20181119-152138F.manifest.gz\n"
            "2018/20181119-152900F.manifest.gz\n"
            "2018/20181119-152900F_20181119-152500I.manifest.gz\n");

        TEST_RESULT_LOG(
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 no archive to remove\n"
            "P00   INFO: [DRY-RUN] repo1: 10-2 no archive to remove\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup history path 2017\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup history manifest 20181029-152138F_20181104-152138I.manifest.gz\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup history manifest 20181029-152138F.manifest.gz");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire backup history manifests older than 20 days");

        // Load config with the backup command to be sure repo-retention-history is valid
        argList = strLstDup(argListBase);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "2");
        hrnCfgArgRawFmt(argList, cfgOptRepoRetentionHistory, "%u", historyRetentionDays);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2019/20191119-152138F.manifest.gz");

        TEST_RESULT_VOID(cmdExpire(), "expire");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP "/backup.history",
            "2018/\n"
            // 14 day-old history manifest
            "2018/20181105-152138F.manifest.gz\n"
            // Manifests for current backups
            "2018/20181119-152138F.manifest.gz\n"
            "2018/20181119-152900F.manifest.gz\n"
            "2018/20181119-152900F_20181119-152500I.manifest.gz\n"
            "2019/\n"
            "2019/20191119-152138F.manifest.gz\n");

        TEST_RESULT_LOG(
            "P00   INFO: repo1: 9.4-1 no archive to remove\n"
            "P00   INFO: repo1: 10-2 no archive to remove\n"
            "P00   INFO: repo1: remove expired backup history path 2017\n"
            "P00   INFO: repo1: remove expired backup history manifest 20181029-152138F_20181104-152138I.manifest.gz\n"
            "P00   INFO: repo1: remove expired backup history manifest 20181029-152138F.manifest.gz");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire backup history manifests older than 20 days using backup config");

        // Add 21 day-old full backup
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2018/20181029-152138F.manifest.gz");

        // Add one year old full backup
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2017/20171119-152138F.manifest.gz");

        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH_PG);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP "/backup.history",
            "2018/\n"
            // 14 day-old history manifest
            "2018/20181105-152138F.manifest.gz\n"
            // Manifests for current backups are kept
            "2018/20181119-152138F.manifest.gz\n"
            "2018/20181119-152900F.manifest.gz\n"
            "2018/20181119-152900F_20181119-152500I.manifest.gz\n"
            "2019/\n"
            "2019/20191119-152138F.manifest.gz\n");

        TEST_RESULT_LOG(
            "P00   INFO: repo1: 9.4-1 no archive to remove\n"
            "P00   INFO: repo1: 10-2 no archive to remove\n"
            "P00   INFO: repo1: remove expired backup history path 2017\n"
            "P00   INFO: repo1: remove expired backup history manifest 20181029-152138F.manifest.gz");
    }

    // *****************************************************************************************************************************
    if (testBegin("info files mismatch"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive.info has only current db with different db history id as backup.info");

        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "2");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
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
                "\"db-version\":\"10\"}\n");

        // Write backup.manifest so infoBackup reconstruct produces same results as backup.info on disk and removeExpiredBackup
        // will find backup directories to remove
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F/" BACKUP_MANIFEST_FILE);

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"10\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6626363367545678089,\"db-version\":\"10\"}");

        // Create 10-1 and 10-2 although 10-2 is not realistic since the archive.info knows nothing about it - it is just to
        // confirm that nothing from disk is removed and it will also be used for the next test.
        archiveGenerate(storageTest, archiveStanzaPath, 1, 7, "10-1", "0000000100000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 7, "10-2", "0000000100000000");

        TEST_ERROR(
            cmdExpire(), CommandError, CFGCMD_EXPIRE " command encountered 1 error(s), check the log file for details");

        TEST_RESULT_LOG(
            "P00   INFO: repo1: expire full backup 20181119-152138F\n"
            "P00   INFO: repo1: remove expired backup 20181119-152138F\n"
            "P00  ERROR: [029]: repo1: archive expiration cannot continue - archive and backup history lists do not match");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/10-1/0000000100000000", strZ(archiveExpectList(1, 7, "0000000100000000")),
            .comment = "none removed from 10-1/0000000100000000");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/10-2/0000000100000000", strZ(archiveExpectList(1, 7, "0000000100000000")),
            .comment = "none removed from 10-2/0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive.info old history db system id not the same as backup.info");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"10\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6626363367545671234,\"db-version\":\"10\"}\n"
            "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}");

        TEST_ERROR(
            cmdExpire(), CommandError, CFGCMD_EXPIRE " command encountered 1 error(s), check the log file for details");

        TEST_RESULT_LOG(
            "P00  ERROR: [029]: repo1: archive expiration cannot continue - archive and backup history lists do not match");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive.info old history db version not the same as backup.info");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"10\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}");

        TEST_ERROR(
            cmdExpire(), CommandError, CFGCMD_EXPIRE " command encountered 1 error(s), check the log file for details");

        TEST_RESULT_LOG(
            "P00  ERROR: [029]: repo1: archive expiration cannot continue - archive and backup history lists do not match");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive.info has only current db with same db history id as backup.info");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
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
                "\"db-version\":\"10\"}\n");

        // Write backup.manifest so infoBackup reconstruct produces same results as backup.info on disk and removeExpiredBackup
        // will find backup directories to remove
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F/" BACKUP_MANIFEST_FILE);

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"10\"\n"
            "\n"
            "[db:history]\n"
            "2={\"db-id\":6626363367545678089,\"db-version\":\"10\"}");

        argList = strLstDup(argListAvoidWarn);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        // Here, although backup 20181119-152138F of 10-1 will be expired, the WAL in 10-1 will not since the archive.info
        // does not know about that dir. Again, not really realistic since if it is on disk and reconstructed it would have. So
        // here we are testing that things on disk that we are not aware of are not touched.
        TEST_RESULT_VOID(cmdExpire(), "expire archive that archive.info is aware of");

        TEST_RESULT_LOG(
            "P00   INFO: repo1: expire full backup 20181119-152138F\n"
            "P00   INFO: repo1: expire full backup 20181119-152800F\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F\n"
            "P00   INFO: repo1: remove expired backup 20181119-152138F\n"
            "P00   INFO: repo1: 10-2 remove archive, start = 000000010000000000000001, stop = 000000010000000000000005");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/10-1/0000000100000000", strZ(archiveExpectList(1, 7, "0000000100000000")),
            .comment = "none removed from 10-1/0000000100000000");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/10-2/0000000100000000", strZ(archiveExpectList(6, 7, "0000000100000000")),
            .comment = "all prior to 000000010000000000000006 removed from 10-2/0000000100000000");
    }

    // *****************************************************************************************************************************
    if (testBegin("expireAdhocBackup()"))
    {
        // Test setup for this section
        StringList *argList = strLstDup(argListAvoidWarn);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        // Create backup.info
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
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
                    "\"db-version\":\"12\"}\n");

        // Add backup directories with manifest file including a resumable backup dependent on last backup
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F_20181119-152152D/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F_20181119-152155I/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152850F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F/" BACKUP_MANIFEST_FILE);

        // Resumable backup
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F_20181119-153000I/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
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
            "pg_data={\"path\":\"" TEST_PATH_PG "\",\"type\":\"path\"}\n"
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
            "user=\"postgres\"\n");

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL), "get backup.info");

        // Create "latest" symlink
        const String *latestLink = storagePathP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest"));
        THROW_ON_SYS_ERROR_FMT(
            symlink(strZ(infoBackupData(infoBackup, infoBackupDataTotal(infoBackup) - 1).backupLabel), strZ(latestLink)) == -1,
            FileOpenError, "unable to create symlink '%s' to '%s'", strZ(latestLink),
            strZ(infoBackupData(infoBackup, infoBackupDataTotal(infoBackup) - 1).backupLabel));

        // Create archive info
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"12\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6626363367545678089,\"db-version\":\"12\"}");

        // Create archive directories and generate archive
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "9.4-1", "0000000200000000");
        archiveGenerate(storageTest, archiveStanzaPath, 1, 10, "12-2", "0000000100000000");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire backup and dependent");

        hrnCfgArgRawZ(argList, cfgOptSet, "20181119-152800F_20181119-152152D");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        // Set the log level to detail so archive expiration messages are seen
        harnessLogLevelSet(logLevelDetail);

        TEST_RESULT_VOID(cmdExpire(), "adhoc expire only backup and dependent");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP,
            "20181119-152138F/\n"
            "20181119-152138F/" BACKUP_MANIFEST_FILE "\n"
            "20181119-152800F/\n"
            "20181119-152800F/" BACKUP_MANIFEST_FILE "\n"
            "20181119-152800F_20181119-152252D/\n"
            "20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE "\n"
            "20181119-152850F/\n"
            "20181119-152850F/" BACKUP_MANIFEST_FILE "\n"
            "20181119-152900F/\n"
            "20181119-152900F/" BACKUP_MANIFEST_FILE "\n"
            "20181119-152900F_20181119-153000I/\n"
            "20181119-152900F_20181119-153000I/" BACKUP_MANIFEST_FILE INFO_COPY_EXT "\n"
            "backup.info\n"
            "backup.info.copy\n"
            "latest>\n",
            .comment = "only adhoc and dependents removed - resumable and all other backups remain");
        TEST_RESULT_STR(storageInfoP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")).linkDestination,
            STRDEF("20181119-152900F"), "latest link not updated");
        TEST_RESULT_LOG(
            "P00   INFO: repo1: expire adhoc backup set 20181119-152800F_20181119-152152D, 20181119-152800F_20181119-152155I\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00 DETAIL: repo1: 9.4-1 archive retention on backup 20181119-152138F, start = 000000020000000000000001,"
                " stop = 000000020000000000000001\n"
            "P00 DETAIL: repo1: 9.4-1 archive retention on backup 20181119-152800F, start = 000000020000000000000002\n"
            "P00   INFO: repo1: 9.4-1 no archive to remove\n"
            "P00 DETAIL: repo1: 12-2 archive retention on backup 20181119-152850F, start = 000000010000000000000002,"
                " stop = 000000010000000000000004\n"
            "P00 DETAIL: repo1: 12-2 archive retention on backup 20181119-152900F, start = 000000010000000000000006\n"
            "P00   INFO: repo1: 12-2 remove archive, start = 000000010000000000000001, stop = 000000010000000000000001\n"
            "P00   INFO: repo1: 12-2 remove archive, start = 000000010000000000000005, stop = 000000010000000000000005");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire full and archive (no dependents)");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptSet, "20181119-152138F");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "adhoc expire full backup");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP,
            "20181119-152800F/\n"
            "20181119-152800F/" BACKUP_MANIFEST_FILE "\n"
            "20181119-152800F_20181119-152252D/\n"
            "20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE "\n"
            "20181119-152850F/\n"
            "20181119-152850F/" BACKUP_MANIFEST_FILE "\n"
            "20181119-152900F/\n"
            "20181119-152900F/" BACKUP_MANIFEST_FILE "\n"
            "20181119-152900F_20181119-153000I/\n"
            "20181119-152900F_20181119-153000I/" BACKUP_MANIFEST_FILE INFO_COPY_EXT "\n"
            "backup.info\n"
            "backup.info.copy\n"
            "latest>\n",
            .comment = "only adhoc full removed");
        TEST_RESULT_LOG(
            "P00   INFO: repo1: expire adhoc backup 20181119-152138F\n"
            "P00   INFO: repo1: remove expired backup 20181119-152138F\n"
            "P00 DETAIL: repo1: 9.4-1 archive retention on backup 20181119-152800F, start = 000000020000000000000002\n"
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000020000000000000001, stop = 000000020000000000000001\n"
            "P00 DETAIL: repo1: 12-2 archive retention on backup 20181119-152850F, start = 000000010000000000000002,"
                " stop = 000000010000000000000004\n"
            "P00 DETAIL: repo1: 12-2 archive retention on backup 20181119-152900F, start = 000000010000000000000006\n"
            "P00   INFO: repo1: 12-2 no archive to remove");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire latest and resumable");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptSet, "20181119-152900F");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "adhoc expire latest backup");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP,
            "20181119-152800F/\n"
            "20181119-152800F/" BACKUP_MANIFEST_FILE "\n"
            "20181119-152800F_20181119-152252D/\n"
            "20181119-152800F_20181119-152252D/" BACKUP_MANIFEST_FILE "\n"
            "20181119-152850F/\n"
            "20181119-152850F/" BACKUP_MANIFEST_FILE "\n"
            "backup.info\n"
            "backup.info.copy\n"
            "latest>\n",
            .comment = "latest backup and resumable removed");
        TEST_RESULT_LOG(
            "P00   WARN: repo1: expiring latest backup 20181119-152900F - the ability to perform point-in-time-recovery (PITR) may"
                " be affected\n"
            "            HINT: non-default settings for 'repo1-retention-archive'/'repo1-retention-archive-type' (even in prior"
                " expires) can cause gaps in the WAL.\n"
            "P00   INFO: repo1: expire adhoc backup 20181119-152900F\n"
            "P00   INFO: repo1: remove expired backup 20181119-152900F_20181119-153000I\n"
            "P00   INFO: repo1: remove expired backup 20181119-152900F\n"
            "P00 DETAIL: repo1: 9.4-1 archive retention on backup 20181119-152800F, start = 000000020000000000000002\n"
            "P00   INFO: repo1: 9.4-1 no archive to remove\n"
            "P00 DETAIL: repo1: 12-2 archive retention on backup 20181119-152850F, start = 000000010000000000000002\n"
            "P00   INFO: repo1: 12-2 no archive to remove");
        TEST_RESULT_STR(storageInfoP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")).linkDestination,
            STRDEF("20181119-152850F"), "latest link updated");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/12-2/0000000100000000", strZ(strNewFmt(
                "%s%s", strZ(archiveExpectList(2, 4, "0000000100000000")), strZ(archiveExpectList(6, 10, "0000000100000000")))),
            .comment = "no archives removed from latest except what was already removed");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on expire last full backup in current db-id");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptSet, "20181119-152850F");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_ERROR(
            cmdExpire(), CommandError, CFGCMD_EXPIRE " command encountered 1 error(s), check the log file for details");

        TEST_RESULT_LOG(
            "P00  ERROR: [075]: repo1: full backup 20181119-152850F cannot be expired until another full backup has been created on"
                " this repo");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("allow adhoc expire on last full backup in prior db-id");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptSet, "20181119-152800F");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "adhoc expire last prior db-id backup");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP,
            "20181119-152850F/\n"
            "20181119-152850F/" BACKUP_MANIFEST_FILE "\n"
            "backup.info\n"
            "backup.info.copy\n"
            "latest>\n",
            .comment = "only last prior backup removed");
        TEST_RESULT_LOG(
            "P00   INFO: repo1: expire adhoc backup set 20181119-152800F, 20181119-152800F_20181119-152252D\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F_20181119-152252D\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F\n"
            "P00   INFO: repo1: remove archive path " TEST_PATH "/repo/archive/db/9.4-1\n"
            "P00 DETAIL: repo1: 12-2 archive retention on backup 20181119-152850F, start = 000000010000000000000002\n"
            "P00   INFO: repo1: 12-2 no archive to remove");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on expire last full backup on disk");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptSet, "20181119-152850F");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_ERROR(
            cmdExpire(), CommandError, CFGCMD_EXPIRE " command encountered 1 error(s), check the log file for details");
        TEST_RESULT_LOG(
            "P00  ERROR: [075]: repo1: full backup 20181119-152850F cannot be expired until another full backup has been created on"
                " this repo");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("adhoc dry-run");

        // Create backup.info
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
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
                "\"db-version\":\"12\"}\n");

        // Load the backup info. Do not store a manifest file for the adhoc backup for code coverage
        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL), "get backup.info");

        HRN_STORAGE_PATH_CREATE(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152850F_20181119-152252D",
            .comment = "create empty backup directory for code coverage");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptSet, "20181119-152850F_20181119-152252D");
        hrnCfgArgRawBool(argList, cfgOptDryRun, true);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        const String *adhocBackupLabel = STRDEF("20181119-152850F_20181119-152252D");
        TEST_RESULT_UINT(expireAdhocBackup(infoBackup, adhocBackupLabel, 0), 1, "adhoc expire last dependent backup");
        TEST_RESULT_VOID(
            removeExpiredBackup(infoBackup, adhocBackupLabel, 0), "code coverage: removeExpireBackup with no manifests");
        TEST_RESULT_LOG(
            "P00   WARN: [DRY-RUN] repo1: expiring latest backup 20181119-152850F_20181119-152252D - the ability to perform"
                " point-in-time-recovery (PITR) may be affected\n"
            "            HINT: non-default settings for 'repo1-retention-archive'/'repo1-retention-archive-type' (even in prior"
                " expires) can cause gaps in the WAL.\n"
            "P00   INFO: [DRY-RUN] repo1: expire adhoc backup 20181119-152850F_20181119-152252D\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup 20181119-152850F_20181119-152252D");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("resumable possibly based on adhoc expire backup, multi-repo, encryption");

        argList = strLstDup(argListAvoidWarn);
        hrnCfgArgRawZ(argList, cfgOptSet, "20181119-152850F_20181119-152252D");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 2, "1");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        // Create backup.info
        #define TEST_BACKUP_CURRENT                                                                                                \
            "[backup:current]\n"                                                                                                   \
            "20181119-152850F={"                                                                                                   \
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","                                                              \
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000004\","          \
            "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"                                           \
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"                                                   \
            "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","                 \
            "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"             \
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"           \
            "20181119-152850F_20181119-152252D={"                                                                                  \
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000006\","        \
            "\"backup-archive-stop\":\"000000010000000000000007\",\"backup-info-repo-size\":2369186,"                              \
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"                   \
            "\"backup-prior\":\"20181119-152850F\",\"backup-reference\":[\"20181119-152850F\"],"                                   \
            "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","                 \
            "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"             \
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":false}\n"

        #define TEST_BACKUP_DB                                                                                                     \
            "\n"                                                                                                                   \
            "[db]\n"                                                                                                               \
            "db-catalog-version=201909212\n"                                                                                       \
            "db-control-version=1201\n"                                                                                            \
            "db-id=2\n"                                                                                                            \
            "db-system-id=6626363367545678089\n"                                                                                   \
            "db-version=\"12\"\n"                                                                                                  \
            "\n"                                                                                                                   \
            "[db:history]\n"                                                                                                       \
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"                 \
                "\"db-version\":\"9.4\"}\n"                                                                                        \
            "2={\"db-catalog-version\":201909212,\"db-control-version\":1201,\"db-system-id\":6626363367545678089,"                \
                "\"db-version\":\"12\"}\n"

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
            TEST_BACKUP_CURRENT
            TEST_BACKUP_DB);
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE INFO_COPY_EXT,
            TEST_BACKUP_CURRENT
            TEST_BACKUP_DB);

        // Adhoc backup and resumable backup manifests
        const String *manifestContent = STRDEF(
                "[backup]\n"
                "backup-archive-start=\"000000010000000000000009\"\n"
                "backup-label=null\n"
                "backup-prior=\"20181119-152850F\"\n"
                "backup-timestamp-copy-start=0\n"
                "backup-timestamp-start=0\n"
                "backup-timestamp-stop=0\n"
                "backup-type=\"diff\"\n"
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
                "pg_data={\"path\":\"" TEST_PATH_PG "\",\"type\":\"path\"}\n"
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
                "user=\"postgres\"\n");

        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152850F_20181119-152252D/" BACKUP_MANIFEST_FILE,
            strZ(manifestContent));
        HRN_INFO_PUT(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152850F_20181119-152252D/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
            strZ(manifestContent));

        // archives to repo1
        archiveGenerate(storageTest, archiveStanzaPath, 2, 10, "12-2", "0000000100000000");

        // Create encrypted repo2 with same data from repo1 and ensure results are reported the same. This will test that the
        // manifest can be read on encrypted repos.
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[cipher]\n"
            "cipher-pass=\"" TEST_CIPHER_PASS_ARCHIVE "\"\n"
            "\n"
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"12\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6626363367545678089,\"db-version\":\"12\"}",
            .cipherType = cipherTypeAes256Cbc);

        const String *backupInfoContent = STRDEF(
            TEST_BACKUP_CURRENT
            "\n"
            "[cipher]\n"
            "cipher-pass=\"somepass\"\n"
            TEST_BACKUP_DB);

        HRN_INFO_PUT(storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE, strZ(backupInfoContent), .cipherType = cipherTypeAes256Cbc);
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE INFO_COPY_EXT, strZ(backupInfoContent),
            .cipherType = cipherTypeAes256Cbc);
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), STORAGE_REPO_BACKUP "/20181119-152850F/" BACKUP_MANIFEST_FILE,
            "[backup]\nbackup-type=\"full\"\n", .cipherType = cipherTypeAes256Cbc, .cipherPass = "somepass");
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), STORAGE_REPO_BACKUP "/20181119-152850F_20181119-152252D/" BACKUP_MANIFEST_FILE,
            "[backup]\nbackup-type=\"diff\"\n", .cipherType = cipherTypeAes256Cbc, .cipherPass = "somepass");
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), STORAGE_REPO_BACKUP "/20181119-152850F_20181200-152252D/" BACKUP_MANIFEST_FILE INFO_COPY_EXT,
            strZ(manifestContent), .cipherType = cipherTypeAes256Cbc, .cipherPass = "somepass");

        // archives to repo2
        archiveGenerate(storageTest, STRDEF(TEST_PATH "/repo2/archive/db"), 2, 10, "12-2", "0000000100000000");

        // Create "latest" symlink, repo2
        latestLink = storagePathP(storageRepoIdx(1), STRDEF(STORAGE_REPO_BACKUP "/latest"));
        THROW_ON_SYS_ERROR_FMT(
            symlink("20181119-152850F_20181200-152252D", strZ(latestLink)) == -1,
            FileOpenError, "unable to create symlink '%s' to '%s'", strZ(latestLink), "20181119-152850F_20181200-152252D");

        TEST_RESULT_VOID(cmdExpire(), "adhoc expire latest with resumable possibly based on it");

        TEST_RESULT_LOG(
            "P00   WARN: repo1: expiring latest backup 20181119-152850F_20181119-152252D - the ability to perform"
                " point-in-time-recovery (PITR) may be affected\n"
            "            HINT: non-default settings for 'repo1-retention-archive'/'repo1-retention-archive-type' (even in prior"
                " expires) can cause gaps in the WAL.\n"
            "P00   INFO: repo1: expire adhoc backup 20181119-152850F_20181119-152252D\n"
            "P00   INFO: repo1: remove expired backup 20181119-152850F_20181119-152252D\n"
            "P00 DETAIL: repo1: 12-2 archive retention on backup 20181119-152850F, start = 000000010000000000000002\n"
            "P00   INFO: repo1: 12-2 no archive to remove\n"
            "P00   WARN: repo2: expiring latest backup 20181119-152850F_20181119-152252D - the ability to perform"
                " point-in-time-recovery (PITR) may be affected\n"
            "            HINT: non-default settings for 'repo2-retention-archive'/'repo2-retention-archive-type' (even in prior"
                " expires) can cause gaps in the WAL.\n"
            "P00   INFO: repo2: expire adhoc backup 20181119-152850F_20181119-152252D\n"
            "P00   INFO: repo2: remove expired backup 20181119-152850F_20181119-152252D\n"
            "P00 DETAIL: repo2: 12-2 archive retention on backup 20181119-152850F, start = 000000010000000000000002\n"
            "P00   INFO: repo2: 12-2 no archive to remove");

        TEST_RESULT_STR(storageInfoP(storageRepoIdx(1), STRDEF(STORAGE_REPO_BACKUP "/latest")).linkDestination,
            STRDEF("20181119-152850F"), "latest link updated, repo2");

        // Cleanup
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);
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

        TEST_RESULT_UINT(expireTimeBasedBackup(infoBackup, (time_t)(timeNow - (40 * SEC_PER_DAY)), 0), 0, "no backups to expire");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("oldest backup not expired");

        // Set up
        StringList *argListTime = strLstDup(argListBase);
        hrnCfgArgRawZ(argListTime, cfgOptRepoRetentionFullType, "time");
        HRN_CFG_LOAD(cfgCmdExpire, argListTime);

        // Create backup.info and archive.info
        HRN_INFO_PUT(storageRepoWrite(), INFO_BACKUP_PATH_FILE, strZ(backupInfoContent));
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6625592122879095702\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}");

        // Write backup.manifest so infoBackup reconstruct produces same results as backup.info on disk
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F_20181119-152152D/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152800F_20181119-152155I/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F/" BACKUP_MANIFEST_FILE);
        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152900F_20181119-152600D/" BACKUP_MANIFEST_FILE);

        // Genreate archive for backups in backup.info
        archiveGenerate(storageTest, archiveStanzaPath, 1, 11, "9.4-1", "0000000100000000");

        // Set the log level to detail so archive expiration messages are seen
        harnessLogLevelSet(logLevelDetail);

        TEST_ASSIGN(infoBackup, infoBackupNewLoad(ioBufferReadNew(backupInfoBase)), "get backup.info");
        TEST_RESULT_VOID(cmdExpire(), "repo-retention-full not set for time-based");
        TEST_RESULT_LOG(
            "P00   WARN: option 'repo1-retention-full' is not set for 'repo1-retention-full-type=time', the repository may run out"
                " of space\n"
            "            HINT: to retain full backups indefinitely (without warning), set option 'repo1-retention-full' to the"
                " maximum.\n"
            "P00   INFO: repo1: time-based archive retention not met - archive logs will not be expired");

        // Stop time equals retention time
        TEST_RESULT_UINT(
            expireTimeBasedBackup(infoBackup, (time_t)(timeNow - (40 * SEC_PER_DAY)), 0), 0,
            "oldest backup stop time equals retention time");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL),
            "20181119-152138F\n20181119-152800F\n20181119-152800F_20181119-152152D\n20181119-152800F_20181119-152155I\n"
            "20181119-152900F\n20181119-152900F_20181119-152600D\n", "no backups expired");

        // // Add a time period
        StringList *argList = strLstDup(argListTime);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "35");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "oldest backup older but other backups too young");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000", strZ(archiveExpectList(1, 11, "0000000100000000")),
            .comment = "no archives expired");
        TEST_RESULT_STRLST_Z(
            infoBackupDataLabelList(infoBackup, NULL),
            "20181119-152138F\n20181119-152800F\n20181119-152800F_20181119-152152D\n20181119-152800F_20181119-152155I\n"
            "20181119-152900F\n20181119-152900F_20181119-152600D\n", "no backups expired");
        TEST_RESULT_LOG("P00   INFO: repo1: time-based archive retention not met - archive logs will not be expired");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("oldest backup expired");

        argList = strLstDup(argListTime);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "30");
        hrnCfgArgRawBool(argList, cfgOptDryRun, true);
        HRN_CFG_LOAD(cfgCmdExpire, argList);
        TEST_RESULT_VOID(cmdExpire(), "only oldest backup expired - dry-run");
        TEST_RESULT_LOG(
            "P00   INFO: [DRY-RUN] repo1: expire time-based backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup 20181119-152138F\n"
            "P00 DETAIL: [DRY-RUN] repo1: 9.4-1 archive retention on backup 20181119-152800F, start = 000000010000000000000004\n"
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 remove archive, start = 000000010000000000000001, stop = 000000010000000000000003");

        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "9999999");
        HRN_CFG_LOAD(cfgCmdExpire, argList);
        TEST_RESULT_VOID(cmdExpire(), "only oldest backup expired - dry-run, retention-archive set to max, no archives expired");
        TEST_RESULT_LOG(
            "P00   INFO: [DRY-RUN] repo1: expire time-based backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup 20181119-152138F");

        argList = strLstDup(argListTime);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "30");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "1"); // 1-day: expire non-essential archive prior to newest full backup
        hrnCfgArgRawBool(argList, cfgOptDryRun, true);
        HRN_CFG_LOAD(cfgCmdExpire, argList);
        TEST_RESULT_VOID(cmdExpire(), "only oldest backup expired but retention archive set lower - dry-run");
        TEST_RESULT_LOG(
            "P00   INFO: [DRY-RUN] repo1: expire time-based backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup 20181119-152138F\n"
            "P00 DETAIL: [DRY-RUN] repo1: 9.4-1 archive retention on backup 20181119-152800F, start = 000000010000000000000004,"
                " stop = 000000010000000000000004\n"
            "P00 DETAIL: [DRY-RUN] repo1: 9.4-1 archive retention on backup 20181119-152800F_20181119-152152D,"
                " start = 000000010000000000000006, stop = 000000010000000000000006\n"
            "P00 DETAIL: [DRY-RUN] repo1: 9.4-1 archive retention on backup 20181119-152800F_20181119-152155I,"
                " start = 000000010000000000000007, stop = 000000010000000000000007\n"
            "P00 DETAIL: [DRY-RUN] repo1: 9.4-1 archive retention on backup 20181119-152900F, start = 000000010000000000000009\n"
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 remove archive, start = 000000010000000000000001, stop = 000000010000000000000003\n"
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 remove archive, start = 000000010000000000000005, stop = 000000010000000000000005\n"
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 remove archive, start = 000000010000000000000008, stop = 000000010000000000000008");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repo1-retention-archive-type=diff");

        argList = strLstDup(argListTime);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "30");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchiveType, "diff");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionArchive, "1"); // 1-day: expire non-essential archive prior to newest diff backup
        hrnCfgArgRawBool(argList, cfgOptDryRun, true);
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "only oldest backup expired, retention archive is DIFF - dry-run");
        TEST_RESULT_LOG(
            "P00   WARN: [DRY-RUN] option 'repo1-retention-diff' is not set for 'repo1-retention-archive-type=diff'\n"
            "            HINT: to retain differential backups indefinitely (without warning), set option 'repo1-retention-diff'"
                " to the maximum.\n"
            "P00   INFO: [DRY-RUN] repo1: expire time-based backup 20181119-152138F\n"
            "P00   INFO: [DRY-RUN] repo1: remove expired backup 20181119-152138F\n"
            "P00 DETAIL: [DRY-RUN] repo1: 9.4-1 archive retention on backup 20181119-152800F, start = 000000010000000000000004,"
                " stop = 000000010000000000000004\n"
            "P00 DETAIL: [DRY-RUN] repo1: 9.4-1 archive retention on backup 20181119-152800F_20181119-152152D,"
                " start = 000000010000000000000006, stop = 000000010000000000000006\n"
            "P00 DETAIL: [DRY-RUN] repo1: 9.4-1 archive retention on backup 20181119-152800F_20181119-152155I,"
                " start = 000000010000000000000007, stop = 000000010000000000000007\n"
            "P00 DETAIL: [DRY-RUN] repo1: 9.4-1 archive retention on backup 20181119-152900F, start = 000000010000000000000009,"
                " stop = 000000010000000000000009\n"
            "P00 DETAIL: [DRY-RUN] repo1: 9.4-1 archive retention on backup 20181119-152900F_20181119-152600D,"
                " start = 000000010000000000000011\n"
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 remove archive, start = 000000010000000000000001, stop = 000000010000000000000003\n"
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 remove archive, start = 000000010000000000000005, stop = 000000010000000000000005\n"
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 remove archive, start = 000000010000000000000008, stop = 000000010000000000000008\n"
            "P00   INFO: [DRY-RUN] repo1: 9.4-1 remove archive, start = 000000010000000000000010, stop = 000000010000000000000010");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("expire oldest full");

        argList = strLstDup(argListTime);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "25");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        // Expire oldest from backup.info only, leaving the backup and archives on disk then save backup.info without oldest backup
        TEST_RESULT_UINT(expireTimeBasedBackup(infoBackup, (time_t)(timeNow - (25 * SEC_PER_DAY)), 0), 1, "expire oldest backup");
        TEST_RESULT_VOID(
            infoBackupSaveFile(infoBackup, storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL),
            "save backup.info without oldest");
        TEST_RESULT_LOG("P00   INFO: repo1: expire time-based backup 20181119-152138F");
        TEST_RESULT_VOID(cmdExpire(), "only oldest backup expired");
        TEST_RESULT_LOG(
            "P00   INFO: repo1: remove expired backup 20181119-152138F\n"
            "P00 DETAIL: repo1: 9.4-1 archive retention on backup 20181119-152800F, start = 000000010000000000000004\n"
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000010000000000000001, stop = 000000010000000000000003");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("newest backup - retention met but must keep one");

        argList = strLstDup(argListTime);
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        HRN_CFG_LOAD(cfgCmdExpire, argList);

        TEST_RESULT_VOID(cmdExpire(), "expire all but newest");
        TEST_RESULT_LOG(
            "P00   INFO: repo1: expire time-based backup set 20181119-152800F, 20181119-152800F_20181119-152152D,"
                " 20181119-152800F_20181119-152155I\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F_20181119-152155I\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F_20181119-152152D\n"
            "P00   INFO: repo1: remove expired backup 20181119-152800F\n"
            "P00 DETAIL: repo1: 9.4-1 archive retention on backup 20181119-152900F, start = 000000010000000000000009\n"
            "P00   INFO: repo1: 9.4-1 remove archive, start = 000000010000000000000004, stop = 000000010000000000000008");

        harnessLogLevelReset();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
