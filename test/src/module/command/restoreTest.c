/***********************************************************************************************************************************
Test Restore Command
***********************************************************************************************************************************/
#include "common/compress/helper.h"
#include "common/crypto/cipherBlock.h"
#include "postgres/version.h"
#include "storage/posix/storage.h"
#include "storage/helper.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPostgres.h"
#include "common/harnessProtocol.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Special string constants
***********************************************************************************************************************************/
#define UTF8_DB_NAME                                                "这个用汉语怎么说"

/***********************************************************************************************************************************
Test data for backup.info
***********************************************************************************************************************************/
#define TEST_RESTORE_BACKUP_INFO_DB                                                                                                \
    "[db]\n"                                                                                                                       \
    "db-catalog-version=201409291\n"                                                                                               \
    "db-control-version=942\n"                                                                                                     \
    "db-id=1\n"                                                                                                                    \
    "db-system-id=6569239123849665679\n"                                                                                           \
    "db-version=\"9.4\"\n"                                                                                                         \
    "\n"                                                                                                                           \
    "[db:history]\n"                                                                                                               \
    "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"                         \
        "\"db-version\":\"9.4\"}\n"

#define TEST_RESTORE_BACKUP_INFO                                                                                                   \
    "[backup:current]\n"                                                                                                           \
    "20161219-212741F={\"backrest-format\":5,\"backrest-version\":\"2.04\","                                                       \
    "\"backup-archive-start\":\"00000007000000000000001C\",\"backup-archive-stop\":\"00000007000000000000001C\","                  \
    "\"backup-info-repo-size\":3159776,\"backup-info-repo-size-delta\":3159776,\"backup-info-size\":26897030,"                     \
    "\"backup-info-size-delta\":26897030,\"backup-lsn-stop\":\"0/1C000101\",\"backup-timestamp-start\":1482182846,"                \
    "\"backup-timestamp-stop\":1482182861,\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,"                     \
    "\"option-archive-copy\":false,\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,"       \
    "\"option-hardlink\":false,\"option-online\":true}\n"                                                                          \
    "20161219-212741F_20161219-212803D={\"backrest-format\":5,\"backrest-version\":\"2.04\","                                      \
    "\"backup-archive-start\":\"00000008000000000000001E\",\"backup-archive-stop\":\"00000008000000000000001E\","                  \
    "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"                       \
    "\"backup-info-size-delta\":163866,\"backup-lsn-stop\":\"0/1E000101\",\"backup-prior\":\"20161219-212741F\","                  \
    "\"backup-reference\":[\"20161219-212741F\"],\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,"      \
    "\"backup-type\":\"diff\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"                            \
    "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,"           \
    "\"option-online\":true}\n"                                                                                                    \
    "20161219-212741F_20161219-212918I={\"backrest-format\":5,\"backrest-version\":\"2.04\","                                      \
    "\"backup-archive-start\":null,\"backup-archive-stop\":null,"                                                                  \
    "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"                       \
    "\"backup-info-size-delta\":163866,\"backup-lsn-stop\":\"0/1E000105\",\"backup-prior\":\"20161219-212741F\","                  \
    "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803D\"],\"backup-timestamp-start\":1482182884,"     \
    "\"backup-timestamp-stop\":1482182985,\"backup-type\":\"incr\",\"db-id\":1,\"option-archive-check\":true,"                     \
    "\"option-archive-copy\":false,\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,"       \
    "\"option-hardlink\":false,\"option-online\":true}\n"

// To verify handling of missing backup-lsn-stop for --type=lsn --target=<lsn> backup
#define TEST_RESTORE_BACKUP_INFO1                                                                                                  \
    "[backup:current]\n"                                                                                                           \
    "20161219-212741F={\"backrest-format\":5,\"backrest-version\":\"2.04\","                                                       \
    "\"backup-archive-start\":\"00000007000000000000001C\",\"backup-archive-stop\":\"00000007000000000000001C\","                  \
    "\"backup-info-repo-size\":3159776,\"backup-info-repo-size-delta\":3159776,\"backup-info-size\":26897030,"                     \
    "\"backup-info-size-delta\":26897030,\"backup-timestamp-start\":1482182846,\"backup-timestamp-stop\":1482182861,"              \
    "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"                            \
    "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,"           \
    "\"option-online\":true}\n"                                                                                                    \
    "20161219-212741F_20161219-212803D={\"backrest-format\":5,\"backrest-version\":\"2.04\","                                      \
    "\"backup-archive-start\":\"00000008000000000000001E\",\"backup-archive-stop\":\"00000008000000000000001E\","                  \
    "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"                       \
    "\"backup-info-size-delta\":163866,\"backup-lsn-stop\":\"0/1E000101\",\"backup-prior\":\"20161219-212741F\","                  \
    "\"backup-reference\":[\"20161219-212741F\"],\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,"      \
    "\"backup-type\":\"diff\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"                            \
    "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,"           \
    "\"option-online\":true}\n"                                                                                                    \
    "20161219-212741F_20161219-212918I={\"backrest-format\":5,\"backrest-version\":\"2.04\","                                      \
    "\"backup-archive-start\":null,\"backup-archive-stop\":null,"                                                                  \
    "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"                       \
    "\"backup-info-size-delta\":163866,\"backup-lsn-stop\":\"0/1E000105\",\"backup-prior\":\"20161219-212741F\","                  \
    "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803D\"],\"backup-timestamp-start\":1482182884,"     \
    "\"backup-timestamp-stop\":1482182985,\"backup-type\":\"incr\",\"db-id\":1,\"option-archive-check\":true,"                     \
    "\"option-archive-copy\":false,\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,"       \
    "\"option-hardlink\":false,\"option-online\":true}\n"

/***********************************************************************************************************************************
Build a simple manifest for testing
***********************************************************************************************************************************/
static Manifest *
testManifestMinimal(const String *label, unsigned int pgVersion, const String *pgPath)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, label);
        FUNCTION_HARNESS_PARAM(UINT, pgVersion);
        FUNCTION_HARNESS_PARAM(STRING, pgPath);
    FUNCTION_HARNESS_END();

    ASSERT(label != NULL);
    ASSERT(pgVersion != 0);
    ASSERT(pgPath != NULL);

    Manifest *result = NULL;

    OBJ_NEW_BEGIN(Manifest, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        result = manifestNewInternal();
        result->pub.info = infoNew(NULL);

        result->pub.data.backupLabel = strDup(label);
        result->pub.data.pgVersion = pgVersion;

        if (strEndsWithZ(label, "I"))
            result->pub.data.backupType = backupTypeIncr;
        else if (strEndsWithZ(label, "D"))
            result->pub.data.backupType = backupTypeDiff;
        else
            result->pub.data.backupType = backupTypeFull;

        ManifestTarget targetBase = {.name = MANIFEST_TARGET_PGDATA_STR, .path = pgPath};
        manifestTargetAdd(result, &targetBase);
        ManifestPath pathBase = {.name = MANIFEST_TARGET_PGDATA_STR, .mode = 0700, .group = groupName(), .user = userName()};
        manifestPathAdd(result, &pathBase);
        ManifestFile fileVersion = {
            .name = STRDEF("pg_data/" PG_FILE_PGVERSION), .mode = 0600, .group = groupName(), .user = userName()};
        manifestFileAdd(result, &fileVersion);
    }
    OBJ_NEW_END();

    FUNCTION_HARNESS_RETURN(MANIFEST, result);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Install local command handler shim
    static const ProtocolServerHandler testLocalHandlerList[] = {PROTOCOL_SERVER_HANDLER_RESTORE_LIST};
    hrnProtocolLocalShimInstall(testLocalHandlerList, LENGTH_OF(testLocalHandlerList));

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("restoreFile()"))
    {
        const String *repoFileReferenceFull = STRDEF("20190509F");
        const String *repoFile1 = STRDEF("pg_data/testfile");
        unsigned int repoIdx = 0;

        // Load Parameters
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        // Create the pg path
        HRN_STORAGE_PATH_CREATE(storagePgWrite(), NULL, .mode = 0700);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("compressed encrypted repo file - fail");

        HRN_STORAGE_PUT_Z(
            storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strZ(repoFileReferenceFull), strZ(repoFile1)),
            "acefile", .compressType = compressTypeGz, .cipherType = cipherTypeAes256Cbc, .cipherPass = "badpass",
            .comment = "create a compressed encrypted repo file");

        List *fileList = lstNewP(sizeof(RestoreFile));

        RestoreFile file =
        {
            .name = STRDEF("normal"),
            .checksum = STRDEF("ffffffffffffffffffffffffffffffffffffffff"),
            .size = 7,
            .timeModified = 1557432154,
            .mode = 0600,
            .zero = false,
            .user = NULL,
            .group = NULL,
        };

        lstAdd(fileList, &file);

        TEST_ERROR(
            restoreFile(
                strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strZ(repoFileReferenceFull), strZ(repoFile1)), repoIdx, compressTypeGz,
                0, false, false, STRDEF("badpass"), fileList),
            ChecksumError,
            "error restoring 'normal': actual checksum 'd1cd8a7d11daa26814b93eb604e1d49ab4b43770' does not match expected checksum"
                " 'ffffffffffffffffffffffffffffffffffffffff'");
    }

    // *****************************************************************************************************************************
    if (testBegin("restorePathValidate()"))
    {
        const String *pgPath = STRDEF(TEST_PATH "/pg");
        const String *repoPath = STRDEF(TEST_PATH "/repo");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when pg appears to be running");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), PG_FILE_POSTMTRPID);

        TEST_ERROR(
            restorePathValidate(), PgRunningError,
            "unable to restore while PostgreSQL is running\n"
            "HINT: presence of '" PG_FILE_POSTMTRPID "' in '" TEST_PATH "/pg' indicates PostgreSQL is running.\n"
            "HINT: remove '" PG_FILE_POSTMTRPID "' only if PostgreSQL is not running.");

        HRN_STORAGE_REMOVE(storagePgWrite(), PG_FILE_POSTMTRPID, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on data directory does not look valid - delta");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawBool(argList, cfgOptDelta, true);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restorePathValidate(), "restore --delta with invalid PGDATA");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptDelta), false, "--delta set to false");
        TEST_RESULT_LOG(
            "P00   WARN: --delta or --force specified but unable to find 'PG_VERSION' or 'backup.manifest' in '" TEST_PATH "/pg' to"
                " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                " exist in the destination directories the restore will be aborted.");

        HRN_CFG_LOAD(cfgCmdRestore, argList);
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "backup.manifest");
        TEST_RESULT_VOID(restorePathValidate(), "restore --delta with valid PGDATA");
        HRN_STORAGE_REMOVE(storagePgWrite(), "backup.manifest", .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on data directory does not look valid - force");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawBool(argList, cfgOptForce, true);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restorePathValidate(), "restore --force with invalid PGDATA");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptForce), false, "--force set to false");
        TEST_RESULT_LOG(
            "P00   WARN: --delta or --force specified but unable to find 'PG_VERSION' or 'backup.manifest' in '" TEST_PATH "/pg' to"
                " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                " exist in the destination directories the restore will be aborted.");

        HRN_CFG_LOAD(cfgCmdRestore, argList);
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), PG_FILE_PGVERSION);
        TEST_RESULT_VOID(restorePathValidate(), "restore --force with valid PGDATA");
        HRN_STORAGE_REMOVE(storagePgWrite(), PG_FILE_PGVERSION, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("getEpoch()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("system time UTC");

        setenv("TZ", "UTC", true);
        TEST_RESULT_INT(getEpoch(STRDEF("2020-01-08 09:18:15-0700")), 1578500295, "epoch with timezone");
        TEST_RESULT_INT(getEpoch(STRDEF("2020-01-08 16:18:15.0000")), 1578500295, "same epoch no timezone");
        TEST_RESULT_INT(getEpoch(STRDEF("2020-01-08 16:18:15.0000+00")), 1578500295, "same epoch timezone 0");
        TEST_ERROR(getEpoch(STRDEF("2020-13-08 16:18:15")), FormatError, "invalid date 2020-13-08");
        TEST_ERROR(getEpoch(STRDEF("2020-01-08 16:68:15")), FormatError, "invalid time 16:68:15");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("system time America/New_York");

        setenv("TZ", "America/New_York", true);
        time_t testTime = 1573754569;
        char timeBuffer[20];
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localtime(&testTime));
        TEST_RESULT_Z(timeBuffer, "2019-11-14 13:02:49", "check timezone set");
        TEST_RESULT_INT(getEpoch(STRDEF("2019-11-14 13:02:49-0500")), 1573754569, "offset same as local");
        TEST_RESULT_INT(getEpoch(STRDEF("2019-11-14 13:02:49")), 1573754569, "GMT-0500 (EST)");
        TEST_RESULT_INT(getEpoch(STRDEF("2019-09-14 20:02:49")), 1568505769, "GMT-0400 (EDT)");
        TEST_RESULT_INT(getEpoch(STRDEF("2018-04-27 04:29:00+04:30")), 1524787140, "GMT+0430");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid target time format");

        TEST_ERROR(
            getEpoch(STRDEF("Tue, 15 Nov 1994 12:45:26")), FormatError,
            "automatic backup set selection cannot be performed with provided time 'Tue, 15 Nov 1994 12:45:26'\n"
            "HINT: time format must be YYYY-MM-DD HH:MM:SS with optional msec and optional timezone (+/- HH or HHMM or HH:MM) - if"
                " timezone is omitted, local time is assumed (for UTC use +00)");

        setenv("TZ", "UTC", true);
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreBackupSet()"))
    {
        const String *pgPath = STRDEF(TEST_PATH "/pg");
        const String *repoPath = STRDEF(TEST_PATH "/repo");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when no backups are present");

        HRN_INFO_PUT(storageRepoWrite(), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO_DB);
        TEST_ERROR(restoreBackupSet(), BackupSetInvalidError, "no backup set found to restore");
        TEST_RESULT_LOG("P00   WARN: repo1: [BackupSetInvalidError] no backup sets to restore");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid backup set");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
            TEST_RESTORE_BACKUP_INFO
            "\n"
            TEST_RESTORE_BACKUP_INFO_DB);

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptSet, "BOGUS");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(restoreBackupSet(), BackupSetInvalidError, "backup set BOGUS is not valid");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("target time");
        setenv("TZ", "UTC", true);

        const String *repoPath2 = STRDEF(TEST_PATH "/repo2");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "2016-12-19 16:28:04-0500");

        HRN_CFG_LOAD(cfgCmdRestore, argList);

        // Write out backup.info with no current backups to repo1
        HRN_INFO_PUT(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO_DB);

        RestoreBackupData backupData = {0};
        TEST_ASSIGN(backupData, restoreBackupSet(), "get backup set");
        TEST_RESULT_STR_Z(backupData.backupSet, "20161219-212741F_20161219-212803D", "backup set found");
        TEST_RESULT_UINT(backupData.repoIdx, 1, "backup set found, repo2");
        TEST_RESULT_LOG("P00   WARN: repo1: [BackupSetInvalidError] no backup sets to restore");

        // Switch repo paths and confirm same result but on repo1
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "2016-12-19 16:28:04-0500");

        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ASSIGN(backupData, restoreBackupSet(), "get backup set");
        TEST_RESULT_STR_Z(backupData.backupSet, "20161219-212741F_20161219-212803D", "backup set found");
        TEST_RESULT_UINT(backupData.repoIdx, 0, "backup set found, repo1");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("target time, multi repo");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza ,"test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "2016-12-19 16:27:30-0500");

        HRN_CFG_LOAD(cfgCmdRestore, argList);

        #define TEST_RESTORE_BACKUP_INFO_NEWEST                                                                                    \
            "[backup:current]\n"                                                                                                   \
            "20201212-201243F={\"backrest-format\":5,\"backrest-version\":\"2.04\","                                               \
            "\"backup-archive-start\":\"00000007000000000000001C\",\"backup-archive-stop\":\"00000007000000000000001C\","          \
            "\"backup-info-repo-size\":3159776,\"backup-info-repo-size-delta\":3159776,\"backup-info-size\":26897030,"             \
            "\"backup-info-size-delta\":26897030,\"backup-timestamp-start\":1607803000,\"backup-timestamp-stop\":1607803963,"      \
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"                    \
            "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,"   \
            "\"option-online\":true}\n"

        // Write out backup.info with current backup newest to repo2 but still does not satisfy time requirement, so repo1 chosen
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE,
            TEST_RESTORE_BACKUP_INFO_NEWEST
            "\n"
            TEST_RESTORE_BACKUP_INFO_DB);

        TEST_ERROR(
            restoreBackupSet(), BackupSetInvalidError,
            "unable to find backup set with stop time less than '2016-12-19 16:27:30-0500'");

        // Request repo2 - latest from repo2 will be chosen
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreBackupSet(), BackupSetInvalidError,
            "unable to find backup set with stop time less than '2016-12-19 16:27:30-0500'");

        // Switch paths so newest on repo1
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza ,"test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "2016-12-19 16:27:30-0500");

        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreBackupSet(), BackupSetInvalidError,
            "unable to find backup set with stop time less than '2016-12-19 16:27:30-0500'");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza ,"test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "Tue, 15 Nov 1994 12:45:26");

        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreBackupSet(), FormatError,
            "automatic backup set selection cannot be performed with provided time 'Tue, 15 Nov 1994 12:45:26'\n"
            "HINT: time format must be YYYY-MM-DD HH:MM:SS with optional msec and optional timezone (+/- HH or HHMM or HH:MM) - if"
                " timezone is omitted, local time is assumed (for UTC use +00)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("target time, multi repo, no candidates found");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza ,"test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "2016-12-19 16:27:30-0500");

        HRN_CFG_LOAD(cfgCmdRestore, argList);

        // Write out backup.info with no current backups to repo1 and repo2
        HRN_INFO_PUT(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO_DB);
        HRN_INFO_PUT(storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO_DB);

        TEST_ERROR(
            restoreBackupSet(), BackupSetInvalidError,
            "unable to find backup set with stop time less than '2016-12-19 16:27:30-0500'");
        TEST_RESULT_LOG(
            "P00   WARN: repo1: [BackupSetInvalidError] no backup sets to restore\n"
            "P00   WARN: repo2: [BackupSetInvalidError] no backup sets to restore");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("target lsn");

        // Match oldest backup on repo 2
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "lsn");
        hrnCfgArgRawZ(argList, cfgOptTarget, "0/1C000101");

        HRN_CFG_LOAD(cfgCmdRestore, argList);

        // Write out backup.info with no current backups to repo1, with current backups to repo2
        HRN_INFO_PUT(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO_DB);
        HRN_INFO_PUT(storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO "\n" TEST_RESTORE_BACKUP_INFO_DB);

        TEST_ASSIGN(backupData, restoreBackupSet(), "get backup set for lsn 0/1C000101");
        TEST_RESULT_STR_Z(backupData.backupSet, "20161219-212741F", "backup set found");
        TEST_RESULT_UINT(backupData.repoIdx, 1, "backup set found, repo2");
        TEST_RESULT_LOG("P00   WARN: repo1: [BackupSetInvalidError] no backup sets to restore");

        // Switch repo paths and target lsn to match newer backup on repo1
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "lsn");
        hrnCfgArgRawZ(argList, cfgOptTarget, "0/1E000105");

        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ASSIGN(backupData, restoreBackupSet(), "get backup set for lsn 0/1E000105");
        TEST_RESULT_STR_Z(backupData.backupSet, "20161219-212741F_20161219-212918I", "backup set found");
        TEST_RESULT_UINT(backupData.repoIdx, 0, "backup set found, repo1");

        // Log warning if missing backup-lsn-stop is found before finding a match
        // Missing backup-lsn-stop in repo1, no backups in repo2, no qualifying auto-selectable backup in either repo
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "lsn");
        hrnCfgArgRawZ(argList, cfgOptTarget, "0/1C000101");

        // Re-write repo information with set missing backup-lsn-stop
        HRN_INFO_PUT(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO1 "\n" TEST_RESTORE_BACKUP_INFO_DB);

        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreBackupSet(), BackupSetInvalidError, "unable to find backup set with lsn less than or equal to '0/1C000101'");
        TEST_RESULT_LOG(
            "P00   WARN: repo1 reached backup from prior version missing required LSN info before finding a match -- backup"
                " auto-select has been disabled for this repo\n"
            "            HINT: you may specify a backup to restore using the --set option.\n"
            "P00   WARN: repo2: [BackupSetInvalidError] no backup sets to restore");

        // Log warning if missing backup-lsn-stop is found before finding a match
        // Missing backup-lsn-stop in repo1, qualifying auto-selectable backup set in repo2
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "lsn");
        hrnCfgArgRawZ(argList, cfgOptTarget, "0/1C000102");

        // Write repo2 information with data required to find backup
        HRN_INFO_PUT(storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO "\n" TEST_RESTORE_BACKUP_INFO_DB);

        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ASSIGN(backupData, restoreBackupSet(), "get backup set for lsn 0/1C000102");
        TEST_RESULT_STR_Z(backupData.backupSet, "20161219-212741F", "backup set found");
        TEST_RESULT_UINT(backupData.repoIdx, 1, "backup set found, repo2");
        TEST_RESULT_LOG(
            "P00   WARN: repo1 reached backup from prior version missing required LSN info before finding a match -- backup"
                " auto-select has been disabled for this repo\n"
            "            HINT: you may specify a backup to restore using the --set option.");

        // No backups to search for qualifying backup set
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "lsn");
        hrnCfgArgRawZ(argList, cfgOptTarget, "0/1A000102");

        // Write repo info with no current backups
        HRN_INFO_PUT(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO_DB);

        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreBackupSet(), BackupSetInvalidError, "unable to find backup set with lsn less than or equal to '0/1A000102'");
        TEST_RESULT_LOG(
            "P00   WARN: repo2: [BackupSetInvalidError] no backup sets to restore");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreManifestValidate()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on mismatched label");

        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F"), PG_VERSION_94, STRDEF("/pg"));

        TEST_ERROR(
            restoreManifestValidate(manifest, STRDEF("20161219-212741F_20161219-212918I")), FormatError,
            "requested backup '20161219-212741F_20161219-212918I' and manifest label '20161219-212741F' do not match\n"
            "HINT: this indicates some sort of corruption (at the very least paths have been renamed).");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreManifestMap()"))
    {
        const String *pgPath = STRDEF(TEST_PATH "/pg");
        const String *repoPath = STRDEF(TEST_PATH "/repo");
        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F"), PG_VERSION_94, pgPath);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remap data directory");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "base directory is not remapped");
        TEST_RESULT_STR(manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path, pgPath, "base directory is not remapped");

        // Now change pg1-path so the data directory gets remapped
        pgPath = STRDEF(TEST_PATH "/pg2");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "base directory is remapped");
        TEST_RESULT_STR(manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path, pgPath, "base directory is remapped");
        TEST_RESULT_LOG("P00   INFO: remap data directory to '" TEST_PATH "/pg2'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remap tablespaces");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptTablespaceMap, "bogus=/bogus");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(restoreManifestMap(manifest), TablespaceMapError, "unable to remap invalid tablespace 'bogus'");

        // Add some tablespaces
        manifestTargetAdd(
            manifest, &(ManifestTarget){
                .name = STRDEF("pg_tblspc/1"), .path = STRDEF("/1"), .tablespaceId = 1, .tablespaceName = STRDEF("1"),
                .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_tblspc/1"), .destination = STRDEF("/1")});
        manifestTargetAdd(
            manifest, &(ManifestTarget){
                .name = STRDEF("pg_tblspc/2"), .path = STRDEF("/2"), .tablespaceId = 2, .tablespaceName = STRDEF("ts2"),
                .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_tblspc/2"), .destination = STRDEF("/2")});

        // Error on different paths
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptTablespaceMap, "2=/2");
        hrnCfgArgRawZ(argList, cfgOptTablespaceMap, "ts2=/ts2");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreManifestMap(manifest), TablespaceMapError, "tablespace remapped by name 'ts2' and id 2 with different paths");

        // Remap one tablespace using the id and another with the name
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptTablespaceMap, "1=/1-2");
        hrnCfgArgRawZ(argList, cfgOptTablespaceMap, "ts2=/2-2");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap tablespaces");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/1"))->path, "/1-2", "check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/1"))->destination, "/1-2", "check tablespace 1 link");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/2"))->path, "/2-2", "check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/2"))->destination, "/2-2", "check tablespace 1 link");

        TEST_RESULT_LOG(
            "P00   INFO: map tablespace 'pg_tblspc/1' to '/1-2'\n"
            "P00   INFO: map tablespace 'pg_tblspc/2' to '/2-2'");

        // Remap a tablespace using just the id and map the rest with tablespace-map-all
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptTablespaceMap, "2=/2-3");
        hrnCfgArgRawZ(argList, cfgOptTablespaceMapAll, "/all");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap tablespaces");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/1"))->path, "/all/1", "check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/1"))->destination, "/all/1", "check tablespace 1 link");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/2"))->path, "/2-3", "check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/2"))->destination, "/2-3", "check tablespace 1 link");

        TEST_RESULT_LOG(
            "P00   INFO: map tablespace 'pg_tblspc/1' to '/all/1'\n"
            "P00   INFO: map tablespace 'pg_tblspc/2' to '/2-3'");

        // Remap all tablespaces with tablespace-map-all and update version to 9.2 to test warning
        manifest->pub.data.pgVersion = PG_VERSION_92;

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptTablespaceMapAll, "/all2");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap tablespaces");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/1"))->path, "/all2/1", "check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/1"))->destination, "/all2/1", "check tablespace 1 link");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/2"))->path, "/all2/ts2", "check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/2"))->destination, "/all2/ts2", "check tablespace 1 link");

        TEST_RESULT_LOG(
            "P00   INFO: map tablespace 'pg_tblspc/1' to '/all2/1'\n"
            "P00   INFO: map tablespace 'pg_tblspc/2' to '/all2/ts2'\n"
            "P00   WARN: update pg_tablespace.spclocation with new tablespace locations for PostgreSQL <= 9.2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid link");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "bogus=bogus");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreManifestMap(manifest), LinkMapError,
            "unable to map link 'bogus'\n"
            "HINT: Does the link reference a valid backup path or file?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on tablespace remap");

        // Add tablespace link which will be ignored unless specified with link-map
        manifestTargetAdd(
            manifest, &(ManifestTarget){.name = STRDEF("pg_data/pg_tblspc/1"), .path = STRDEF("/tblspc1"),
            .type = manifestTargetTypeLink, .tablespaceId = 1});
        manifestLinkAdd(manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_tblspc/1"), .destination = STRDEF("/tblspc1")});

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "pg_tblspc/1=/ignored");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreManifestMap(manifest), LinkMapError,
            "unable to remap tablespace 'pg_tblspc/1'\n"
            "HINT: use 'tablespace-map' option to remap tablespaces.");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("add file link");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "pg_hba.conf=../conf/pg_hba.conf");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        manifestFileAdd(
            manifest,
            &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/pg_hba.conf"), .size = 4, .timestamp = 1482182860});

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap links");

        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->path, "../conf", "check link path");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->file, "pg_hba.conf", "check link file");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_hba.conf"))->destination, "../conf/pg_hba.conf", "check link dest");
        TEST_RESULT_STR(manifestLinkFind(manifest, STRDEF("pg_data/pg_hba.conf"))->group, groupName(), "check link group");
        TEST_RESULT_STR(manifestLinkFind(manifest, STRDEF("pg_data/pg_hba.conf"))->user, userName(), "check link user");

        TEST_RESULT_LOG("P00   INFO: link 'pg_hba.conf' to '../conf/pg_hba.conf'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid file link path");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "pg_hba.conf=bogus");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreManifestMap(manifest), LinkMapError,
            "'bogus' is not long enough to be the destination for file link 'pg_hba.conf'");

        TEST_RESULT_LOG("P00   INFO: map link 'pg_hba.conf' to 'bogus'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("add path link");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "pg_wal=/wal");
        hrnCfgArgRawBool(argList, cfgOptLinkAll, true);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        manifestPathAdd(manifest, &(ManifestPath){.name = STRDEF(MANIFEST_TARGET_PGDATA "/pg_wal"), .mode = 0700});

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap links");

        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_wal"))->path, "/wal", "check link path");
        TEST_RESULT_PTR(manifestTargetFind(manifest, STRDEF("pg_data/pg_wal"))->file, NULL, "check link file");
        TEST_RESULT_STR_Z(manifestLinkFind(manifest, STRDEF("pg_data/pg_wal"))->destination, "/wal", "check link dest");
        TEST_RESULT_STR(manifestLinkFind(manifest, STRDEF("pg_data/pg_wal"))->group, groupName(), "check link group");
        TEST_RESULT_STR(manifestLinkFind(manifest, STRDEF("pg_data/pg_wal"))->user, userName(), "check link user");

        TEST_RESULT_LOG("P00   INFO: link 'pg_wal' to '/wal'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remap file and path links");

        // Add path link that will not be remapped
        manifestTargetAdd(
            manifest, &(ManifestTarget){.name = STRDEF("pg_data/pg_xact"), .path = STRDEF("/pg_xact"),
            .type = manifestTargetTypeLink});
        manifestLinkAdd(manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_xact"), .destination = STRDEF("/pg_xact")});

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "pg_hba.conf=../conf2/pg_hba2.conf");
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "pg_wal=/wal2");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap links");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->path, "../conf2", "check link path");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->file, "pg_hba2.conf", "check link file");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_hba.conf"))->destination, "../conf2/pg_hba2.conf", "check link dest");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_wal"))->path, "/wal2", "check link path");
        TEST_RESULT_STR_Z(manifestLinkFind(manifest, STRDEF("pg_data/pg_wal"))->destination, "/wal2", "check link dest");
        TEST_RESULT_PTR(manifestTargetFindDefault(manifest, STRDEF("pg_data/pg_xact"), NULL), NULL, "pg_xact target missing");
        TEST_RESULT_PTR(manifestLinkFindDefault(manifest, STRDEF("pg_data/pg_xact"), NULL), NULL, "pg_xact link missing");

        TEST_RESULT_LOG(
            "P00   INFO: map link 'pg_hba.conf' to '../conf2/pg_hba2.conf'\n"
            "P00   INFO: map link 'pg_wal' to '/wal2'\n"
            "P00   WARN: contents of directory link 'pg_xact' will be restored in a directory at the same location");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("preserve all links");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawBool(argList, cfgOptLinkAll, true);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "leave links as they are");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->path, "../conf2", "check link path");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->file, "pg_hba2.conf", "check link file");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_hba.conf"))->destination, "../conf2/pg_hba2.conf", "check link dest");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_wal"))->path, "/wal2", "check link path");
        TEST_RESULT_STR_Z(manifestLinkFind(manifest, STRDEF("pg_data/pg_wal"))->destination, "/wal2", "check link dest");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove all links");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remove all links");

        TEST_RESULT_PTR(
            manifestTargetFindDefault(manifest, STRDEF("pg_data/pg_hba.conf"), NULL), NULL, "pg_hba.conf target missing");
        TEST_RESULT_PTR(manifestLinkFindDefault(manifest, STRDEF("pg_data/pg_hba.conf"), NULL), NULL, "pg_hba.conf link missing");
        TEST_RESULT_PTR(manifestTargetFindDefault(manifest, STRDEF("pg_data/pg_wal"), NULL), NULL, "pg_wal target missing");
        TEST_RESULT_PTR(manifestLinkFindDefault(manifest, STRDEF("pg_data/pg_wal"), NULL), NULL, "pg_wal link missing");

        TEST_RESULT_LOG(
            "P00   WARN: file link 'pg_hba.conf' will be restored as a file at the same location\n"
            "P00   WARN: contents of directory link 'pg_wal' will be restored in a directory at the same location");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreManifestOwner()"))
    {
        userInitInternal();

        const String *pgPath = STRDEF(TEST_PATH "/pg");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("owner is not root and all ownership is good");

        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);
        const String *rootReplaceUser = NULL;
        const String *rootReplaceGroup = NULL;

        TEST_RESULT_VOID(restoreManifestOwner(manifest, &rootReplaceUser, &rootReplaceGroup), "check ownership");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("owner is not root but has no user name");

        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275I"), PG_VERSION_96, pgPath);

        userLocalData.groupName = NULL;
        userLocalData.userName = NULL;

        TEST_RESULT_VOID(restoreManifestOwner(manifest, &rootReplaceUser, &rootReplaceGroup), "check ownership");

        TEST_RESULT_STR(rootReplaceUser, NULL, "root replace user not set");
        TEST_RESULT_STR(rootReplaceGroup, NULL, "root replace group not set");

        TEST_RESULT_LOG(
            "P00   WARN: unknown user '" TEST_USER "' in backup manifest mapped to current user\n"
            "P00   WARN: unknown group '" TEST_GROUP "' in backup manifest mapped to current group");

        userInitInternal();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("owner is not root and some ownership is bad");

        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);

        ManifestPath path = {.name = STRDEF("pg_data/bogus_path"), .user = STRDEF("path-user-bogus")};
        manifestPathAdd(manifest, &path);
        ManifestFile file = {.name = STRDEF("pg_data/bogus_file"), .mode = 0600, .group = STRDEF("file-group-bogus")};
        manifestFileAdd(manifest, &file);
        ManifestLink link = {.name = STRDEF("pg_data/bogus_link"), .destination = STRDEF("/"), .group = STRDEF("link-group-bogus")};
        manifestLinkAdd(manifest, &link);

        TEST_RESULT_VOID(restoreManifestOwner(manifest, &rootReplaceUser, &rootReplaceGroup), "check ownership");

        TEST_RESULT_STR(rootReplaceUser, NULL, "root replace user not set");
        TEST_RESULT_STR(rootReplaceGroup, NULL, "root replace group not set");

        TEST_RESULT_LOG(
            "P00   WARN: unknown user in backup manifest mapped to current user\n"
            "P00   WARN: unknown user 'path-user-bogus' in backup manifest mapped to current user\n"
            "P00   WARN: unknown group in backup manifest mapped to current group\n"
            "P00   WARN: unknown group 'file-group-bogus' in backup manifest mapped to current group\n"
            "P00   WARN: unknown group 'link-group-bogus' in backup manifest mapped to current group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("owner is root and ownership is good");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);
        userLocalData.userRoot = true;

        HRN_STORAGE_PATH_CREATE(storagePgWrite(), NULL, .mode = 0700);

        TEST_RESULT_VOID(restoreManifestOwner(manifest, &rootReplaceUser, &rootReplaceGroup), "check ownership");

        TEST_RESULT_STR(rootReplaceUser, NULL, "root replace user not set");
        TEST_RESULT_STR(rootReplaceGroup, NULL, "root replace group not set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("owner is root and user is bad");

        manifestPathAdd(manifest, &path);

        TEST_RESULT_VOID(restoreManifestOwner(manifest, &rootReplaceUser, &rootReplaceGroup), "check ownership");

        TEST_RESULT_STR(rootReplaceUser, TEST_USER_STR, "root replace user set");
        TEST_RESULT_STR(rootReplaceGroup, TEST_GROUP_STR, "root replace group set");

        TEST_RESULT_LOG("P00   WARN: unknown group in backup manifest mapped to '" TEST_GROUP "'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("owner is root and group is bad");

        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);

        manifestFileAdd(manifest, &file);
        manifestLinkAdd(manifest, &link);

        TEST_RESULT_VOID(restoreManifestOwner(manifest, &rootReplaceUser, &rootReplaceGroup), "check ownership");

        TEST_RESULT_STR(rootReplaceUser, TEST_USER_STR, "root replace user set");
        TEST_RESULT_STR(rootReplaceGroup, TEST_GROUP_STR, "root replace group set");

        TEST_RESULT_LOG("P00   WARN: unknown user in backup manifest mapped to '" TEST_USER "'");

        userInitInternal();

        // -------------------------------------------------------------------------------------------------------------------------
#ifdef TEST_CONTAINER_REQUIRED
        TEST_TITLE("owner is root and ownership of pg_data is bad");

        manifestPathAdd(manifest, &path);
        manifestFileAdd(manifest, &file);

        HRN_SYSTEM_FMT("sudo chown 77777:77777 %s", strZ(pgPath));

        userLocalData.userName = STRDEF("root");
        userLocalData.groupName = STRDEF("root");
        userLocalData.userRoot = true;

        TEST_RESULT_VOID(restoreManifestOwner(manifest, &rootReplaceUser, &rootReplaceGroup), "check ownership");

        TEST_RESULT_STR(rootReplaceUser, STRDEF("root"), "root replace user set");
        TEST_RESULT_STR(rootReplaceGroup, STRDEF("root"), "root replace group set");

        TEST_RESULT_LOG(
            "P00   WARN: unknown user in backup manifest mapped to 'root'\n"
            "P00   WARN: unknown group in backup manifest mapped to 'root'");

        userInitInternal();

#endif // TEST_CONTAINER_REQUIRED
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreClean*()"))
    {
        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("restoreCleanOwnership() update to root (existing)");

        userLocalData.userRoot = true;

        // Expect an error here since we can't really set ownership to root
        TEST_ERROR(
            restoreCleanOwnership(
                TEST_PATH_STR, STRDEF("root"), STRDEF("root"), STRDEF("root"), STRDEF("root"), userId(), groupId(), false),
            FileOwnerError, "unable to set ownership for '" TEST_PATH "': [1] Operation not permitted");

        TEST_RESULT_LOG("P00 DETAIL: update ownership for '" TEST_PATH "'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("restoreCleanOwnership() update to bogus (new)");

        // Will succeed because bogus will be remapped to the current user/group
        restoreCleanOwnership(TEST_PATH_STR, STRDEF("bogus"), NULL, STRDEF("bogus"), NULL, 0, 0, true);

        // Test again with only group for coverage
        restoreCleanOwnership(TEST_PATH_STR, STRDEF("bogus"), NULL, STRDEF("bogus"), NULL, userId(), 0, true);

        userInitInternal();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("directory with bad permissions/mode");

        const String *pgPath = STRDEF(TEST_PATH "/pg");
        const String *repoPath = STRDEF(TEST_PATH "/repo");
        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        HRN_STORAGE_PATH_CREATE(storagePgWrite(), NULL, .mode = 0600);

        userLocalData.userId = TEST_USER_ID + 1;

        TEST_ERROR(
            restoreCleanBuild(manifest, NULL, NULL), PathOpenError,
            "unable to restore to path '" TEST_PATH "/pg' not owned by current user");

        TEST_RESULT_LOG("P00 DETAIL: check '" TEST_PATH "/pg' exists");

        userLocalData.userRoot = true;

        TEST_ERROR(
            restoreCleanBuild(manifest, TEST_USER_STR, TEST_GROUP_STR), PathOpenError,
            "unable to restore to path '" TEST_PATH "/pg' without rwx permissions");

        TEST_RESULT_LOG("P00 DETAIL: check '" TEST_PATH "/pg' exists");

        userInitInternal();

        TEST_ERROR(
            restoreCleanBuild(manifest, NULL, NULL), PathOpenError,
            "unable to restore to path '" TEST_PATH "/pg' without rwx permissions");

        TEST_RESULT_LOG("P00 DETAIL: check '" TEST_PATH "/pg' exists");

        HRN_STORAGE_PATH_REMOVE(storagePgWrite(), NULL);
        HRN_STORAGE_PATH_CREATE(storagePgWrite(), NULL, .mode = 0700);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fail on restore with directory not empty");

        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), PG_FILE_RECOVERYCONF);

        TEST_ERROR(
            restoreCleanBuild(manifest, NULL, NULL), PathNotEmptyError,
            "unable to restore to path '" TEST_PATH "/pg' because it contains files\n"
                "HINT: try using --delta if this is what you intended.");

        TEST_RESULT_LOG("P00 DETAIL: check '" TEST_PATH "/pg' exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when all directories empty");

        HRN_STORAGE_REMOVE(storagePgWrite(), PG_FILE_RECOVERYCONF);

        manifestTargetAdd(
            manifest, &(ManifestTarget){
                .name = STRDEF("pg_data/pg_hba.conf"), .path = STRDEF("../conf"), .file = STRDEF("pg_hba.conf"),
                .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_hba.conf"), .destination = STRDEF("../conf/pg_hba.conf")});

        HRN_STORAGE_PATH_CREATE(storageTest, "conf", .mode = 0700);

        TEST_RESULT_VOID(restoreCleanBuild(manifest, NULL, NULL), "restore");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/conf' exists\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/pg_hba.conf' to '../conf/pg_hba.conf'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when linked file already exists without delta");

        HRN_STORAGE_REMOVE(storagePgWrite(), "pg_hba.conf");
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "../conf/pg_hba.conf");

        TEST_ERROR(
            restoreCleanBuild(manifest, NULL, NULL), FileExistsError,
            "unable to restore file '" TEST_PATH "/conf/pg_hba.conf' because it already exists\n"
            "HINT: try using --delta if this is what you intended.");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/conf' exists");

        HRN_STORAGE_REMOVE(storagePgWrite(), "../conf/pg_hba.conf", .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when all directories empty and ignore recovery.conf");

        HRN_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "preserve");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreCleanBuild(manifest, NULL, NULL), "restore");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/conf' exists\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/pg_hba.conf' to '../conf/pg_hba.conf'");

        HRN_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), PG_FILE_RECOVERYCONF);
        TEST_RESULT_VOID(restoreCleanBuild(manifest, NULL, NULL), "normal restore ignore recovery.conf");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/conf' exists\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/pg_hba.conf' to '../conf/pg_hba.conf'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when all directories empty and PG12 and preserve but no recovery files");

        HRN_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        manifest->pub.data.pgVersion = PG_VERSION_12;

        TEST_RESULT_VOID(restoreCleanBuild(manifest, NULL, NULL), "restore");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/conf' exists\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/pg_hba.conf' to '../conf/pg_hba.conf'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when all directories empty and ignore PG12 recovery files");

        HRN_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        manifestFileAdd(manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_POSTGRESQLAUTOCONF)});

        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF);
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), PG_FILE_RECOVERYSIGNAL);
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), PG_FILE_STANDBYSIGNAL);

        TEST_RESULT_VOID(restoreCleanBuild(manifest, NULL, NULL), "restore");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/conf' exists\n"
            "P00 DETAIL: skip 'postgresql.auto.conf' -- recovery type is preserve\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/pg_hba.conf' to '../conf/pg_hba.conf'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when all directories empty and PG12");

        HRN_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreCleanBuild(manifest, NULL, NULL), "restore");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/conf' exists\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/pg_hba.conf' to '../conf/pg_hba.conf'");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreSelectiveExpression()"))
    {
        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no valid databases");

        StringList *argListClean = strLstNew();
        hrnCfgArgRawZ(argListClean, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argListClean, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argListClean, cfgOptPgPath, "/pg");

        StringList *argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbInclude, UTF8_DB_NAME);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        Manifest *manifest = NULL;

        OBJ_NEW_BEGIN(Manifest, .childQty = MEM_CONTEXT_QTY_MAX)
        {
            manifest = manifestNewInternal();
            manifest->pub.data.pgVersion = PG_VERSION_90;
            manifest->pub.data.pgCatalogVersion = hrnPgCatalogVersion(PG_VERSION_90);

            manifestTargetAdd(manifest, &(ManifestTarget){.name = MANIFEST_TARGET_PGDATA_STR, .path = STRDEF("/pg")});
            manifestFileAdd(manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_PGVERSION)});
        }
        OBJ_NEW_END();

        TEST_ERROR(
            restoreSelectiveExpression(manifest), FormatError,
            "no databases found for selective restore\n"
            "HINT: is this a valid cluster?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("database id is missing on disk");

        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            // Give non-systemId to postgres db
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("postgres"), .id = 16385, .lastSystemId = 99999});
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("template0"), .id = 12168, .lastSystemId = 99999});
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("template1"), .id = 1, .lastSystemId = 99999});
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("user-made-system-db"), .id = 16380, .lastSystemId = 99999});
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF(UTF8_DB_NAME), .id = 16384, .lastSystemId = 99999});
            manifestFileAdd(
                manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/1/" PG_FILE_PGVERSION)});
            manifestFileAdd(
                manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/16381/" PG_FILE_PGVERSION)});
            manifestFileAdd(
                manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/16385/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_END();

        TEST_ERROR(restoreSelectiveExpression(manifest), DbMissingError, "database to include '" UTF8_DB_NAME "' does not exist");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16385)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("database id to exclude is missing on disk");

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbExclude, UTF8_DB_NAME);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(restoreSelectiveExpression(manifest), DbMissingError, "database to exclude '" UTF8_DB_NAME "' does not exist");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16385)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("all databases selected");

        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            manifestFileAdd(
                manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/16384/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_END();

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbInclude, UTF8_DB_NAME);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR(restoreSelectiveExpression(manifest), NULL, "all databases selected");

        TEST_RESULT_LOG(
            "P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385)\n"
            "P00   INFO: nothing to filter - all user databases have been selected");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on system database selected");

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "1");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreSelectiveExpression(manifest), DbInvalidError,
            "system databases (template0, postgres, etc.) are included by default");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on system database with non-systemId selected");

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "16385");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreSelectiveExpression(manifest), DbInvalidError,
            "system databases (template0, postgres, etc.) are included by default");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on system database with non-systemId selected, by name");

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "postgres");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreSelectiveExpression(manifest), DbInvalidError,
            "system databases (template0, postgres, etc.) are included by default");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing database selected");

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "7777777");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(restoreSelectiveExpression(manifest), DbMissingError, "database to include '7777777' does not exist");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("select database by id");

        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("test2"), .id = 32768, .lastSystemId = 99999});
            manifestFileAdd(
                manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/32768/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_END();

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "16384");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(restoreSelectiveExpression(manifest), "(^pg_data/base/32768/)", "check expression");

        TEST_RESULT_LOG(
            "P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768)\n"
            "P00 DETAIL: databases excluded (zeroed) from selective restore (32768)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("one database selected without tablespace id");

        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            manifestTargetAdd(
                manifest, &(ManifestTarget){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/16387"), .tablespaceId = 16387, .tablespaceName = STRDEF("ts1"),
                    .path = STRDEF("/ts1")});
            manifestFileAdd(
                manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/32768/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_END();

        TEST_RESULT_STR_Z(
            restoreSelectiveExpression(manifest), "(^pg_data/base/32768/)|(^pg_tblspc/16387/PG_9.0_201008051/32768/)",
            "check expression");

        TEST_RESULT_LOG(
            "P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768)\n"
            "P00 DETAIL: databases excluded (zeroed) from selective restore (32768)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("one database selected with tablespace id");

        manifest->pub.data.pgVersion = PG_VERSION_94;
        manifest->pub.data.pgCatalogVersion = hrnPgCatalogVersion(PG_VERSION_94);

        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("test3"), .id = 65536, .lastSystemId = 99999});
            manifestFileAdd(
                manifest, &(ManifestFile){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/16387/PG_9.4_201409291/65536/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_END();

        TEST_RESULT_STR_Z(
            restoreSelectiveExpression(manifest),
            "(^pg_data/base/32768/)|(^pg_tblspc/16387/PG_9.4_201409291/32768/)|(^pg_data/base/65536/)"
                "|(^pg_tblspc/16387/PG_9.4_201409291/65536/)",
            "check expression");

        TEST_RESULT_LOG(
            "P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768, 65536)\n"
            "P00 DETAIL: databases excluded (zeroed) from selective restore (32768, 65536)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("exclude database by id");

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbExclude, "16384");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreSelectiveExpression(manifest),
            "(^pg_data/base/16384/)|(^pg_tblspc/16387/PG_9.4_201409291/16384/)",
            "check expression");

        TEST_RESULT_LOG(
            "P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768, 65536)\n"
            "P00 DETAIL: databases excluded (zeroed) from selective restore (16384)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("exclude database by name");

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbExclude, UTF8_DB_NAME);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreSelectiveExpression(manifest),
            "(^pg_data/base/16384/)|(^pg_tblspc/16387/PG_9.4_201409291/16384/)",
            "check expression");

        TEST_RESULT_LOG(
            "P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768, 65536)\n"
            "P00 DETAIL: databases excluded (zeroed) from selective restore (16384)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("exclude system database");

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbExclude, "16385");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreSelectiveExpression(manifest),
            "(^pg_data/base/16385/)|(^pg_tblspc/16387/PG_9.4_201409291/16385/)",
            "check expression");

        TEST_RESULT_LOG(
            "P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768, 65536)\n"
            "P00 DETAIL: databases excluded (zeroed) from selective restore (16385)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing database to exclude selected");

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbExclude, "7777777");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(restoreSelectiveExpression(manifest), DbMissingError, "database to exclude '7777777' does not exist");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768, 65536)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on combining include and exclude options");

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "test2");
        hrnCfgArgRawZ(argList, cfgOptDbExclude, "test2");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(restoreSelectiveExpression(manifest), DbInvalidError, "database to include '32768' is in the exclude list");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768, 65536)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("combine include and exclude options");

        argList = strLstDup(argListClean);
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "16384");
        hrnCfgArgRawZ(argList, cfgOptDbExclude, "1");
        hrnCfgArgRawZ(argList, cfgOptDbExclude, "16385");
        hrnCfgArgRawZ(argList, cfgOptDbExclude, "32768");  // user databases excluded will be silently ignored
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreSelectiveExpression(manifest),
            "(^pg_data/base/1/)|(^pg_tblspc/16387/PG_9.4_201409291/1/)|"
            "(^pg_data/base/16385/)|(^pg_tblspc/16387/PG_9.4_201409291/16385/)|"
            "(^pg_data/base/32768/)|(^pg_tblspc/16387/PG_9.4_201409291/32768/)|"
            "(^pg_data/base/65536/)|(^pg_tblspc/16387/PG_9.4_201409291/65536/)",
            "check expression");

        TEST_RESULT_LOG(
            "P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768, 65536)\n"
            "P00 DETAIL: databases excluded (zeroed) from selective restore (1, 16385, 32768, 65536)");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreRecoveryOption() and restoreRecoveryConf()"))
    {
        StringList *argBaseList = strLstNew();
        hrnCfgArgRawZ(argBaseList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argBaseList, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argBaseList, cfgOptPgPath, "/pg");

        const String *restoreLabel = STRDEF("LABEL");
        #define RECOVERY_SETTING_HEADER                             "# Recovery settings generated by pgBackRest restore on LABEL\n"

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("user-specified options");

        StringList *argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "a-setting=a");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "b_setting=b");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "a_setting = 'a'\n"
            "b_setting = 'b'\n"
            "restore_command = '" TEST_PROJECT_EXE " --lock-path=" HRN_PATH "/lock --log-path=" HRN_PATH " --pg1-path=/pg"
                " --repo1-path=/repo --stanza=test1 archive-get %f \"%p\"'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("user-specified cmd");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptCmd, "/usr/local/bin/pg_wrapper.sh");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = '/usr/local/bin/pg_wrapper.sh --lock-path=" HRN_PATH "/lock --log-path=" HRN_PATH " --pg1-path=/pg"
                " --repo1-path=/repo --stanza=test1 archive-get %f \"%p\"'\n",
            "restore_command invokes /usr/local/bin/pg_wrapper.sh per --cmd option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("override restore_command");

        hrnCfgArgRawZ(argBaseList, cfgOptRecoveryOption, "restore-command=my_restore_command");
        argList = strLstDup(argBaseList);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery target immediate, pg < 12");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptType, "immediate");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_11, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "recovery_target = 'immediate'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery target immediate, pg >= 12");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptType, "immediate");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_12, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "recovery_target = 'immediate'\n"
            "recovery_target_timeline = 'current'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery target time with timeline");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "TIME");
        hrnCfgArgRawZ(argList, cfgOptTargetTimeline, "3");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "recovery_target_time = 'TIME'\n"
            "recovery_target_timeline = '3'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery target inclusive");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "TIME");
        hrnCfgArgRawBool(argList, cfgOptTargetExclusive, true);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "recovery_target_time = 'TIME'\n"
            "recovery_target_inclusive = 'false'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no recovery_target_inclusive for target=name");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptType, "name");
        hrnCfgArgRawZ(argList, cfgOptTarget, "NAME");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "recovery_target_name = 'NAME'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery target lsn");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptType, "lsn");
        hrnCfgArgRawZ(argList, cfgOptTarget, "5218/5E35BBA8");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_10, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "recovery_target_lsn = '5218/5E35BBA8'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery target action = shutdown");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptType, "immediate");
        hrnCfgArgRawZ(argList, cfgOptTargetAction, "shutdown");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_95, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "recovery_target = 'immediate'\n"
            "recovery_target_action = 'shutdown'\n",
            "check recovery options");

        TEST_ERROR(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel), OptionInvalidError,
            "target-action=shutdown is only available in PostgreSQL >= 9.5");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery target action = pause");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptType, "immediate");
        hrnCfgArgRawZ(argList, cfgOptTargetAction, "promote");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "recovery_target = 'immediate'\n"
            "pause_at_recovery_target = 'false'\n",
            "check recovery options");

        TEST_ERROR(
            restoreRecoveryConf(PG_VERSION_90, restoreLabel), OptionInvalidError,
            "target-action option is only available in PostgreSQL >= 9.1");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery type = standby");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptType, "standby");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "standby_mode = 'on'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery type = standby with timeline");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptType, "standby");
        hrnCfgArgRawZ(argList, cfgOptTargetTimeline, "current");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "standby_mode = 'on'\n"
            "recovery_target_timeline = 'current'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when archive-mode set on PG < 12");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptArchiveMode, "off");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel), OptionInvalidError,
            "option 'archive-mode' is not supported on PostgreSQL < 12\n"
                "HINT: 'archive_mode' should be manually set to 'off' in postgresql.conf.");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery type = standby with recovery GUCs and archive-mode=off");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptType, "standby");
        hrnCfgArgRawZ(argList, cfgOptArchiveMode, "off");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_12, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "archive_mode = 'off'\n",
            "check recovery options");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreRecoveryWrite*()"))
    {
        const String *pgPath = STRDEF(TEST_PATH "/pg");
        HRN_STORAGE_PATH_CREATE(storageTest, strZ(pgPath), .mode = 0700);

        const String *restoreLabel = STRDEF("LABEL");
        #define RECOVERY_SETTING_PREFIX                             "# Removed by pgBackRest restore on LABEL # "

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when standby_mode setting is present");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "default");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "standby-mode=on");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreRecoveryWriteAutoConf(PG_VERSION_12, restoreLabel), OptionInvalidError,
            "'standby_mode' setting is not valid for PostgreSQL >= 12\n"
            "HINT: use --type=standby instead of --recovery-option=standby_mode=on.");

        TEST_RESULT_LOG("P00   WARN: postgresql.auto.conf does not exist -- creating to contain recovery settings");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PG12 restore missing postgresql.auto.conf");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "none");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        restoreRecoveryWriteAutoConf(PG_VERSION_12, restoreLabel);

        TEST_STORAGE_GET_EMPTY(storagePg(), PG_FILE_POSTGRESQLAUTOCONF, .comment = "check postgresql.auto.conf");
        TEST_STORAGE_LIST(
            storagePg(), NULL,
            PG_FILE_POSTGRESQLAUTOCONF "\n"
            PG_FILE_RECOVERYSIGNAL "\n",
            .comment = "recovery.signal exists, standby.signal missing");

        TEST_RESULT_LOG(
            "P00   WARN: postgresql.auto.conf does not exist -- creating to contain recovery settings\n"
            "P00   INFO: write " TEST_PATH "/pg/postgresql.auto.conf");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PG12 restore type none");

        HRN_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        HRN_STORAGE_PUT_Z(
            storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF,
            "# DO NOT MODIFY\n"
            "\t recovery_target_action='promote'\n\n");

        restoreRecoveryWriteAutoConf(PG_VERSION_12, restoreLabel);

        TEST_STORAGE_GET(
            storagePg(), PG_FILE_POSTGRESQLAUTOCONF,
            "# DO NOT MODIFY\n"
            RECOVERY_SETTING_PREFIX "\t recovery_target_action='promote'\n\n",
            .comment = "check postgresql.auto.conf");
        TEST_STORAGE_LIST(
            storagePg(), NULL,
            PG_FILE_POSTGRESQLAUTOCONF "\n"
            PG_FILE_RECOVERYSIGNAL "\n",
            .comment = "recovery.signal exists, standby.signal missing");

        TEST_RESULT_LOG("P00   INFO: write updated " TEST_PATH "/pg/postgresql.auto.conf");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PG12 restore type standby and remove existing recovery settings");

        HRN_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        HRN_STORAGE_PUT_Z(
            storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF,
            "# DO NOT MODIFY\n"
            "recovery_target_name\t='name'\n"
            "recovery_target_inclusive = false\n");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "standby");
        hrnCfgArgRawZ(argList, cfgOptRecoveryOption, "restore-command=my_restore_command");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        restoreRecoveryWriteAutoConf(PG_VERSION_12, restoreLabel);

        TEST_STORAGE_GET(
            storagePg(), PG_FILE_POSTGRESQLAUTOCONF,
            "# DO NOT MODIFY\n"
            RECOVERY_SETTING_PREFIX "recovery_target_name\t='name'\n"
            RECOVERY_SETTING_PREFIX "recovery_target_inclusive = false\n"
            "\n"
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n",
            .comment = "check postgresql.auto.conf");
        TEST_STORAGE_LIST(
            storagePg(), NULL,
            PG_FILE_POSTGRESQLAUTOCONF "\n"
            PG_FILE_STANDBYSIGNAL "\n",
            .comment = "recovery.signal missing, standby.signal exists");

        TEST_RESULT_LOG("P00   INFO: write updated " TEST_PATH "/pg/postgresql.auto.conf");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PG12 restore type preserve");

        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F"), PG_VERSION_12, STRDEF("/pg"));

        HRN_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF, "# DO NOT MODIFY\n");
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), PG_FILE_STANDBYSIGNAL);

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "preserve");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        restoreRecoveryWrite(manifest);

        TEST_STORAGE_GET(
            storagePg(), PG_FILE_POSTGRESQLAUTOCONF, "# DO NOT MODIFY\n", .comment = "check postgresql.auto.conf");
        TEST_STORAGE_LIST(
            storagePg(), NULL,
            PG_FILE_POSTGRESQLAUTOCONF "\n"
            PG_FILE_STANDBYSIGNAL "\n",
            .comment = "recovery.signal missing, standby.signal exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PG12 restore type default");

        HRN_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF, "# DO NOT MODIFY\n");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        restoreRecoveryWrite(manifest);

        TEST_RESULT_BOOL(
            bufEq(storageGetP(storageNewReadP(storagePg(), PG_FILE_POSTGRESQLAUTOCONF_STR)), BUFSTRDEF("# DO NOT MODIFY\n")),
            false, "check postgresql.auto.conf has changed");
        TEST_STORAGE_LIST(
            storagePg(), NULL,
            PG_FILE_POSTGRESQLAUTOCONF "\n"
            PG_FILE_RECOVERYSIGNAL "\n",
            .comment = "recovery.signal exists, standby.signal missing");

        TEST_RESULT_LOG("P00   INFO: write updated " TEST_PATH "/pg/postgresql.auto.conf");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdRestore()"))
    {
        const String *pgPath = STRDEF(TEST_PATH "/pg");
        const String *repoPath = STRDEF(TEST_PATH "/repo");
        const String *repoPathEncrpyt = STRDEF(TEST_PATH "/repo-encrypt");

        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("verify next queue calculations");

        TEST_RESULT_INT(restoreJobQueueNext(0, 0, 1), 0, "client idx 0, queue idx 0, 1 queue");
        TEST_RESULT_INT(restoreJobQueueNext(0, 0, 2), 1, "client idx 0, queue idx 0, 2 queues");
        TEST_RESULT_INT(restoreJobQueueNext(1, 1, 2), 0, "client idx 1, queue idx 1, 2 queues");
        TEST_RESULT_INT(restoreJobQueueNext(0, 1, 2), 0, "client idx 0, queue idx 1, 2 queues");
        TEST_RESULT_INT(restoreJobQueueNext(1, 0, 2), 1, "client idx 1, queue idx 0, 2 queues");

        // Locality error
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("incorrect locality");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptPgHost, "pg1");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        TEST_ERROR(cmdRestore(), HostInvalidError, "restore command must be run on the PostgreSQL host");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full restore without delta, multi-repo");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPathEncrpyt);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptSet, "20161219-212741F");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        #define TEST_LABEL                                          "20161219-212741F"
        #define TEST_PGDATA                                         MANIFEST_TARGET_PGDATA "/"
        #define TEST_REPO_PATH                                      STORAGE_REPO_BACKUP "/" TEST_LABEL "/" TEST_PGDATA

        Manifest *manifest = NULL;

        OBJ_NEW_BEGIN(Manifest, .childQty = MEM_CONTEXT_QTY_MAX)
        {
            manifest = manifestNewInternal();
            manifest->pub.info = infoNew(NULL);
            manifest->pub.data.backupLabel = strNewZ(TEST_LABEL);
            manifest->pub.data.pgVersion = PG_VERSION_90;
            manifest->pub.data.pgCatalogVersion = hrnPgCatalogVersion(PG_VERSION_90);
            manifest->pub.data.backupType = backupTypeFull;
            manifest->pub.data.backupTimestampCopyStart = 1482182861; // So file timestamps should be less than this

            // Data directory
            manifestTargetAdd(manifest, &(ManifestTarget){.name = MANIFEST_TARGET_PGDATA_STR, .path = pgPath});
            manifestPathAdd(
                manifest,
                &(ManifestPath){.name = MANIFEST_TARGET_PGDATA_STR, .mode = 0700, .group = groupName(), .user = userName()});

            // Global directory
            manifestPathAdd(
                manifest,
                &(ManifestPath){
                    .name = STRDEF(TEST_PGDATA PG_PATH_GLOBAL), .mode = 0700, .group = groupName(), .user = userName()});

            // PG_VERSION
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA PG_FILE_PGVERSION), .size = 4, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "b74d60e763728399bcd3fb63f7dd1f97b46c6b44"});
            HRN_STORAGE_PUT_Z(storageRepoIdxWrite(0), TEST_REPO_PATH PG_FILE_PGVERSION, PG_VERSION_90_STR "\n");

            // Store the file also to the encrypted repo
            HRN_STORAGE_PUT_Z(
                storageRepoIdxWrite(1), TEST_REPO_PATH PG_FILE_PGVERSION, PG_VERSION_90_STR "\n",
                .cipherType = cipherTypeAes256Cbc, .cipherPass = TEST_CIPHER_PASS_ARCHIVE);

            // pg_tblspc
            manifestPathAdd(
                manifest, &(ManifestPath){
                    .name = STRDEF(MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC), .mode = 0700, .group = groupName(),
                    .user = userName()});

            // Always sort
            lstSort(manifest->pub.targetList, sortOrderAsc);
            lstSort(manifest->pub.fileList, sortOrderAsc);
            lstSort(manifest->pub.linkList, sortOrderAsc);
            lstSort(manifest->pub.pathList, sortOrderAsc);
        }
        OBJ_NEW_END();

        manifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteP(storageRepoIdxWrite(0),
                STRDEF(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE))));

        // Read the manifest, set a cipher passphrase and store it to the encrypted repo
        Manifest *manifestEncrypted = manifestLoadFile(
            storageRepoIdxWrite(0), STRDEF(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE), cipherTypeNone, NULL);
        manifestCipherSubPassSet(manifestEncrypted, STRDEF(TEST_CIPHER_PASS_ARCHIVE));

        // Open file for write
        IoWrite *write = storageWriteIo(
            storageNewWriteP(
                storageRepoIdxWrite(1),
                STRDEF(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE)));

        // Add encryption filter and save the encrypted manifest
        #define TEST_CIPHER_PASS_MANIFEST "backpass"
        cipherBlockFilterGroupAdd(
            ioWriteFilterGroup(write), cfgOptionIdxStrId(cfgOptRepoCipherType, 1), cipherModeEncrypt,
            STRDEF(TEST_CIPHER_PASS_MANIFEST));
        manifestSave(manifestEncrypted, write);

        // Write backup.info to the encrypted repo
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO "\n[cipher]\ncipher-pass=\""
            TEST_CIPHER_PASS_MANIFEST "\"\n\n" TEST_RESTORE_BACKUP_INFO_DB, .cipherType = cipherTypeAes256Cbc);

        TEST_RESULT_VOID(cmdRestore(), "successful restore");

        TEST_RESULT_LOG(
            zNewFmt(
                "P00   WARN: repo1: [FileMissingError] unable to load info file"
                " '%s/repo/backup/test1/backup.info' or '%s/repo/backup/test1/backup.info.copy':\n"
                "            FileMissingError: unable to open missing file '%s/repo/backup/test1/backup.info' for read\n"
                "            FileMissingError: unable to open missing file '%s/repo/backup/test1/backup.info.copy' for read\n"
                "            HINT: backup.info cannot be opened and is required to perform a backup.\n"
                "            HINT: has a stanza-create been performed?\n"
                "P00   INFO: repo2: restore backup set 20161219-212741F\n"
                "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
                "P00 DETAIL: create path '" TEST_PATH "/pg/global'\n"
                "P00 DETAIL: create path '" TEST_PATH "/pg/pg_tblspc'\n"
                "P01 DETAIL: restore file " TEST_PATH "/pg/PG_VERSION (4B, 100.00%%) checksum b74d60e763728399bcd3fb63f7dd1f97b46c6b44"
                    "\n"
                "P00   INFO: write " TEST_PATH "/pg/recovery.conf\n"
                "P00 DETAIL: sync path '" TEST_PATH "/pg'\n"
                "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc'\n"
                "P00   WARN: backup does not contain 'global/pg_control' -- cluster will not start\n"
                "P00 DETAIL: sync path '" TEST_PATH "/pg/global'\n"
                "P00   INFO: restore size = 4B, file total = 1",
                TEST_PATH, TEST_PATH, TEST_PATH, TEST_PATH));

        // Remove recovery.conf before file comparison since it will have a new timestamp.  Make sure it existed, though.
        HRN_STORAGE_REMOVE(storagePgWrite(), PG_FILE_RECOVERYCONF, .errorOnMissing = true);

        TEST_STORAGE_LIST(
            storagePg(), NULL,
            "./\n"
            "PG_VERSION {s=4, t=1482182860}\n"
            "global/\n"
            "pg_tblspc/\n",
            .level = storageInfoLevelBasic, .includeDot = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full restore with delta force");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPathEncrpyt);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "preserve");
        hrnCfgArgRawZ(argList, cfgOptSet, "20161219-212741F");
        hrnCfgArgRawBool(argList, cfgOptDelta, true);
        hrnCfgArgRawBool(argList, cfgOptForce, true);
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        // Munge PGDATA mode so it gets fixed
        HRN_STORAGE_MODE(storagePg(), NULL, 0777);

        // Store backup.info to repo1 - repo1 will be selected because of the priority order
        HRN_INFO_PUT(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO "\n" TEST_RESTORE_BACKUP_INFO_DB);

        // Make sure existing backup.manifest file is ignored
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), BACKUP_MANIFEST_FILE);

        // Add a bogus file that will be removed
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "bogus-file");

        // Add a special file that will be removed
        HRN_SYSTEM_FMT("mkfifo %s/pipe", strZ(pgPath));

        // Modify time of postgresql.conf so it will be copied even though content is the same
        HRN_STORAGE_PUT_Z(storagePgWrite(), "postgresql.conf", "VALID_CONF", .modeFile = 0600);

        // Modify time of postgresql.auto.conf so it will be copied even though content is the same and timestamp matches
        HRN_STORAGE_PUT_Z(
            storagePgWrite(), "postgresql.auto.conf", "VALID_CONF_AUTO", .modeFile = 0600, .timeModified = 1482182861);

        // Size mismatch
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "size-mismatch", .modeFile = 0600);

        // Overwrite PG_VERSION with bogus content that will not be detected by delta force because the time and size are the same
        HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, "BOG\n", .modeFile = 0600, .timeModified = 1482182860);

        // Change destination of tablespace link
        THROW_ON_SYS_ERROR(
            symlink("/bogus", zNewFmt("%s/pg_tblspc/1", strZ(pgPath))) == -1, FileOpenError, "unable to create symlink");

        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            // tablespace_map (will be ignored during restore)
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA PG_FILE_TABLESPACEMAP), .size = 0, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .checksumSha1 = HASH_TYPE_SHA1_ZERO});
            HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), TEST_REPO_PATH PG_FILE_TABLESPACEMAP);

            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(MANIFEST_TARGET_PGDATA "/postgresql.conf"), .size = 10,
                    .timestamp = 1482182860, .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "1a49a3c2240449fee1422e4afcf44d5b96378511"});
            HRN_STORAGE_PUT_Z(storageRepoWrite(), TEST_REPO_PATH "/postgresql.conf", "VALID_CONF");

            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(MANIFEST_TARGET_PGDATA "/postgresql.auto.conf"), .size = 15,
                    .timestamp = 1482182861, .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "37a0c84d42c3ec3d08c311cec2cef2a7ab55a7c3"});
            HRN_STORAGE_PUT_Z(storageRepoWrite(), TEST_REPO_PATH "/postgresql.auto.conf", "VALID_CONF_AUTO");

            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(MANIFEST_TARGET_PGDATA "/size-mismatch"), .size = 1,
                    .timestamp = 1482182861, .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "c032adc1ff629c9b66f22749ad667e6beadf144b"});
            HRN_STORAGE_PUT_Z(storageRepoWrite(), TEST_REPO_PATH "/size-mismatch", "X");

            // pg_tblspc/1
            manifestTargetAdd(
                manifest, &(ManifestTarget){
                    .type = manifestTargetTypeLink, .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1"),
                    .path = STRDEF(TEST_PATH "/ts/1"), .tablespaceId = 1, .tablespaceName = STRDEF("ts1")});
            manifestPathAdd(
                manifest, &(ManifestPath){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC), .mode = 0700, .group = groupName(), .user = userName()});
            manifestPathAdd(
                manifest, &(ManifestPath){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1"), .mode = 0700, .group = groupName(), .user = userName()});
            manifestPathAdd(
                manifest, &(ManifestPath){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1/PG_9.0_201008051"), .mode = 0700, .group = groupName(),
                    .user = userName()});
            manifestLinkAdd(
                manifest, &(ManifestLink){
                    .name = STRDEF(MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC "/1"),
                    .destination = STRDEF(TEST_PATH "/ts/1"), .group = groupName(), .user = userName()});

            // pg_tblspc/1/16384 path
            manifestPathAdd(
                manifest, &(ManifestPath){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1/16384"), .mode = 0700,
                    .group = groupName(), .user = userName()});

            // pg_tblspc/1/16384/PG_VERSION
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1/16384/" PG_FILE_PGVERSION), .size = 4,
                    .timestamp = 1482182860, .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "b74d60e763728399bcd3fb63f7dd1f97b46c6b44"});
            HRN_STORAGE_PUT_Z(
                storageRepoWrite(), STORAGE_REPO_BACKUP "/" TEST_LABEL "/" MANIFEST_TARGET_PGTBLSPC "/1/16384/" PG_FILE_PGVERSION,
                PG_VERSION_90_STR "\n");

            // Always sort
            lstSort(manifest->pub.targetList, sortOrderAsc);
            lstSort(manifest->pub.fileList, sortOrderAsc);
            lstSort(manifest->pub.linkList, sortOrderAsc);
            lstSort(manifest->pub.pathList, sortOrderAsc);
        }
        MEM_CONTEXT_END();

        manifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteP(storageRepoWrite(),
                STRDEF(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE))));

        #undef TEST_LABEL
        #undef TEST_PGDATA
        #undef TEST_REPO_PATH

        cmdRestore();

        TEST_RESULT_LOG(
            "P00   INFO: repo1: restore backup set 20161219-212741F\n"
            "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/ts/1/PG_9.0_201008051' exists\n"
            "P00   INFO: remove invalid files/links/paths from '" TEST_PATH "/pg'\n"
            "P00 DETAIL: update mode for '" TEST_PATH "/pg' to 0700\n"
            "P00 DETAIL: remove invalid file '" TEST_PATH "/pg/bogus-file'\n"
            "P00 DETAIL: remove link '" TEST_PATH "/pg/pg_tblspc/1' because destination changed\n"
            "P00 DETAIL: remove special file '" TEST_PATH "/pg/pipe'\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/pg_tblspc/1' to '" TEST_PATH "/ts/1'\n"
            "P00 DETAIL: create path '" TEST_PATH "/pg/pg_tblspc/1/16384'\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/postgresql.auto.conf (15B, 44.12%)"
                " checksum 37a0c84d42c3ec3d08c311cec2cef2a7ab55a7c3\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/postgresql.conf (10B, 73.53%) checksum"
                " 1a49a3c2240449fee1422e4afcf44d5b96378511\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/PG_VERSION - exists and matches size 4 and modification time 1482182860"
                " (4B, 85.29%) checksum b74d60e763728399bcd3fb63f7dd1f97b46c6b44\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/size-mismatch (1B, 88.24%) checksum c032adc1ff629c9b66f22749ad667e6beadf144b\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/tablespace_map (0B, 88.24%)\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/pg_tblspc/1/16384/PG_VERSION (4B, 100.00%)"
                " checksum b74d60e763728399bcd3fb63f7dd1f97b46c6b44\n"
            "P00   WARN: recovery type is preserve but recovery file does not exist at '" TEST_PATH "/pg/recovery.conf'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc/1'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc/1/16384'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc/1/PG_9.0_201008051'\n"
            "P00   WARN: backup does not contain 'global/pg_control' -- cluster will not start\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/global'\n"
            "P00   INFO: restore size = 34B, file total = 6");

        TEST_STORAGE_LIST(
            storagePg(), NULL,
            "./\n"
            "PG_VERSION {s=4, t=1482182860}\n"
            "global/\n"
            "pg_tblspc/\n"
            "pg_tblspc/1> {d=" TEST_PATH "/ts/1}\n"
            "postgresql.auto.conf {s=15, t=1482182861}\n"
            "postgresql.conf {s=10, t=1482182860}\n"
            "size-mismatch {s=1, t=1482182861}\n"
            "tablespace_map {s=0, t=1482182860}\n",
            .level = storageInfoLevelBasic, .includeDot = true);

        TEST_STORAGE_LIST(
            storagePg(), "pg_tblspc/1",
            ".> {d=" TEST_PATH "/ts/1}\n"
            "16384/\n"
            "16384/PG_VERSION {s=4, t=1482182860}\n"
            "PG_9.0_201008051/\n",
            .level = storageInfoLevelBasic, .includeDot = true);

        // PG_VERSION was not restored because delta force relies on time and size which were the same in the manifest and on disk
        TEST_STORAGE_GET(storagePg(), PG_FILE_PGVERSION, "BOG\n", .comment = "check PG_VERSION was not restored");

        // Cleanup
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);
        HRN_STORAGE_PATH_REMOVE(storageRepoIdxWrite(1), NULL, .recurse = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full restore with force");

        // Replace percent complete and restore size since they can cause a lot of churn when files are added/removed
        hrnLogReplaceAdd(", [0-9]{1,3}\\.[0-9]{2}%\\)", "[0-9]+\\.[0-9]+%", "PCT", false);
        hrnLogReplaceAdd(" restore size = [0-9]+[A-Z]+", "[^ ]+$", "SIZE", false);

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawStrId(argList, cfgOptType, CFGOPTVAL_TYPE_PRESERVE);
        hrnCfgArgRawZ(argList, cfgOptSet, "20161219-212741F");
        hrnCfgArgRawBool(argList, cfgOptForce, true);
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        cmdRestore();

        TEST_RESULT_LOG(
            "P00   INFO: repo1: restore backup set 20161219-212741F\n"
            "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/ts/1/PG_9.0_201008051' exists\n"
            "P00   INFO: remove invalid files/links/paths from '" TEST_PATH "/pg'\n"
            "P00   INFO: remove invalid files/links/paths from '" TEST_PATH "/ts/1/PG_9.0_201008051'\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/postgresql.auto.conf (15B, [PCT]) checksum"
                " 37a0c84d42c3ec3d08c311cec2cef2a7ab55a7c3\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/postgresql.conf (10B, [PCT]) checksum"
                " 1a49a3c2240449fee1422e4afcf44d5b96378511\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/PG_VERSION (4B, [PCT]) checksum b74d60e763728399bcd3fb63f7dd1f97b46c6b44\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/size-mismatch (1B, [PCT]) checksum"
                " c032adc1ff629c9b66f22749ad667e6beadf144b\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/tablespace_map (0B, [PCT])\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/pg_tblspc/1/16384/PG_VERSION (4B, [PCT])"
                " checksum b74d60e763728399bcd3fb63f7dd1f97b46c6b44\n"
            "P00   WARN: recovery type is preserve but recovery file does not exist at '" TEST_PATH "/pg/recovery.conf'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc/1'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc/1/16384'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc/1/PG_9.0_201008051'\n"
            "P00   WARN: backup does not contain 'global/pg_control' -- cluster will not start\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/global'\n"
            "P00   INFO: restore size = [SIZE], file total = 6");

        TEST_STORAGE_LIST(
            storagePg(), NULL,
            "./\n"
            "PG_VERSION {s=4, t=1482182860}\n"
            "global/\n"
            "pg_tblspc/\n"
            "pg_tblspc/1> {d=" TEST_PATH "/ts/1}\n"
            "postgresql.auto.conf {s=15, t=1482182861}\n"
            "postgresql.conf {s=10, t=1482182860}\n"
            "size-mismatch {s=1, t=1482182861}\n"
            "tablespace_map {s=0, t=1482182860}\n",
            .level = storageInfoLevelBasic, .includeDot = true);

        TEST_STORAGE_LIST(
            storagePg(), "pg_tblspc/1",
            ".> {d=" TEST_PATH "/ts/1}\n"
            "16384/\n"
            "16384/PG_VERSION {s=4, t=1482182860}\n"
            "PG_9.0_201008051/\n",
            .level = storageInfoLevelBasic, .includeDot = true);

        // PG_VERSION was restored by the force option
        TEST_STORAGE_GET(storagePg(), PG_FILE_PGVERSION, PG_VERSION_90_STR "\n", .comment = "check PG_VERSION was restored");

        // Remove tablespace
        HRN_STORAGE_PATH_REMOVE(storagePgWrite(), MANIFEST_TARGET_PGTBLSPC "/1/PG_9.0_201008051", .recurse = true);

        // Remove files
        HRN_STORAGE_REMOVE(storagePgWrite(), "postgresql.conf");
        HRN_STORAGE_REMOVE(storagePgWrite(), "postgresql.auto.conf");
        HRN_STORAGE_REMOVE(storagePgWrite(), "size-mismatch");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("incremental delta");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawBool(argList, cfgOptDelta, true);
        hrnCfgArgRawZ(argList, cfgOptType, "none");
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "pg_wal=../wal");
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "postgresql.conf=../config/postgresql.conf");
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "pg_hba.conf=../config/pg_hba.conf");
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "pg_xact=../xact");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        #define TEST_LABEL_FULL                                     "20161219-212741F"
        #define TEST_LABEL_DIFF                                     "20161219-212741F_20161219-212800D"
        #define TEST_LABEL_INCR                                     "20161219-212741F_20161219-212900I"
        #define TEST_LABEL                                          "20161219-212741F_20161219-212918I"
        #define TEST_PGDATA                                         MANIFEST_TARGET_PGDATA "/"
        #define TEST_REPO_PATH                                      STORAGE_REPO_BACKUP "/" TEST_LABEL "/" TEST_PGDATA

        OBJ_NEW_BEGIN(Manifest, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
        {
            manifest = manifestNewInternal();
            manifest->pub.info = infoNew(NULL);
            manifest->pub.data.backupLabel = strNewZ(TEST_LABEL);
            manifest->pub.data.pgVersion = PG_VERSION_10;
            manifest->pub.data.pgCatalogVersion = hrnPgCatalogVersion(PG_VERSION_10);
            manifest->pub.data.backupType = backupTypeIncr;
            manifest->pub.data.backupTimestampCopyStart = 1482182861; // So file timestamps should be less than this

            // Data directory
            manifestTargetAdd(manifest, &(ManifestTarget){.name = MANIFEST_TARGET_PGDATA_STR, .path = pgPath});
            manifestPathAdd(
                manifest,
                &(ManifestPath){.name = MANIFEST_TARGET_PGDATA_STR, .mode = 0700, .group = groupName(), .user = userName()});
            HRN_STORAGE_PATH_CREATE(storagePgWrite(), NULL, .noErrorOnExists = true);

            // global directory
            manifestPathAdd(
                manifest,
                &(ManifestPath){
                    .name = STRDEF(TEST_PGDATA PG_PATH_GLOBAL), .mode = 0700, .group = groupName(), .user = userName()});

            // global/pg_control
            Buffer *fileBuffer = bufNew(8192);
            memset(bufPtr(fileBuffer), 255, bufSize(fileBuffer));
            bufUsedSet(fileBuffer, bufSize(fileBuffer));

            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL), .size = 8192, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "5e2b96c19c4f5c63a5afa2de504d29fe64a4c908"});
            HRN_STORAGE_PUT(storageRepoWrite(), TEST_REPO_PATH PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, fileBuffer);

            // global/888
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA PG_PATH_GLOBAL "/888"), .size = 0, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = HASH_TYPE_SHA1_ZERO});
            HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), TEST_REPO_PATH PG_PATH_GLOBAL "/888");

            // global/999
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA PG_PATH_GLOBAL "/999"), .size = 0, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = HASH_TYPE_SHA1_ZERO});
            HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), TEST_REPO_PATH PG_PATH_GLOBAL "/999");

            // PG_VERSION
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA PG_FILE_PGVERSION), .size = 4, .sizeRepo = 4, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .bundleId = 1, .bundleOffset = 0,
                    .reference = NULL, .checksumSha1 = "8dbabb96e032b8d9f1993c0e4b9141e71ade01a1"});
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "yyy"), .size = 3, .sizeRepo = 3, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .bundleId = 1, .bundleOffset = 8,
                    .reference = NULL, .checksumSha1 = "186154712b2d5f6791d85b9a0987b98fa231779c"});
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "xxxxx"), .size = 5, .sizeRepo = 5, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .bundleId = 1, .bundleOffset = 11,
                    .reference = NULL, .checksumSha1 = "9addbf544119efa4a64223b649750a510f0d463f"});
            // Set bogus sizeRepo and checksumSha1 to ensure this is not handled as a regular file
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "zero-length"), .size = 0, .sizeRepo = 1, .timestamp = 1482182866,
                    .mode = 0600, .group = groupName(), .user = userName(), .bundleId = 1, .bundleOffset = 16,
                    .reference = NULL, .checksumSha1 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"});
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "zz"), .size = 2, .sizeRepo = 2, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .bundleId = 1, .bundleOffset = 17,
                    .reference = NULL, .checksumSha1 = "d7dacae2c968388960bf8970080a980ed5c5dcb7"});
            HRN_STORAGE_PUT_Z(
                storageRepoWrite(), STORAGE_REPO_BACKUP "/" TEST_LABEL "/bundle/1",
                PG_VERSION_94_STR "\n" PG_VERSION_94_STR "\nyyyxxxxxAzzA");

            // base directory
            manifestPathAdd(
                manifest,
                &(ManifestPath){
                    .name = STRDEF(TEST_PGDATA PG_PATH_BASE), .mode = 0700, .group = groupName(), .user = userName()});

            // base/1 directory
            manifestPathAdd(
                manifest,
                &(ManifestPath){
                    .name = STRDEF(TEST_PGDATA PG_PATH_BASE "/1"), .mode = 0700, .group = groupName(), .user = userName()});

            // base/1/PG_VERSION. File was written as part of bundle 1 above
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/1/" PG_FILE_PGVERSION), .size = 4, .sizeRepo = 4, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .bundleId = 1, .bundleOffset = 4,
                    .checksumSha1 = "8dbabb96e032b8d9f1993c0e4b9141e71ade01a1"});

            // base/1/2
            fileBuffer = bufNew(8192);
            memset(bufPtr(fileBuffer), 1, bufSize(fileBuffer));
            bufUsedSet(fileBuffer, bufSize(fileBuffer));

            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/1/2"), .size = 8192, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "4d7b2a36c5387decf799352a3751883b7ceb96aa"});
            HRN_STORAGE_PUT(storageRepoWrite(), TEST_REPO_PATH "base/1/2", fileBuffer);

            // base/1/10
            fileBuffer = bufNew(8194);
            memset(bufPtr(fileBuffer), 10, bufSize(fileBuffer));
            bufPtr(fileBuffer)[0] = 0xFF;
            bufPtr(fileBuffer)[8193] = 0xFF;
            bufUsedSet(fileBuffer, bufSize(fileBuffer));

            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/1/10"), .size = 8192, .sizeRepo = 8192, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .bundleId = 1, .bundleOffset = 1,
                    .reference = STRDEF(TEST_LABEL_FULL), .checksumSha1 = "28757c756c03c37aca13692cb719c18d1510c190"});
            HRN_STORAGE_PUT(storageRepoWrite(), STORAGE_REPO_BACKUP "/" TEST_LABEL_FULL "/bundle/1", fileBuffer);

            // base/1/20 and base/1/21
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/1/20"), .size = 1, .sizeRepo = 1, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .bundleId = 2, .bundleOffset = 1,
                    .reference = STRDEF(TEST_LABEL_DIFF), .checksumSha1 = "c032adc1ff629c9b66f22749ad667e6beadf144b"});
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/1/21"), .size = 1, .sizeRepo = 1, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .bundleId = 2, .bundleOffset = 2,
                    .reference = STRDEF(TEST_LABEL_DIFF), .checksumSha1 = "e9d71f5ee7c92d6dc9e92ffdad17b8bd49418f98"});
            HRN_STORAGE_PUT_Z(storageRepoWrite(), STORAGE_REPO_BACKUP "/" TEST_LABEL_DIFF "/bundle/2", "aXb");

            // base/1/30 and base/1/31
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/1/30"), .size = 1, .sizeRepo = 1, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .bundleId = 2, .bundleOffset = 1,
                    .reference = STRDEF(TEST_LABEL_INCR), .checksumSha1 = "c032adc1ff629c9b66f22749ad667e6beadf144b"});
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/1/31"), .size = 1, .sizeRepo = 1, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .bundleId = 2, .bundleOffset = 2,
                    .reference = STRDEF(TEST_LABEL_INCR), .checksumSha1 = "e9d71f5ee7c92d6dc9e92ffdad17b8bd49418f98"});
            HRN_STORAGE_PUT_Z(storageRepoWrite(), STORAGE_REPO_BACKUP "/" TEST_LABEL_INCR "/bundle/2", "aXb");

            // system db name
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("template1"), .id = 1, .lastSystemId = 99999});

            // base/16384 directory
            manifestPathAdd(
                manifest,
                &(ManifestPath){
                    .name = STRDEF(TEST_PGDATA PG_PATH_BASE "/16384"), .mode = 0700, .group = groupName(), .user = userName()});

            // base/16384/PG_VERSION
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/16384/" PG_FILE_PGVERSION), .size = 4, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "8dbabb96e032b8d9f1993c0e4b9141e71ade01a1"});
            HRN_STORAGE_PUT_Z(storageRepoWrite(), TEST_REPO_PATH "base/16384/" PG_FILE_PGVERSION, PG_VERSION_94_STR "\n");

            // base/16384/16385
            fileBuffer = bufNew(16384);
            memset(bufPtr(fileBuffer), 2, bufSize(fileBuffer));
            bufUsedSet(fileBuffer, bufSize(fileBuffer));

            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/16384/16385"), .size = 16384, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "d74e5f7ebe52a3ed468ba08c5b6aefaccd1ca88f"});
            HRN_STORAGE_PUT(storageRepoWrite(), TEST_REPO_PATH "base/16384/16385", fileBuffer);

            // base/32768 directory
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("test2"), .id = 32768, .lastSystemId = 99999});
            manifestPathAdd(
                manifest,
                &(ManifestPath){
                    .name = STRDEF(TEST_PGDATA PG_PATH_BASE "/32768"), .mode = 0700, .group = groupName(), .user = userName()});

            // base/32768/PG_VERSION
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/32768/" PG_FILE_PGVERSION), .size = 4, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "8dbabb96e032b8d9f1993c0e4b9141e71ade01a1"});
            HRN_STORAGE_PUT_Z(storageRepoWrite(), TEST_REPO_PATH "base/32768/" PG_FILE_PGVERSION, PG_VERSION_94_STR "\n");

            // base/32768/32769
            fileBuffer = bufNew(32768);
            memset(bufPtr(fileBuffer), 2, bufSize(fileBuffer));
            bufUsedSet(fileBuffer, bufSize(fileBuffer));

            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/32768/32769"), .size = 32768, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "a40f0986acb1531ce0cc75a23dcf8aa406ae9081"});
            HRN_STORAGE_PUT(storageRepoWrite(), TEST_REPO_PATH "base/32768/32769", fileBuffer);

            // File link to postgresql.conf
            const String *name = STRDEF(MANIFEST_TARGET_PGDATA "/postgresql.conf");

            manifestTargetAdd(
                manifest, &(ManifestTarget){
                    .type = manifestTargetTypeLink, .name = name, .path = STRDEF("../config"), .file = STRDEF("postgresql.conf")});
            manifestLinkAdd(
                manifest, &(ManifestLink){
                    .name = name, .destination = STRDEF("../config/postgresql.conf"), .group = groupName(), .user = userName()});
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "postgresql.conf"), .size = 15, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "98b8abb2e681e2a5a7d8ab082c0a79727887558d"});
            HRN_STORAGE_PUT_Z(storageRepoWrite(), TEST_REPO_PATH "postgresql.conf", "POSTGRESQL.CONF");

            // File link to pg_hba.conf
            name = STRDEF(MANIFEST_TARGET_PGDATA "/pg_hba.conf");

            manifestTargetAdd(
                manifest, &(ManifestTarget){
                    .type = manifestTargetTypeLink, .name = name, .path = STRDEF("../config"), .file = STRDEF("pg_hba.conf")});
            manifestLinkAdd(
                manifest, &(ManifestLink){
                    .name = name, .destination = STRDEF("../config/pg_hba.conf"), .group = groupName(), .user = userName()});
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "pg_hba.conf"), .size = 11, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "401215e092779574988a854d8c7caed7f91dba4b"});
            HRN_STORAGE_PUT_Z(storageRepoWrite(), TEST_REPO_PATH "pg_hba.conf", "PG_HBA.CONF");

            // tablespace_map (will be ignored during restore)
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA PG_FILE_TABLESPACEMAP), .size = 0, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .checksumSha1 = HASH_TYPE_SHA1_ZERO});
            HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), TEST_REPO_PATH PG_FILE_TABLESPACEMAP);

            // Path link to pg_wal
            name = STRDEF(MANIFEST_TARGET_PGDATA "/pg_wal");
            const String *destination = STRDEF("../wal");

            manifestTargetAdd(manifest, &(ManifestTarget){.type = manifestTargetTypeLink, .name = name, .path = destination});
            manifestPathAdd(manifest, &(ManifestPath){.name = name, .mode = 0700, .group = groupName(), .user = userName()});
            manifestLinkAdd(
                manifest, &(ManifestLink){.name = name, .destination = destination, .group = groupName(), .user = userName()});
            THROW_ON_SYS_ERROR(
                symlink("../wal", zNewFmt("%s/pg_wal", strZ(pgPath))) == -1, FileOpenError, "unable to create symlink");

            // pg_xact path
            manifestPathAdd(
                manifest, &(ManifestPath){.name = STRDEF(MANIFEST_TARGET_PGDATA "/pg_xact"), .mode = 0700, .group = groupName(),
                .user = userName()});

            // pg_tblspc/1
            manifestTargetAdd(
                manifest, &(ManifestTarget){
                    .type = manifestTargetTypeLink, .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1"),
                    .path = STRDEF(TEST_PATH "/ts/1"), .tablespaceId = 1, .tablespaceName = STRDEF("ts1")});
            manifestPathAdd(
                manifest, &(ManifestPath){
                    .name = STRDEF(MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC), .mode = 0700, .group = groupName(),
                    .user = userName()});
            manifestPathAdd(
                manifest, &(ManifestPath){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC), .mode = 0700, .group = groupName(), .user = userName()});
            manifestPathAdd(
                manifest, &(ManifestPath){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1"), .mode = 0700, .group = groupName(), .user = userName()});
            manifestPathAdd(
                manifest, &(ManifestPath){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1/PG_10_201707211"), .mode = 0700, .group = groupName(),
                    .user = userName()});
            manifestLinkAdd(
                manifest, &(ManifestLink){
                    .name = STRDEF(MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC "/1"),
                    .destination = STRDEF(TEST_PATH "/ts/1"), .group = groupName(), .user = userName()});

            // Always sort
            lstSort(manifest->pub.targetList, sortOrderAsc);
            lstSort(manifest->pub.fileList, sortOrderAsc);
            lstSort(manifest->pub.linkList, sortOrderAsc);
            lstSort(manifest->pub.pathList, sortOrderAsc);
        }
        OBJ_NEW_END();

        manifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteP(storageRepoWrite(),
                STRDEF(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE))));

        // Add a few bogus paths/files/links to be removed in delta
        HRN_STORAGE_PATH_CREATE(storagePgWrite(), "bogus1/bogus2");
        HRN_STORAGE_PATH_CREATE(storagePgWrite(), PG_PATH_GLOBAL "/bogus3");

        // Create yyy file so it is not copied
        HRN_STORAGE_PUT_Z(storagePgWrite(), "yyy", "yyy", .modeFile = 0600);

        // Add a few bogus links to be deleted
        THROW_ON_SYS_ERROR(symlink("../wal", zNewFmt("%s/pg_wal2", strZ(pgPath))) == -1, FileOpenError, "unable to create symlink");

        TEST_RESULT_VOID(cmdRestore(), "successful restore");

        TEST_RESULT_LOG(
            "P00   INFO: repo1: restore backup set 20161219-212741F_20161219-212918I\n"
            "P00   INFO: map link 'pg_hba.conf' to '../config/pg_hba.conf'\n"
            "P00   INFO: map link 'pg_wal' to '../wal'\n"
            "P00   INFO: link 'pg_xact' to '../xact'\n"
            "P00   INFO: map link 'postgresql.conf' to '../config/postgresql.conf'\n"
            "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/config' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/wal' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/ts/1/PG_10_201707211' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/xact' exists\n"
            "P00 DETAIL: skip 'tablespace_map' -- tablespace links will be created based on mappings\n"
            "P00   INFO: remove invalid files/links/paths from '" TEST_PATH "/pg'\n"
            "P00 DETAIL: remove invalid path '" TEST_PATH "/pg/bogus1'\n"
            "P00 DETAIL: remove invalid path '" TEST_PATH "/pg/global/bogus3'\n"
            "P00 DETAIL: remove invalid link '" TEST_PATH "/pg/pg_wal2'\n"
            "P00 DETAIL: remove invalid file '" TEST_PATH "/pg/tablespace_map'\n"
            "P00 DETAIL: create path '" TEST_PATH "/pg/base'\n"
            "P00 DETAIL: create path '" TEST_PATH "/pg/base/1'\n"
            "P00 DETAIL: create path '" TEST_PATH "/pg/base/16384'\n"
            "P00 DETAIL: create path '" TEST_PATH "/pg/base/32768'\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/pg_xact' to '../xact'\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/pg_hba.conf' to '../config/pg_hba.conf'\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/postgresql.conf' to '../config/postgresql.conf'\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/32768/32769 (32KB, [PCT]) checksum"
                " a40f0986acb1531ce0cc75a23dcf8aa406ae9081\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/16384/16385 (16KB, [PCT]) checksum"
                " d74e5f7ebe52a3ed468ba08c5b6aefaccd1ca88f\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/global/pg_control.pgbackrest.tmp (8KB, [PCT])"
                " checksum 5e2b96c19c4f5c63a5afa2de504d29fe64a4c908\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/2 (8KB, [PCT]) checksum 4d7b2a36c5387decf799352a3751883b7ceb96aa\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/postgresql.conf (15B, [PCT]) checksum"
                " 98b8abb2e681e2a5a7d8ab082c0a79727887558d\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/pg_hba.conf (11B, [PCT]) checksum"
                " 401215e092779574988a854d8c7caed7f91dba4b\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/32768/PG_VERSION (4B, [PCT])"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/16384/PG_VERSION (4B, [PCT])"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/10 (bundle 20161219-212741F/1/1, 8KB, [PCT])"
                " checksum 28757c756c03c37aca13692cb719c18d1510c190\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/PG_VERSION (bundle 1/0, 4B, [PCT]) checksum"
                " 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/PG_VERSION (bundle 1/4, 4B, [PCT]) checksum"
                " 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/yyy - exists and matches backup (bundle 1/8, 3B, [PCT]) checksum"
                " 186154712b2d5f6791d85b9a0987b98fa231779c\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/xxxxx (bundle 1/11, 5B, [PCT]) checksum"
                " 9addbf544119efa4a64223b649750a510f0d463f\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/zz (bundle 1/17, 2B, [PCT]) checksum"
                " d7dacae2c968388960bf8970080a980ed5c5dcb7\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/20 (bundle 20161219-212741F_20161219-212800D/2/1, 1B, [PCT]) checksum"
                " c032adc1ff629c9b66f22749ad667e6beadf144b\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/21 (bundle 20161219-212741F_20161219-212800D/2/2, 1B, [PCT]) checksum"
                " e9d71f5ee7c92d6dc9e92ffdad17b8bd49418f98\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/30 (bundle 20161219-212741F_20161219-212900I/2/1, 1B, [PCT]) checksum"
                " c032adc1ff629c9b66f22749ad667e6beadf144b\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/31 (bundle 20161219-212741F_20161219-212900I/2/2, 1B, [PCT]) checksum"
                " e9d71f5ee7c92d6dc9e92ffdad17b8bd49418f98\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/global/999 (0B, [PCT])\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/global/888 (0B, [PCT])\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/zero-length (bundle 1/16, 0B, [PCT])\n"
            "P00 DETAIL: sync path '" TEST_PATH "/config'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/base'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/base/1'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/base/16384'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/base/32768'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_wal'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_xact'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc/1'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc/1/PG_10_201707211'\n"
            "P00   INFO: restore global/pg_control (performed last to ensure aborted restores cannot be started)\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/global'\n"
            "P00   INFO: restore size = [SIZE], file total = 21");

        TEST_STORAGE_LIST(
            storagePg(), NULL,
            "./\n"
            "PG_VERSION {s=4, t=1482182860}\n"
            "base/\n"
            "base/1/\n"
            "base/1/10 {s=8192, t=1482182860}\n"
            "base/1/2 {s=8192, t=1482182860}\n"
            "base/1/20 {s=1, t=1482182860}\n"
            "base/1/21 {s=1, t=1482182860}\n"
            "base/1/30 {s=1, t=1482182860}\n"
            "base/1/31 {s=1, t=1482182860}\n"
            "base/1/PG_VERSION {s=4, t=1482182860}\n"
            "base/16384/\n"
            "base/16384/16385 {s=16384, t=1482182860}\n"
            "base/16384/PG_VERSION {s=4, t=1482182860}\n"
            "base/32768/\n"
            "base/32768/32769 {s=32768, t=1482182860}\n"
            "base/32768/PG_VERSION {s=4, t=1482182860}\n"
            "global/\n"
            "global/888 {s=0, t=1482182860}\n"
            "global/999 {s=0, t=1482182860}\n"
            "global/pg_control {s=8192, t=1482182860}\n"
            "pg_hba.conf> {d=../config/pg_hba.conf}\n"
            "pg_tblspc/\n"
            "pg_tblspc/1> {d=" TEST_PATH "/ts/1}\n"
            "pg_wal> {d=../wal}\n"
            "pg_xact> {d=../xact}\n"
            "postgresql.conf> {d=../config/postgresql.conf}\n"
            "xxxxx {s=5, t=1482182860}\n"
            "yyy {s=3, t=1482182860}\n"
            "zero-length {s=0, t=1482182866}\n"
            "zz {s=2, t=1482182860}\n",
            .level = storageInfoLevelBasic, .includeDot = true);

        TEST_STORAGE_LIST(
            storagePg(), "pg_tblspc/1",
            ".> {d=" TEST_PATH "/ts/1}\n"
            "16384/\n"
            "16384/PG_VERSION {s=4, t=1482182860}\n"
            "PG_10_201707211/\n",
            .level = storageInfoLevelBasic, .includeDot = true);

        TEST_STORAGE_LIST(
            storagePg(), "../wal",
            "./\n",
            .level = storageInfoLevelBasic, .includeDot = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("incremental delta selective restore");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, "/repo-bogus");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath);
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH "/spool");
        hrnCfgArgRawBool(argList, cfgOptDelta, true);
        hrnCfgArgRawZ(argList, cfgOptType, "preserve");
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "pg_wal=../wal");
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "postgresql.conf=../config/postgresql.conf");
        hrnCfgArgRawZ(argList, cfgOptLinkMap, "pg_hba.conf=../config/pg_hba.conf");
        hrnCfgArgRawZ(argList, cfgOptDbInclude, "16384");
        HRN_CFG_LOAD(cfgCmdRestore, argList);

        // Move pg1-path and put a link in its place. This tests that restore works when pg1-path is a symlink yet should be
        // completely invisible in the manifest and logging.
        HRN_SYSTEM_FMT("mv %s %s-data", strZ(pgPath), strZ(pgPath));
        HRN_SYSTEM_FMT("ln -s %s-data %s ", strZ(pgPath), strZ(pgPath));

        // Create the stanza archive pool path to check that it gets removed
        HRN_STORAGE_PUT_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE "/empty.txt");

        // Write recovery.conf so we don't get a preserve warning
        HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_RECOVERYCONF, "Some Settings");

        // Change timestamp so it will be updated
        HRN_STORAGE_TIME(storagePgWrite(), "global/999", 777);

        // Enlarge a file so it gets truncated. Keep timestamp the same to prove that it gets updated after the truncate.
        HRN_STORAGE_PUT_Z(
            storagePgWrite(), "base/16384/" PG_FILE_PGVERSION, PG_VERSION_94_STR "\n\n", .modeFile = 0600,
            .timeModified = 1482182860);

        // Enlarge a zero-length file so it gets truncated
        HRN_STORAGE_PUT_Z(storagePgWrite(), "global/888", "X", .modeFile = 0600);

        // Change size so delta will skip based on size
        HRN_STORAGE_PUT_Z(storagePgWrite(), "base/1/2", BOGUS_STR);
        HRN_STORAGE_MODE(storagePgWrite(), "base/1/2", 0600);

        // Covert pg_wal to a path so it will be removed
        HRN_STORAGE_REMOVE(storagePgWrite(), "pg_wal");
        HRN_STORAGE_PATH_CREATE(storagePgWrite(), "pg_wal");

        // Covert pg_hba.conf to a path so it will be removed
        HRN_STORAGE_REMOVE(storagePgWrite(), "pg_hba.conf");
        HRN_STORAGE_PUT_Z(storagePgWrite(), "pg_hba.conf", BOGUS_STR);

        // Update the manifest with online = true to test recovery start time logging
        manifest->pub.data.backupOptionOnline = true;
        manifest->pub.data.backupTimestampStart = 1482182958;

        hrnLogReplaceAdd("[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}", NULL, "TIME", false);

        manifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteP(storageRepoWrite(),
                STRDEF(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE))));

        TEST_RESULT_VOID(cmdRestore(), "successful restore");

        TEST_RESULT_LOG(
            "P00   INFO: repo2: restore backup set 20161219-212741F_20161219-212918I, recovery will start at [TIME]\n"
            "P00   INFO: map link 'pg_hba.conf' to '../config/pg_hba.conf'\n"
            "P00   INFO: map link 'pg_wal' to '../wal'\n"
            "P00   INFO: map link 'postgresql.conf' to '../config/postgresql.conf'\n"
            "P00 DETAIL: databases found for selective restore (1, 16384, 32768)\n"
            "P00 DETAIL: databases excluded (zeroed) from selective restore (32768)\n"
            "P00 DETAIL: check '" TEST_PATH "/pg' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/config' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/wal' exists\n"
            "P00 DETAIL: check '" TEST_PATH "/ts/1/PG_10_201707211' exists\n"
            "P00 DETAIL: skip 'tablespace_map' -- tablespace links will be created based on mappings\n"
            "P00 DETAIL: remove 'global/pg_control' so cluster will not start if restore does not complete\n"
            "P00   INFO: remove invalid files/links/paths from '" TEST_PATH "/pg'\n"
            "P00 DETAIL: remove invalid file '" TEST_PATH "/pg/pg_hba.conf'\n"
            "P00 DETAIL: remove invalid path '" TEST_PATH "/pg/pg_wal'\n"
            "P00 DETAIL: remove invalid link '" TEST_PATH "/pg/pg_xact'\n"
            "P00   INFO: remove invalid files/links/paths from '" TEST_PATH "/wal'\n"
            "P00   INFO: remove invalid files/links/paths from '" TEST_PATH "/ts/1/PG_10_201707211'\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/pg_wal' to '../wal'\n"
            "P00 DETAIL: create path '" TEST_PATH "/pg/pg_xact'\n"
            "P00 DETAIL: create symlink '" TEST_PATH "/pg/pg_hba.conf' to '../config/pg_hba.conf'\n"
            "P01 DETAIL: restore zeroed file " TEST_PATH "/pg/base/32768/32769 (32KB, [PCT])\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/16384/16385 - exists and matches backup (16KB, [PCT])"
                " checksum d74e5f7ebe52a3ed468ba08c5b6aefaccd1ca88f\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/global/pg_control.pgbackrest.tmp (8KB, [PCT])"
                " checksum 5e2b96c19c4f5c63a5afa2de504d29fe64a4c908\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/2 (8KB, [PCT]) checksum 4d7b2a36c5387decf799352a3751883b7ceb96aa\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/postgresql.conf - exists and matches backup (15B, [PCT])"
                " checksum 98b8abb2e681e2a5a7d8ab082c0a79727887558d\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/pg_hba.conf - exists and matches backup (11B, [PCT])"
                " checksum 401215e092779574988a854d8c7caed7f91dba4b\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/32768/PG_VERSION - exists and matches backup (4B, [PCT])"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/16384/PG_VERSION - exists and matches backup (4B, [PCT])"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/10 - exists and matches backup (bundle 20161219-212741F/1/1, 8KB,"
                " [PCT]) checksum 28757c756c03c37aca13692cb719c18d1510c190\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/PG_VERSION - exists and matches backup (bundle 1/0, 4B, [PCT])"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/PG_VERSION - exists and matches backup (bundle 1/4, 4B, [PCT])"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/yyy - exists and matches backup (bundle 1/8, 3B, [PCT]) checksum"
                " 186154712b2d5f6791d85b9a0987b98fa231779c\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/xxxxx - exists and matches backup (bundle 1/11, 5B, [PCT]) checksum"
                " 9addbf544119efa4a64223b649750a510f0d463f\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/zz - exists and matches backup (bundle 1/17, 2B, [PCT]) checksum"
                " d7dacae2c968388960bf8970080a980ed5c5dcb7\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/20 - exists and matches backup (bundle"
                " 20161219-212741F_20161219-212800D/2/1, 1B, [PCT]) checksum c032adc1ff629c9b66f22749ad667e6beadf144b\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/21 - exists and matches backup (bundle"
                " 20161219-212741F_20161219-212800D/2/2, 1B, [PCT]) checksum e9d71f5ee7c92d6dc9e92ffdad17b8bd49418f98\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/30 - exists and matches backup (bundle"
                " 20161219-212741F_20161219-212900I/2/1, 1B, [PCT]) checksum c032adc1ff629c9b66f22749ad667e6beadf144b\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/base/1/31 - exists and matches backup (bundle"
                " 20161219-212741F_20161219-212900I/2/2, 1B, [PCT]) checksum e9d71f5ee7c92d6dc9e92ffdad17b8bd49418f98\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/global/999 - exists and is zero size (0B, [PCT])\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/global/888 - exists and is zero size (0B, [PCT])\n"
            "P01 DETAIL: restore file " TEST_PATH "/pg/zero-length - exists and is zero size (bundle 1/16, 0B, [PCT])\n"
            "P00 DETAIL: sync path '" TEST_PATH "/config'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/base'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/base/1'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/base/16384'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/base/32768'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_wal'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_xact'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc/1'\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/pg_tblspc/1/PG_10_201707211'\n"
            "P00   INFO: restore global/pg_control (performed last to ensure aborted restores cannot be started)\n"
            "P00 DETAIL: sync path '" TEST_PATH "/pg/global'\n"
            "P00   INFO: restore size = [SIZE], file total = 21");

        // Check stanza archive spool path was removed
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_PATH_ARCHIVE);

        // -------------------------------------------------------------------------------------------------------------------------
        // Keep this test at the end since is corrupts the repo
        TEST_TITLE("remove a repo file so a restore job errors");

        HRN_STORAGE_REMOVE(storageRepoWrite(), TEST_REPO_PATH PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, .errorOnMissing = true);
        HRN_STORAGE_REMOVE(storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, .errorOnMissing = true);

        HRN_CFG_LOAD(cfgCmdRestore, argList, .jobRetry = 1);

        // Set log level to warn
        harnessLogLevelSet(logLevelWarn);

        TEST_ERROR(
            cmdRestore(), FileMissingError,
            "raised from local-1 shim protocol: unable to open missing file"
                " '" TEST_PATH "/repo/backup/test1/20161219-212741F_20161219-212918I/pg_data/global/pg_control' for read\n"
            "[FileMissingError] on retry after 0ms");

        // Free local processes that were not freed because of the error
        protocolFree();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
