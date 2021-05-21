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
    "\"backup-info-size-delta\":26897030,\"backup-timestamp-start\":1482182846,\"backup-timestamp-stop\":1482182861,"              \
    "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"                            \
    "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,"           \
    "\"option-online\":true}\n"                                                                                                    \
    "20161219-212741F_20161219-212803D={\"backrest-format\":5,\"backrest-version\":\"2.04\","                                      \
    "\"backup-archive-start\":\"00000008000000000000001E\",\"backup-archive-stop\":\"00000008000000000000001E\","                  \
    "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"                       \
    "\"backup-info-size-delta\":163866,\"backup-prior\":\"20161219-212741F\",\"backup-reference\":[\"20161219-212741F\"],"         \
    "\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,\"backup-type\":\"diff\",\"db-id\":1,"             \
    "\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"                                 \
    "\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"                  \
    "20161219-212741F_20161219-212918I={\"backrest-format\":5,\"backrest-version\":\"2.04\","                                      \
    "\"backup-archive-start\":null,\"backup-archive-stop\":null,"                                                                  \
    "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"                       \
    "\"backup-info-size-delta\":163866,\"backup-prior\":\"20161219-212741F\",\"backup-reference\":[\"20161219-212741F\","          \
    "\"20161219-212741F_20161219-212803D\"],"                                                                                      \
    "\"backup-timestamp-start\":1482182884,\"backup-timestamp-stop\":1482182985,\"backup-type\":\"incr\",\"db-id\":1,"             \
    "\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"                                 \
    "\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"

/***********************************************************************************************************************************
Test restores to be sure they match the manifest
***********************************************************************************************************************************/
static void
testRestoreCompare(const Storage *storage, const String *pgPath, const Manifest *manifest, const char *compare)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STORAGE, storage);
        FUNCTION_HARNESS_PARAM(STRING, pgPath);
        FUNCTION_HARNESS_PARAM(MANIFEST, manifest);
        FUNCTION_HARNESS_PARAM(STRINGZ, compare);
    FUNCTION_HARNESS_END();

    // Get the pg-path as a string
    HarnessStorageInfoListCallbackData callbackData =
    {
        .content = strNew(""),
        .modeOmit = true,
        .modePath = 0700,
        .modeFile = 0600,
        .userOmit = true,
        .groupOmit = true,
    };

    TEST_RESULT_VOID(
        storageInfoListP(storage, pgPath, hrnStorageInfoListCallback, &callbackData, .recurse = true, .sortOrder = sortOrderAsc),
        "pg path info list for restore compare");

    // Compare
    TEST_RESULT_STR_Z(callbackData.content, hrnReplaceKey(compare), "    compare result manifest");

    FUNCTION_HARNESS_RETURN_VOID();
}

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

    MEM_CONTEXT_NEW_BEGIN("Manifest")
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
    MEM_CONTEXT_NEW_END();

    FUNCTION_HARNESS_RETURN(MANIFEST, result);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Install local command handler shim
    static const ProtocolServerHandler testLocalHandlerList[] = {PROTOCOL_SERVER_HANDLER_RESTORE_LIST};
    hrnProtocolLocalShimInstall(testLocalHandlerList, PROTOCOL_SERVER_HANDLER_LIST_SIZE(testLocalHandlerList));

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    // *****************************************************************************************************************************
    if (testBegin("restoreFile()"))
    {
        const String *repoFileReferenceFull = strNew("20190509F");
        const String *repoFile1 = strNew("pg_data/testfile");
        unsigned int repoIdx = 0;

        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        harnessCfgLoad(cfgCmdRestore, argList);

        // Create the pg path
        storagePathCreateP(storagePgWrite(), NULL, .mode = 0700);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeNone, strNew("sparse-zero"),
                strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), true, 0x10000000000UL, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 0, true, false, NULL),
            false, "zero sparse 1TB file");
        TEST_RESULT_UINT(storageInfoP(storagePg(), strNew("sparse-zero")).size, 0x10000000000UL, "    check size");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeNone, strNew("normal-zero"),
                strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 0, false, false, NULL),
            true, "zero-length file");
        TEST_RESULT_UINT(storageInfoP(storagePg(), strNew("normal-zero")).size, 0, "    check size");

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a compressed encrypted repo file
        StorageWrite *ceRepoFile = storageNewWriteP(
            storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strZ(repoFileReferenceFull), strZ(repoFile1)));
        IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(ceRepoFile));
        ioFilterGroupAdd(filterGroup, compressFilter(compressTypeGz, 3));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRDEF("badpass"), NULL));

        storagePutP(ceRepoFile, BUFSTRDEF("acefile"));

        TEST_ERROR(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeGz, strNew("normal"),
                strNew("ffffffffffffffffffffffffffffffffffffffff"), false, 7, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 0, false, false, strNew("badpass")),
            ChecksumError,
            "error restoring 'normal': actual checksum 'd1cd8a7d11daa26814b93eb604e1d49ab4b43770' does not match expected checksum"
                " 'ffffffffffffffffffffffffffffffffffffffff'");

        // Create normal file to make it look like a prior restore file failed partway through to ensure that retries work. It will
        // be clear if the file was overwritten when checking the info below since the size and timestamp will be changed.
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storagePgWrite(), STRDEF("normal"), .modeFile = 0600), BUFSTRDEF("PRT")),
            "    create normal file");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeGz, strNew("normal"),
                strNew("d1cd8a7d11daa26814b93eb604e1d49ab4b43770"), false, 7, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 0, false, false, strNew("badpass")),
            true, "copy file");

        StorageInfo info = storageInfoP(storagePg(), strNew("normal"));
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_UINT(info.size, 7, "    check size");
        TEST_RESULT_UINT(info.mode, 0600, "    check mode");
        TEST_RESULT_INT(info.timeModified, 1557432154, "    check time");
        TEST_RESULT_STR_Z(info.user, testUser(), "    check user");
        TEST_RESULT_STR_Z(info.group, testGroup(), "    check group");
        TEST_RESULT_STR_Z(strNewBuf(storageGetP(storageNewReadP(storagePg(), strNew("normal")))), "acefile", "    check contents");

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a repo file
        storagePutP(
            storageNewWriteP(
                storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strZ(repoFileReferenceFull), strZ(repoFile1))),
            BUFSTRDEF("atestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeNone, strNew("delta"),
                strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 9, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta missing");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storagePg(), strNew("delta")))), "atestfile", "    check contents");

        size_t oldBufferSize = ioBufferSize();
        ioBufferSizeSet(4);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeNone, strNew("delta"),
                strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 9, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 0, true, false, NULL),
            false, "sha1 delta existing");

        ioBufferSizeSet(oldBufferSize);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeNone, strNew("delta"),
                strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 9, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 1557432155, true, true, NULL),
            false, "sha1 delta force existing");

        // Change the existing file so it no longer matches by size
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("delta")), BUFSTRDEF("atestfile2"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeNone, strNew("delta"),
                strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 9, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta existing, size differs");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storagePg(), strNew("delta")))), "atestfile", "    check contents");

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("delta")), BUFSTRDEF("atestfile2"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeNone, strNew("delta"),
                strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 9, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 1557432155, true, true, NULL),
            true, "delta force existing, size differs");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storagePg(), strNew("delta")))), "atestfile", "    check contents");

        // Change the existing file so it no longer matches by content
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("delta")), BUFSTRDEF("btestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeNone, strNew("delta"),
                strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 9, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta existing, content differs");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storagePg(), strNew("delta")))), "atestfile", "    check contents");

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("delta")), BUFSTRDEF("btestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeNone, strNew("delta"),
                strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 9, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 1557432155, true, true, NULL),
            true, "delta force existing, timestamp differs");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeNone, strNew("delta"),
                strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 9, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 1557432153, true, true, NULL),
            true, "delta force existing, timestamp after copy time");

        // Change the existing file to zero-length
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("delta")), BUFSTRDEF(""));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoIdx, repoFileReferenceFull, compressTypeNone, strNew("delta"),
                strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, 1557432154, 0600, strNew(testUser()),
                strNew(testGroup()), 0, true, false, NULL),
            false, "sha1 delta existing, content differs");
    }

    // *****************************************************************************************************************************
    if (testBegin("restorePathValidate()"))
    {
        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on data directory missing");

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR_FMT(restorePathValidate(), PathMissingError, "$PGDATA directory '%s/pg' does not exist", testPath());

        // Create PGDATA
        storagePathCreateP(storagePgWrite(), NULL);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when pg appears to be running");

        storagePutP(storageNewWriteP(storagePgWrite(), strNew("postmaster.pid")), NULL);

        TEST_ERROR_FMT(
            restorePathValidate(), PgRunningError,
            "unable to restore while PostgreSQL is running\n"
                "HINT: presence of 'postmaster.pid' in '%s/pg' indicates PostgreSQL is running.\n"
                "HINT: remove 'postmaster.pid' only if PostgreSQL is not running.",
            testPath());

        storageRemoveP(storagePgWrite(), strNew("postmaster.pid"), .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on data directory does not look valid");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--delta");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restorePathValidate(), "restore --delta with invalid PGDATA");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptDelta), false, "--delta set to false");
        TEST_RESULT_LOG(
            "P00   WARN: --delta or --force specified but unable to find 'PG_VERSION' or 'backup.manifest' in '{[path]}/pg' to"
                " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                " exist in the destination directories the restore will be aborted.");

        harnessCfgLoad(cfgCmdRestore, argList);
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("backup.manifest")), NULL);
        TEST_RESULT_VOID(restorePathValidate(), "restore --delta with valid PGDATA");
        storageRemoveP(storagePgWrite(), strNew("backup.manifest"), .errorOnMissing = true);

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAddZ(argList, "--force");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restorePathValidate(), "restore --force with invalid PGDATA");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptForce), false, "--force set to false");
        TEST_RESULT_LOG(
            "P00   WARN: --delta or --force specified but unable to find 'PG_VERSION' or 'backup.manifest' in '{[path]}/pg' to"
                " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                " exist in the destination directories the restore will be aborted.");

        harnessCfgLoad(cfgCmdRestore, argList);
        storagePutP(storageNewWriteP(storagePgWrite(), strNew(PG_FILE_PGVERSION)), NULL);
        TEST_RESULT_VOID(restorePathValidate(), "restore --force with valid PGDATA");
        storageRemoveP(storagePgWrite(), strNew(PG_FILE_PGVERSION), .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("getEpoch()"))
    {
        TEST_TITLE("system time UTC");

        setenv("TZ", "UTC", true);
        TEST_RESULT_INT(getEpoch(strNew("2020-01-08 09:18:15-0700")), 1578500295, "epoch with timezone");
        TEST_RESULT_INT(getEpoch(strNew("2020-01-08 16:18:15.0000")), 1578500295, "same epoch no timezone");
        TEST_RESULT_INT(getEpoch(strNew("2020-01-08 16:18:15.0000+00")), 1578500295, "same epoch timezone 0");
        TEST_ERROR_FMT(getEpoch(strNew("2020-13-08 16:18:15")), FormatError, "invalid date 2020-13-08");
        TEST_ERROR_FMT(getEpoch(strNew("2020-01-08 16:68:15")), FormatError, "invalid time 16:68:15");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("system time America/New_York");

        setenv("TZ", "America/New_York", true);
        time_t testTime = 1573754569;
        char timeBuffer[20];
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localtime(&testTime));
        TEST_RESULT_Z(timeBuffer, "2019-11-14 13:02:49", "check timezone set");
        TEST_RESULT_INT(getEpoch(strNew("2019-11-14 13:02:49-0500")), 1573754569, "offset same as local");
        TEST_RESULT_INT(getEpoch(strNew("2019-11-14 13:02:49")), 1573754569, "GMT-0500 (EST)");
        TEST_RESULT_INT(getEpoch(strNew("2019-09-14 20:02:49")), 1568505769, "GMT-0400 (EDT)");
        TEST_RESULT_INT(getEpoch(strNew("2018-04-27 04:29:00+04:30")), 1524787140, "GMT+0430");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid target time format");

        TEST_RESULT_INT(getEpoch(strNew("Tue, 15 Nov 1994 12:45:26")), 0, "invalid date time format");
        TEST_RESULT_LOG(
            "P00   WARN: automatic backup set selection cannot be performed with provided time 'Tue, 15 Nov 1994 12:45:26',"
            " latest backup set will be used\n"
            "            HINT: time format must be YYYY-MM-DD HH:MM:SS with optional msec and optional timezone"
            " (+/- HH or HHMM or HH:MM) - if timezone is omitted, local time is assumed (for UTC use +00)");

        setenv("TZ", "UTC", true);
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreBackupSet()"))
    {
        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        harnessCfgLoad(cfgCmdRestore, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when no backups are present");

        HRN_INFO_PUT(storageRepoWrite(), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO_DB);
        TEST_ERROR_FMT(restoreBackupSet(), BackupSetInvalidError, "no backup set found to restore");
        TEST_RESULT_LOG("P00   WARN: repo1: [BackupSetInvalidError] no backup sets to restore");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid backup set");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
            TEST_RESTORE_BACKUP_INFO
            "\n"
            TEST_RESTORE_BACKUP_INFO_DB);

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--set=BOGUS");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(restoreBackupSet(), BackupSetInvalidError, "backup set BOGUS is not valid");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("target time");
        setenv("TZ", "UTC", true);

        const String *repoPath2 = strNewFmt("%s/repo2", testPath());

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath);
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--type=time");
        strLstAddZ(argList, "--target=2016-12-19 16:28:04-0500");

        harnessCfgLoad(cfgCmdRestore, argList);

        // Write out backup.info with no current backups to repo1
        HRN_INFO_PUT(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO_DB);

        RestoreBackupData backupData = {0};
        TEST_ASSIGN(backupData, restoreBackupSet(), "get backup set");
        TEST_RESULT_STR_Z(backupData.backupSet, "20161219-212741F_20161219-212803D", "backup set found");
        TEST_RESULT_UINT(backupData.repoIdx, 1, "backup set found, repo2");
        TEST_RESULT_LOG("P00   WARN: repo1: [BackupSetInvalidError] no backup sets to restore");

        // Switch repo paths and confirm same result but on repo1
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath2);
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--type=time");
        strLstAddZ(argList, "--target=2016-12-19 16:28:04-0500");

        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ASSIGN(backupData, restoreBackupSet(), "get backup set");
        TEST_RESULT_STR_Z(backupData.backupSet, "20161219-212741F_20161219-212803D", "backup set found");
        TEST_RESULT_UINT(backupData.repoIdx, 0, "backup set found, repo1");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("target time, multi repo, latest used");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza ,"test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "2016-12-19 16:27:30-0500");

        harnessCfgLoad(cfgCmdRestore, argList);

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

        TEST_ASSIGN(backupData, restoreBackupSet(), "get backup set");
        TEST_RESULT_STR_Z(backupData.backupSet, "20161219-212741F_20161219-212918I", "default to latest backup set");
        TEST_RESULT_UINT(backupData.repoIdx, 0, "repo1 chosen because of priority order");
        TEST_RESULT_LOG(
            "P00   WARN: unable to find backup set with stop time less than '2016-12-19 16:27:30-0500', repo1: latest backup set"
            " will be used");

        // Request repo2 - latest from repo2 will be chosen
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ASSIGN(backupData, restoreBackupSet(), "get backup set");
        TEST_RESULT_STR_Z(backupData.backupSet, "20201212-201243F", "default to latest backup set");
        TEST_RESULT_UINT(backupData.repoIdx, 1, "repo2 chosen because repo option set");
        TEST_RESULT_LOG(
            "P00   WARN: unable to find backup set with stop time less than '2016-12-19 16:27:30-0500', repo2: latest backup set"
            " will be used");

        // Switch paths so newest on repo1
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza ,"test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "2016-12-19 16:27:30-0500");

        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ASSIGN(backupData, restoreBackupSet(), "get backup set");
        TEST_RESULT_STR_Z(backupData.backupSet, "20201212-201243F", "default to latest backup set");
        TEST_RESULT_UINT(backupData.repoIdx, 0, "repo1 chosen because of priority order");
        TEST_RESULT_LOG(
            "P00   WARN: unable to find backup set with stop time less than '2016-12-19 16:27:30-0500', repo1: latest backup set"
            " will be used");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza ,"test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "Tue, 15 Nov 1994 12:45:26");

        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ASSIGN(backupData, restoreBackupSet(), "get backup set");
        TEST_RESULT_STR_Z(backupData.backupSet, "20161219-212741F_20161219-212918I", "time invalid format, default latest");
        TEST_RESULT_UINT(backupData.repoIdx, 0, "repo1 chosen because of priority order");
        TEST_RESULT_LOG(
            "P00   WARN: automatic backup set selection cannot be performed with provided time 'Tue, 15 Nov 1994 12:45:26',"
            " latest backup set will be used\n"
            "            HINT: time format must be YYYY-MM-DD HH:MM:SS with optional msec and optional timezone"
            " (+/- HH or HHMM or HH:MM) - if timezone is omitted, local time is assumed (for UTC use +00)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("target time, multi repo, no candidates found");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza ,"test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath2);
        hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pgPath);
        hrnCfgArgRawZ(argList, cfgOptType, "time");
        hrnCfgArgRawZ(argList, cfgOptTarget, "2016-12-19 16:27:30-0500");

        harnessCfgLoad(cfgCmdRestore, argList);

        // Write out backup.info with no current backups to repo1 and repo2
        HRN_INFO_PUT(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO_DB);
        HRN_INFO_PUT(storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO_DB);

        TEST_ERROR_FMT(restoreBackupSet(), BackupSetInvalidError, "no backup set found to restore");
        TEST_RESULT_LOG(
            "P00   WARN: repo1: [BackupSetInvalidError] no backup sets to restore\n"
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
        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());
        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F"), PG_VERSION_94, pgPath);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remap data directory");

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "base directory is not remapped");
        TEST_RESULT_STR(manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path, pgPath, "base directory is not remapped");

        // Now change pg1-path so the data directory gets remapped
        pgPath = strNewFmt("%s/pg2", testPath());

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "base directory is remapped");
        TEST_RESULT_STR(manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path, pgPath, "base directory is remapped");
        TEST_RESULT_LOG("P00   INFO: remap data directory to '{[path]}/pg2'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remap tablespaces");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--tablespace-map=bogus=/bogus");
        harnessCfgLoad(cfgCmdRestore, argList);

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
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--tablespace-map=2=/2");
        strLstAddZ(argList, "--tablespace-map=ts2=/ts2");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreManifestMap(manifest), TablespaceMapError, "tablespace remapped by name 'ts2' and id 2 with different paths");

        // Remap one tablespace using the id and another with the name
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--tablespace-map=1=/1-2");
        strLstAddZ(argList, "--tablespace-map=ts2=/2-2");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap tablespaces");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/1"))->path, "/1-2", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/1"))->destination, "/1-2", "    check tablespace 1 link");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/2"))->path, "/2-2", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/2"))->destination, "/2-2", "    check tablespace 1 link");

        TEST_RESULT_LOG(
            "P00   INFO: map tablespace 'pg_tblspc/1' to '/1-2'\n"
            "P00   INFO: map tablespace 'pg_tblspc/2' to '/2-2'");

        // Remap a tablespace using just the id and map the rest with tablespace-map-all
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--tablespace-map=2=/2-3");
        strLstAddZ(argList, "--tablespace-map-all=/all");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap tablespaces");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/1"))->path, "/all/1", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/1"))->destination, "/all/1", "    check tablespace 1 link");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/2"))->path, "/2-3", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/2"))->destination, "/2-3", "    check tablespace 1 link");

        TEST_RESULT_LOG(
            "P00   INFO: map tablespace 'pg_tblspc/1' to '/all/1'\n"
            "P00   INFO: map tablespace 'pg_tblspc/2' to '/2-3'");

        // Remap all tablespaces with tablespace-map-all and update version to 9.2 to test warning
        manifest->pub.data.pgVersion = PG_VERSION_92;

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--tablespace-map-all=/all2");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap tablespaces");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/1"))->path, "/all2/1", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/1"))->destination, "/all2/1", "    check tablespace 1 link");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/2"))->path, "/all2/ts2", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/2"))->destination, "/all2/ts2", "    check tablespace 1 link");

        TEST_RESULT_LOG(
            "P00   INFO: map tablespace 'pg_tblspc/1' to '/all2/1'\n"
            "P00   INFO: map tablespace 'pg_tblspc/2' to '/all2/ts2'\n"
            "P00   WARN: update pg_tablespace.spclocation with new tablespace locations for PostgreSQL <= 9.2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remap links");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--link-map=bogus=bogus");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(restoreManifestMap(manifest), LinkMapError, "unable to remap invalid link 'bogus'");

        // Add some links
        manifestTargetAdd(
            manifest, &(ManifestTarget){
                .name = STRDEF("pg_data/pg_hba.conf"), .path = STRDEF("../conf"), .file = STRDEF("pg_hba.conf"),
                .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_hba.conf"), .destination = STRDEF("../conf/pg_hba.conf")});
        manifestTargetAdd(
            manifest, &(ManifestTarget){.name = STRDEF("pg_data/pg_wal"), .path = STRDEF("/wal"),  .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_wal"), .destination = STRDEF("/wal")});

        // Error on invalid file link path
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--link-map=pg_hba.conf=bogus");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreManifestMap(manifest), LinkMapError,
            "'bogus' is not long enough to be the destination for file link 'pg_hba.conf'");

        TEST_RESULT_LOG("P00   INFO: map link 'pg_hba.conf' to 'bogus'");

        // Remap both links
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--link-map=pg_hba.conf=../conf2/pg_hba2.conf");
        strLstAddZ(argList, "--link-map=pg_wal=/wal2");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap links");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->path, "../conf2", "    check link path");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->file, "pg_hba2.conf", "    check link file");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_hba.conf"))->destination, "../conf2/pg_hba2.conf", "    check link dest");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_wal"))->path, "/wal2", "    check link path");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_wal"))->destination, "/wal2", "    check link dest");

        TEST_RESULT_LOG(
            "P00   INFO: map link 'pg_hba.conf' to '../conf2/pg_hba2.conf'\n"
            "P00   INFO: map link 'pg_wal' to '/wal2'");

        // Leave all links as they are
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--link-all");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "leave links as they are");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->path, "../conf2", "    check link path");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->file, "pg_hba2.conf", "    check link file");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_hba.conf"))->destination, "../conf2/pg_hba2.conf", "    check link dest");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_wal"))->path, "/wal2", "    check link path");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_wal"))->destination, "/wal2", "    check link dest");

        // Remove all links
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remove all links");
        TEST_ERROR(
            manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf")), AssertError,
            "unable to find 'pg_data/pg_hba.conf' in manifest target list");
        TEST_ERROR(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_hba.conf")), AssertError,
            "unable to find 'pg_data/pg_hba.conf' in manifest link list");
        TEST_ERROR(
            manifestTargetFind(manifest, STRDEF("pg_data/pg_wal")), AssertError,
            "unable to find 'pg_data/pg_wal' in manifest target list");
        TEST_ERROR(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_wal")), AssertError,
            "unable to find 'pg_data/pg_wal' in manifest link list");

        TEST_RESULT_LOG(
            "P00   WARN: file link 'pg_hba.conf' will be restored as a file at the same location\n"
            "P00   WARN: contents of directory link 'pg_wal' will be restored in a directory at the same location");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreManifestOwner()"))
    {
        userInitInternal();

        const String *pgPath = strNewFmt("%s/pg", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("owner is not root and all ownership is good");

        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);
        TEST_RESULT_VOID(restoreManifestOwner(manifest), "check ownership");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("owner is not root but has no user name");

        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275I"), PG_VERSION_96, pgPath);

        userLocalData.groupName = NULL;
        userLocalData.userName = NULL;

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "check ownership");

        TEST_RESULT_LOG(
            "P00   WARN: unknown user '{[user]}' in backup manifest mapped to current user\n"
            "P00   WARN: unknown group '{[group]}' in backup manifest mapped to current group");

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

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "check ownership");

        TEST_RESULT_LOG(
            "P00   WARN: unknown user in backup manifest mapped to current user\n"
            "P00   WARN: unknown user 'path-user-bogus' in backup manifest mapped to current user\n"
            "P00   WARN: unknown group in backup manifest mapped to current group\n"
            "P00   WARN: unknown group 'file-group-bogus' in backup manifest mapped to current group\n"
            "P00   WARN: unknown group 'link-group-bogus' in backup manifest mapped to current group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("owner is root and ownership is good");

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-path=/repo");
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        harnessCfgLoad(cfgCmdRestore, argList);

        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);
        userLocalData.userRoot = true;

        storagePathCreateP(storagePgWrite(), NULL, .mode = 0700);

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "check ownership");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("owner is root and user is bad");

        manifestPathAdd(manifest, &path);

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "check ownership");

        TEST_RESULT_LOG("P00   WARN: unknown group in backup manifest mapped to '{[group]}'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("owner is root and group is bad");

        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);
        userLocalData.userRoot = true;

        manifestFileAdd(manifest, &file);
        manifestLinkAdd(manifest, &link);

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "check ownership");

        TEST_RESULT_LOG("P00   WARN: unknown user in backup manifest mapped to '{[user]}'");

        // -------------------------------------------------------------------------------------------------------------------------
#ifdef TEST_CONTAINER_REQUIRED
        TEST_TITLE("owner is root and ownership of pg_data is bad");

        manifestPathAdd(manifest, &path);
        manifestFileAdd(manifest, &file);

        TEST_SYSTEM_FMT("sudo chown 77777:77777 %s", strZ(pgPath));

        userLocalData.userName = STRDEF("root");
        userLocalData.groupName = STRDEF("root");

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "check ownership");

        TEST_RESULT_LOG(
            "P00   WARN: unknown user in backup manifest mapped to 'root'\n"
            "P00   WARN: unknown group in backup manifest mapped to 'root'");
#endif // TEST_CONTAINER_REQUIRED
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreClean*()"))
    {
        userInitInternal();

        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("restoreCleanOwnership() update to root (existing)");

        // Expect an error here since we can't really set ownership to root
        TEST_ERROR_FMT(
            restoreCleanOwnership(STR(testPath()), STRDEF("root"), STRDEF("root"), userId(), groupId(), false), FileOwnerError,
            "unable to set ownership for '%s': [1] Operation not permitted", testPath());

        TEST_RESULT_LOG("P00 DETAIL: update ownership for '{[path]}'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("restoreCleanOwnership() update to bogus (new)");

        // Will succeed because bogus will be remapped to the current user/group
        restoreCleanOwnership(STR(testPath()), STRDEF("bogus"), STRDEF("bogus"), 0, 0, true);

        // Test again with only group for coverage
        restoreCleanOwnership(STR(testPath()), STRDEF("bogus"), STRDEF("bogus"), userId(), 0, true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("directory with bad permissions/mode");

        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());
        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        harnessCfgLoad(cfgCmdRestore, argList);

        storagePathCreateP(storagePgWrite(), NULL, .mode = 0600);

        userLocalData.userId = getuid() + 1;

        TEST_ERROR_FMT(
            restoreCleanBuild(manifest), PathOpenError, "unable to restore to path '%s/pg' not owned by current user", testPath());

        TEST_RESULT_LOG("P00 DETAIL: check '{[path]}/pg' exists");

        userLocalData.userRoot = true;

        TEST_ERROR_FMT(
            restoreCleanBuild(manifest), PathOpenError, "unable to restore to path '%s/pg' without rwx permissions", testPath());

        TEST_RESULT_LOG("P00 DETAIL: check '{[path]}/pg' exists");

        userInitInternal();

        TEST_ERROR_FMT(
            restoreCleanBuild(manifest), PathOpenError, "unable to restore to path '%s/pg' without rwx permissions", testPath());

        TEST_RESULT_LOG("P00 DETAIL: check '{[path]}/pg' exists");

        storagePathRemoveP(storagePgWrite(), NULL);
        storagePathCreateP(storagePgWrite(), NULL, .mode = 0700);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fail on restore with directory not empty");

        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR), NULL);

        TEST_ERROR_FMT(
            restoreCleanBuild(manifest), PathNotEmptyError,
            "unable to restore to path '%s/pg' because it contains files\n"
                "HINT: try using --delta if this is what you intended.",
            testPath());

        TEST_RESULT_LOG("P00 DETAIL: check '{[path]}/pg' exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when all directories empty");

        storageRemoveP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR);

        manifestTargetAdd(
            manifest, &(ManifestTarget){
                .name = STRDEF("pg_data/pg_hba.conf"), .path = STRDEF("../conf"), .file = STRDEF("pg_hba.conf"),
                .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_hba.conf"), .destination = STRDEF("../conf/pg_hba.conf")});

        storagePathCreateP(storageTest, STRDEF("conf"), .mode = 0700);

        TEST_RESULT_VOID(restoreCleanBuild(manifest), "restore");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/conf' exists\n"
            "P00 DETAIL: create symlink '{[path]}/pg/pg_hba.conf' to '../conf/pg_hba.conf'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when linked file already exists without delta");

        storageRemoveP(storagePgWrite(), STRDEF("pg_hba.conf"));
        storagePutP(storageNewWriteP(storagePgWrite(), STRDEF("../conf/pg_hba.conf")), NULL);

        TEST_ERROR_FMT(
            restoreCleanBuild(manifest), FileExistsError,
            "unable to restore file '%s/conf/pg_hba.conf' because it already exists\n"
            "HINT: try using --delta if this is what you intended.",
            testPath());

        TEST_RESULT_LOG(
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/conf' exists");

        storageRemoveP(storagePgWrite(), STRDEF("../conf/pg_hba.conf"), .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when all directories empty and ignore recovery.conf");

        TEST_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--type=preserve");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreCleanBuild(manifest), "restore");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/conf' exists\n"
            "P00 DETAIL: create symlink '{[path]}/pg/pg_hba.conf' to '../conf/pg_hba.conf'");

        TEST_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR), NULL);
        TEST_RESULT_VOID(restoreCleanBuild(manifest), "normal restore ignore recovery.conf");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/conf' exists\n"
            "P00 DETAIL: create symlink '{[path]}/pg/pg_hba.conf' to '../conf/pg_hba.conf'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when all directories empty and PG12 and preserve but no recovery files");

        TEST_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        manifest->pub.data.pgVersion = PG_VERSION_12;

        TEST_RESULT_VOID(restoreCleanBuild(manifest), "restore");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/conf' exists\n"
            "P00 DETAIL: create symlink '{[path]}/pg/pg_hba.conf' to '../conf/pg_hba.conf'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when all directories empty and ignore PG12 recovery files");

        TEST_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        manifestFileAdd(manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_POSTGRESQLAUTOCONF)});

        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF_STR), NULL);
        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_RECOVERYSIGNAL_STR), NULL);
        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_STANDBYSIGNAL_STR), NULL);

        TEST_RESULT_VOID(restoreCleanBuild(manifest), "restore");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/conf' exists\n"
            "P00 DETAIL: skip 'postgresql.auto.conf' -- recovery type is preserve\n"
            "P00 DETAIL: create symlink '{[path]}/pg/pg_hba.conf' to '../conf/pg_hba.conf'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("succeed when all directories empty and PG12");

        TEST_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_VOID(restoreCleanBuild(manifest), "restore");

        TEST_RESULT_LOG(
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/conf' exists\n"
            "P00 DETAIL: create symlink '{[path]}/pg/pg_hba.conf' to '../conf/pg_hba.conf'");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreSelectiveExpression()"))
    {
        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no valid databases");

        StringList *argListClean = strLstNew();
        strLstAddZ(argListClean, "--stanza=test1");
        strLstAddZ(argListClean, "--repo1-path=/repo");
        strLstAddZ(argListClean, "--pg1-path=/pg");

        StringList *argList = strLstDup(argListClean);
        strLstAddZ(argList, "--db-include=" UTF8_DB_NAME);
        harnessCfgLoad(cfgCmdRestore, argList);

        Manifest *manifest = NULL;

        MEM_CONTEXT_NEW_BEGIN("Manifest")
        {
            manifest = manifestNewInternal();
            manifest->pub.data.pgVersion = PG_VERSION_84;

            manifestTargetAdd(manifest, &(ManifestTarget){.name = MANIFEST_TARGET_PGDATA_STR, .path = STRDEF("/pg")});
            manifestFileAdd(manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_NEW_END();

        TEST_ERROR(
            restoreSelectiveExpression(manifest), FormatError,
            "no databases found for selective restore\n"
            "HINT: is this a valid cluster?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("database id is missing on disk");

        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            // Give non-systemId to postgres db
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("postgres"), .id = 16385, .lastSystemId = 12168});
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("template0"), .id = 12168, .lastSystemId = 12168});
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("template1"), .id = 1, .lastSystemId = 12168});
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("user-made-system-db"), .id = 16380, .lastSystemId = 12168});
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF(UTF8_DB_NAME), .id = 16384, .lastSystemId = 12168});
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
        strLstAddZ(argList, "--db-exclude=" UTF8_DB_NAME);
        harnessCfgLoad(cfgCmdRestore, argList);

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
        strLstAddZ(argList, "--db-include=" UTF8_DB_NAME);
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_STR(restoreSelectiveExpression(manifest), NULL, "all databases selected");

        TEST_RESULT_LOG(
            "P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385)\n"
            "P00   INFO: nothing to filter - all user databases have been selected");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on system database selected");

        argList = strLstDup(argListClean);
        strLstAddZ(argList, "--db-include=1");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreSelectiveExpression(manifest), DbInvalidError,
            "system databases (template0, postgres, etc.) are included by default");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on system database with non-systemId selected");

        argList = strLstDup(argListClean);
        strLstAddZ(argList, "--db-include=16385");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreSelectiveExpression(manifest), DbInvalidError,
            "system databases (template0, postgres, etc.) are included by default");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on system database with non-systemId selected, by name");

        argList = strLstDup(argListClean);
        strLstAddZ(argList, "--db-include=postgres");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreSelectiveExpression(manifest), DbInvalidError,
            "system databases (template0, postgres, etc.) are included by default");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing database selected");

        argList = strLstDup(argListClean);
        strLstAddZ(argList, "--db-include=7777777");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(restoreSelectiveExpression(manifest), DbMissingError, "database to include '7777777' does not exist");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("select database by id");

        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("test2"), .id = 32768, .lastSystemId = 12168});
            manifestFileAdd(
                manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/32768/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_END();

        argList = strLstDup(argListClean);
        strLstAddZ(argList, "--db-include=16384");
        harnessCfgLoad(cfgCmdRestore, argList);

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
            restoreSelectiveExpression(manifest), "(^pg_data/base/32768/)|(^pg_tblspc/16387/32768/)", "check expression");

        TEST_RESULT_LOG(
            "P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768)\n"
            "P00 DETAIL: databases excluded (zeroed) from selective restore (32768)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("one database selected with tablespace id");

        manifest->pub.data.pgVersion = PG_VERSION_94;
        manifest->pub.data.pgCatalogVersion = hrnPgCatalogVersion(PG_VERSION_94);

        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("test3"), .id = 65536, .lastSystemId = 12168});
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
        strLstAddZ(argList, "--db-exclude=16384");
        harnessCfgLoad(cfgCmdRestore, argList);

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
        strLstAddZ(argList, "--db-exclude=" UTF8_DB_NAME);
        harnessCfgLoad(cfgCmdRestore, argList);

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
        strLstAddZ(argList, "--db-exclude=16385");
        harnessCfgLoad(cfgCmdRestore, argList);

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
        strLstAddZ(argList, "--db-exclude=7777777");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(restoreSelectiveExpression(manifest), DbMissingError, "database to exclude '7777777' does not exist");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768, 65536)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on combining include and exclude options");

        argList = strLstDup(argListClean);
        strLstAddZ(argList, "--db-include=test2");
        strLstAddZ(argList, "--db-exclude=test2");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(restoreSelectiveExpression(manifest), DbInvalidError, "database to include '32768' is in the exclude list");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 12168, 16380, 16381, 16384, 16385, 32768, 65536)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("combine include and exclude options");

        argList = strLstDup(argListClean);
        strLstAddZ(argList, "--db-include=16384");
        strLstAddZ(argList, "--db-exclude=1");
        strLstAddZ(argList, "--db-exclude=16385");
        strLstAddZ(argList, "--db-exclude=32768");  // user databases excluded will be silently ignored
        harnessCfgLoad(cfgCmdRestore, argList);

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
        strLstAddZ(argBaseList, "--stanza=test1");
        strLstAddZ(argBaseList, "--repo1-path=/repo");
        strLstAddZ(argBaseList, "--pg1-path=/pg");

        const String *restoreLabel = STRDEF("LABEL");
        #define RECOVERY_SETTING_HEADER                             "# Recovery settings generated by pgBackRest restore on LABEL\n"

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("user-specified options");

        StringList *argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--recovery-option=a-setting=a");
        strLstAddZ(argList, "--recovery-option=b_setting=b");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            hrnReplaceKey(
                RECOVERY_SETTING_HEADER
                "a_setting = 'a'\n"
                "b_setting = 'b'\n"
                "restore_command = '{[project-exe]} --lock-path={[path-data]}/lock --log-path={[path-data]} --pg1-path=/pg"
                    " --repo1-path=/repo --stanza=test1 archive-get %f \"%p\"'\n"),
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("override restore_command");

        strLstAddZ(argBaseList, "--recovery-option=restore-command=my_restore_command");
        argList = strLstDup(argBaseList);
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery target immediate");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--type=immediate");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "recovery_target = 'immediate'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery target time with timeline");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--type=time");
        strLstAddZ(argList, "--target=TIME");
        strLstAddZ(argList, "--target-timeline=3");
        harnessCfgLoad(cfgCmdRestore, argList);

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
        strLstAddZ(argList, "--type=time");
        strLstAddZ(argList, "--target=TIME");
        strLstAddZ(argList, "--target-exclusive");
        harnessCfgLoad(cfgCmdRestore, argList);

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
        strLstAddZ(argList, "--type=name");
        strLstAddZ(argList, "--target=NAME");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "recovery_target_name = 'NAME'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery target action = shutdown");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--type=immediate");
        strLstAddZ(argList, "--target-action=shutdown");
        harnessCfgLoad(cfgCmdRestore, argList);

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
        strLstAddZ(argList, "--type=immediate");
        strLstAddZ(argList, "--target-action=promote");
        harnessCfgLoad(cfgCmdRestore, argList);

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
        strLstAddZ(argList, "--type=standby");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel),
            RECOVERY_SETTING_HEADER
            "restore_command = 'my_restore_command'\n"
            "standby_mode = 'on'\n",
            "check recovery options");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery type = standby with timeline");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--type=standby");
        strLstAddZ(argList, "--target-timeline=current");
        harnessCfgLoad(cfgCmdRestore, argList);

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
        strLstAddZ(argList, "--archive-mode=off");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreRecoveryConf(PG_VERSION_94, restoreLabel), OptionInvalidError,
            "option 'archive-mode' is not supported on PostgreSQL < 12\n"
                "HINT: 'archive_mode' should be manually set to 'off' in postgresql.conf.");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("recovery type = standby with recovery GUCs and archive-mode=off");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--type=standby");
        strLstAddZ(argList, "--archive-mode=off");
        harnessCfgLoad(cfgCmdRestore, argList);

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
        const String *pgPath = strNewFmt("%s/pg", testPath());
        storagePathCreateP(storageTest, pgPath, .mode = 0700);

        const String *restoreLabel = STRDEF("LABEL");
        #define RECOVERY_SETTING_PREFIX                             "# Removed by pgBackRest restore on LABEL # "

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when standby_mode setting is present");

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-path=/repo");
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--type=default");
        strLstAddZ(argList, "--recovery-option=standby-mode=on");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(
            restoreRecoveryWriteAutoConf(PG_VERSION_12, restoreLabel), OptionInvalidError,
            "'standby_mode' setting is not valid for PostgreSQL >= 12\n"
            "HINT: use --type=standby instead of --recovery-option=standby_mode=on.");

        TEST_RESULT_LOG("P00   WARN: postgresql.auto.conf does not exist -- creating to contain recovery settings");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PG12 restore missing postgresql.auto.conf");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-path=/repo");
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--type=none");
        harnessCfgLoad(cfgCmdRestore, argList);

        restoreRecoveryWriteAutoConf(PG_VERSION_12, restoreLabel);

        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storagePg(), PG_FILE_POSTGRESQLAUTOCONF_STR))),
            "", "check postgresql.auto.conf");
        TEST_RESULT_BOOL(storageExistsP(storagePg(), PG_FILE_RECOVERYSIGNAL_STR), true, "recovery.signal exists");
        TEST_RESULT_BOOL(storageExistsP(storagePg(), PG_FILE_STANDBYSIGNAL_STR), false, "standby.signal missing");

        TEST_RESULT_LOG(
            "P00   WARN: postgresql.auto.conf does not exist -- creating to contain recovery settings\n"
            "P00   INFO: write {[path]}/pg/postgresql.auto.conf");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PG12 restore type none");

        TEST_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        storagePutP(
            storageNewWriteP(storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF_STR),
            BUFSTRDEF(
                "# DO NOT MODIFY\n"
                "\t recovery_target_action='promote'\n\n"));

        restoreRecoveryWriteAutoConf(PG_VERSION_12, restoreLabel);

        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storagePg(), PG_FILE_POSTGRESQLAUTOCONF_STR))),
            "# DO NOT MODIFY\n"
            RECOVERY_SETTING_PREFIX "\t recovery_target_action='promote'\n\n",
            "check postgresql.auto.conf");
        TEST_RESULT_BOOL(storageExistsP(storagePg(), PG_FILE_RECOVERYSIGNAL_STR), true, "recovery.signal exists");
        TEST_RESULT_BOOL(storageExistsP(storagePg(), PG_FILE_STANDBYSIGNAL_STR), false, "standby.signal missing");

        TEST_RESULT_LOG("P00   INFO: write updated {[path]}/pg/postgresql.auto.conf");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PG12 restore type standby and remove existing recovery settings");

        TEST_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        storagePutP(
            storageNewWriteP(storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF_STR),
            BUFSTRDEF(
                "# DO NOT MODIFY\n"
                "recovery_target_name\t='name'\n"
                "recovery_target_inclusive = false\n"));

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-path=/repo");
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--recovery-option=restore-command=my_restore_command");
        strLstAddZ(argList, "--type=standby");
        harnessCfgLoad(cfgCmdRestore, argList);

        restoreRecoveryWriteAutoConf(PG_VERSION_12, restoreLabel);

        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storagePg(), PG_FILE_POSTGRESQLAUTOCONF_STR))),
            "# DO NOT MODIFY\n"
                RECOVERY_SETTING_PREFIX "recovery_target_name\t='name'\n"
                RECOVERY_SETTING_PREFIX "recovery_target_inclusive = false\n"
                "\n"
                RECOVERY_SETTING_HEADER
                "restore_command = 'my_restore_command'\n",
            "check postgresql.auto.conf");
        TEST_RESULT_BOOL(storageExistsP(storagePg(), PG_FILE_RECOVERYSIGNAL_STR), false, "recovery.signal exists");
        TEST_RESULT_BOOL(storageExistsP(storagePg(), PG_FILE_STANDBYSIGNAL_STR), true, "standby.signal missing");

        TEST_RESULT_LOG("P00   INFO: write updated {[path]}/pg/postgresql.auto.conf");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PG12 restore type preserve");

        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F"), PG_VERSION_12, STRDEF("/pg"));

        TEST_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF_STR), BUFSTRDEF("# DO NOT MODIFY\n"));
        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_STANDBYSIGNAL_STR), NULL);

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-path=/repo");
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--type=preserve");
        harnessCfgLoad(cfgCmdRestore, argList);

        restoreRecoveryWrite(manifest);

        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storagePg(), PG_FILE_POSTGRESQLAUTOCONF_STR))), "# DO NOT MODIFY\n",
            "check postgresql.auto.conf");
        TEST_RESULT_BOOL(storageExistsP(storagePg(), PG_FILE_RECOVERYSIGNAL_STR), false, "recovery.signal missing");
        TEST_RESULT_BOOL(storageExistsP(storagePg(), PG_FILE_STANDBYSIGNAL_STR), true, "standby.signal exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PG12 restore type default");

        TEST_SYSTEM_FMT("rm -rf %s/*", strZ(pgPath));

        storagePutP(
            storageNewWriteP(storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF_STR),
            BUFSTRDEF("# DO NOT MODIFY\n"));

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-path=/repo");
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        harnessCfgLoad(cfgCmdRestore, argList);

        restoreRecoveryWrite(manifest);

        TEST_RESULT_BOOL(
            bufEq(storageGetP(storageNewReadP(storagePg(), PG_FILE_POSTGRESQLAUTOCONF_STR)), BUFSTRDEF("# DO NOT MODIFY\n")),
            false, "check postgresql.auto.conf has changed");
        TEST_RESULT_BOOL(storageExistsP(storagePg(), PG_FILE_RECOVERYSIGNAL_STR), true, "recovery.signal exists");
        TEST_RESULT_BOOL(storageExistsP(storagePg(), PG_FILE_STANDBYSIGNAL_STR), false, "standby.signal missing");

        TEST_RESULT_LOG("P00   INFO: write updated {[path]}/pg/postgresql.auto.conf");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdRestore()"))
    {
        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());
        const String *repoPathEncrpyt = strNewFmt("%s/repo-encrypt", testPath());

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
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--pg1-host=pg1");
        harnessCfgLoad(cfgCmdRestore, argList);

        TEST_ERROR(cmdRestore(), HostInvalidError, "restore command must be run on the PostgreSQL host");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full restore without delta, multi-repo");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 1, repoPath);
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPathEncrpyt);
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--set=20161219-212741F");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);
        harnessCfgLoad(cfgCmdRestore, argList);

        #define TEST_LABEL                                          "20161219-212741F"
        #define TEST_PGDATA                                         MANIFEST_TARGET_PGDATA "/"
        #define TEST_REPO_PATH                                      STORAGE_REPO_BACKUP "/" TEST_LABEL "/" TEST_PGDATA

        Manifest *manifest = NULL;

        MEM_CONTEXT_NEW_BEGIN("Manifest")
        {
            manifest = manifestNewInternal();
            manifest->pub.info = infoNew(NULL);
            manifest->pub.data.backupLabel = strNew(TEST_LABEL);
            manifest->pub.data.pgVersion = PG_VERSION_84;
            manifest->pub.data.backupType = backupTypeFull;
            manifest->pub.data.backupTimestampCopyStart = 1482182861; // So file timestamps should be less than this

            // Data directory
            manifestTargetAdd(manifest, &(ManifestTarget){.name = MANIFEST_TARGET_PGDATA_STR, .path = pgPath});
            manifestPathAdd(
                manifest,
                &(ManifestPath){.name = MANIFEST_TARGET_PGDATA_STR, .mode = 0700, .group = groupName(), .user = userName()});
            storagePathCreateP(storagePgWrite(), NULL);

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
                    .checksumSha1 = "797e375b924134687cbf9eacd37a4355f3d825e4"});
            storagePutP(
                storageNewWriteP(
                    storageRepoIdxWrite(0), STRDEF(TEST_REPO_PATH PG_FILE_PGVERSION)), BUFSTRDEF(PG_VERSION_84_STR "\n"));

            // Store the file also to the encrypted repo
            HRN_STORAGE_PUT(
                storageRepoIdxWrite(1), TEST_REPO_PATH PG_FILE_PGVERSION, BUFSTRDEF(PG_VERSION_84_STR "\n"),
                .cipherType = cipherTypeAes256Cbc, .cipherPass = TEST_CIPHER_PASS_ARCHIVE);

            // pg_tblspc/1
            manifestTargetAdd(
                manifest, &(ManifestTarget){
                    .type = manifestTargetTypeLink, .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1"),
                    .path = strNewFmt("%s/ts/1", testPath()), .tablespaceId = 1, .tablespaceName = STRDEF("ts1")});
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
            manifestLinkAdd(
                manifest, &(ManifestLink){
                    .name = STRDEF(MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC "/1"),
                    .destination = strNewFmt("%s/ts/1", testPath()), .group = groupName(), .user = userName()});

            // pg_tblspc/1/16384 path
            manifestPathAdd(
                manifest, &(ManifestPath){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1/16384"), .mode = 0700,
                    .group = groupName(), .user = userName()});

            // Always sort
            lstSort(manifest->pub.targetList, sortOrderAsc);
            lstSort(manifest->pub.fileList, sortOrderAsc);
            lstSort(manifest->pub.linkList, sortOrderAsc);
            lstSort(manifest->pub.pathList, sortOrderAsc);
        }
        MEM_CONTEXT_NEW_END();

        manifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteP(storageRepoIdxWrite(0),
                strNew(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE))));

        // Read the manifest, set a cipher passphrase and store it to the encrypted repo
        Manifest *manifestEncrypted = manifestLoadFile(
            storageRepoIdxWrite(0), strNew(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE), cipherTypeNone, NULL);
        manifestCipherSubPassSet(manifestEncrypted, STRDEF(TEST_CIPHER_PASS_ARCHIVE));

        // Open file for write
        IoWrite *write = storageWriteIo(
            storageNewWriteP(
                storageRepoIdxWrite(1),
                strNew(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE)));

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
            strZ(strNewFmt(
            "P00   WARN: repo1: [FileMissingError] unable to load info file"
            " '%s/repo/backup/test1/backup.info' or '%s/repo/backup/test1/backup.info.copy':\n"
            "            FileMissingError: unable to open missing file '%s/repo/backup/test1/backup.info' for read\n"
            "            FileMissingError: unable to open missing file '%s/repo/backup/test1/backup.info.copy' for read\n"
            "            HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "            HINT: has a stanza-create been performed?\n"
            "P00   INFO: repo2: restore backup set 20161219-212741F\n"
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/ts/1' exists\n"
            "P00 DETAIL: update mode for '{[path]}/pg' to 0700\n"
            "P00 DETAIL: create path '{[path]}/pg/global'\n"
            "P00 DETAIL: create path '{[path]}/pg/pg_tblspc'\n"
            "P00 DETAIL: create symlink '{[path]}/pg/pg_tblspc/1' to '{[path]}/ts/1'\n"
            "P00 DETAIL: create path '{[path]}/pg/pg_tblspc/1/16384'\n"
            "P01   INFO: restore file {[path]}/pg/PG_VERSION (4B, 100%%) checksum 797e375b924134687cbf9eacd37a4355f3d825e4\n"
            "P00   INFO: write {[path]}/pg/recovery.conf\n"
            "P00 DETAIL: sync path '{[path]}/pg'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc/1'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc/1/16384'\n"
            "P00   WARN: backup does not contain 'global/pg_control' -- cluster will not start\n"
            "P00 DETAIL: sync path '{[path]}/pg/global'", testPath(), testPath(), testPath(), testPath())));

        // Remove recovery.conf before file comparison since it will have a new timestamp.  Make sure it existed, though.
        storageRemoveP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR, .errorOnMissing = true);

        testRestoreCompare(
            storagePg(), NULL, manifest,
            ". {path}\n"
            "PG_VERSION {file, s=4, t=1482182860}\n"
            "global {path}\n"
            "pg_tblspc {path}\n"
            "pg_tblspc/1 {link, d={[path]}/ts/1}\n");

        testRestoreCompare(
            storagePg(), STRDEF("pg_tblspc/1"), manifest,
            ". {link, d={[path]}/ts/1}\n"
            "16384 {path}\n");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full restore with delta force");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPathEncrpyt);
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--type=preserve");
        strLstAddZ(argList, "--set=20161219-212741F");
        strLstAddZ(argList, "--" CFGOPT_DELTA);
        strLstAddZ(argList, "--force");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);
        harnessCfgLoad(cfgCmdRestore, argList);

        // Store backup.info to repo1 - repo1 will be selected because of the priority order
        HRN_INFO_PUT(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, TEST_RESTORE_BACKUP_INFO "\n" TEST_RESTORE_BACKUP_INFO_DB);

        // Make sure existing backup.manifest file is ignored
        storagePutP(storageNewWriteP(storagePgWrite(), BACKUP_MANIFEST_FILE_STR), NULL);

        // Add a bogus file that will be removed
        storagePutP(storageNewWriteP(storagePgWrite(), STRDEF("bogus-file")), NULL);

        // Add a special file that will be removed
        TEST_SYSTEM_FMT("mkfifo %s/pipe", strZ(pgPath));

        // Overwrite PG_VERSION with bogus content that will not be detected by delta force because the time and size are the same
        storagePutP(
            storageNewWriteP(storagePgWrite(), STRDEF(PG_FILE_PGVERSION), .modeFile = 0600, .timeModified = 1482182860),
            BUFSTRDEF("BOG\n"));

        // Change destination of tablespace link
        storageRemoveP(storagePgWrite(), STRDEF("pg_tblspc/1"), .errorOnMissing = true);
        THROW_ON_SYS_ERROR(
            symlink("/bogus", strZ(strNewFmt("%s/pg_tblspc/1", strZ(pgPath)))) == -1, FileOpenError,
            "unable to create symlink");

        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            // tablespace_map (will be ignored during restore)
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA PG_FILE_TABLESPACEMAP), .size = 0, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .checksumSha1 = HASH_TYPE_SHA1_ZERO});
            storagePutP(storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH PG_FILE_TABLESPACEMAP)), NULL);

            // pg_tblspc/1/16384/PG_VERSION
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1/16384/" PG_FILE_PGVERSION), .size = 4,
                    .timestamp = 1482182860, .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "797e375b924134687cbf9eacd37a4355f3d825e4"});
            storagePutP(
                storageNewWriteP(
                    storageRepoWrite(),
                    STRDEF(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" MANIFEST_TARGET_PGTBLSPC "/1/16384/" PG_FILE_PGVERSION)),
                BUFSTRDEF(PG_VERSION_84_STR "\n"));

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
                strNew(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE))));

        #undef TEST_LABEL
        #undef TEST_PGDATA
        #undef TEST_REPO_PATH

        cmdRestore();

        TEST_RESULT_LOG(
            "P00   INFO: repo1: restore backup set 20161219-212741F\n"
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/ts/1' exists\n"
            "P00   INFO: remove invalid files/links/paths from '{[path]}/pg'\n"
            "P00 DETAIL: remove invalid file '{[path]}/pg/bogus-file'\n"
            "P00 DETAIL: remove link '{[path]}/pg/pg_tblspc/1' because destination changed\n"
            "P00 DETAIL: remove special file '{[path]}/pg/pipe'\n"
            "P00   INFO: remove invalid files/links/paths from '{[path]}/ts/1'\n"
            "P00 DETAIL: create symlink '{[path]}/pg/pg_tblspc/1' to '{[path]}/ts/1'\n"
            "P01 DETAIL: restore file {[path]}/pg/PG_VERSION - exists and matches size 4 and modification time 1482182860 (4B, 50%)"
                " checksum 797e375b924134687cbf9eacd37a4355f3d825e4\n"
            "P01   INFO: restore file {[path]}/pg/tablespace_map (0B, 50%)\n"
            "P01   INFO: restore file {[path]}/pg/pg_tblspc/1/16384/PG_VERSION (4B, 100%)"
                " checksum 797e375b924134687cbf9eacd37a4355f3d825e4\n"
            "P00   WARN: recovery type is preserve but recovery file does not exist at '{[path]}/pg/recovery.conf'\n"
            "P00 DETAIL: sync path '{[path]}/pg'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc/1'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc/1/16384'\n"
            "P00   WARN: backup does not contain 'global/pg_control' -- cluster will not start\n"
            "P00 DETAIL: sync path '{[path]}/pg/global'");

        testRestoreCompare(
            storagePg(), NULL, manifest,
            ". {path}\n"
            "PG_VERSION {file, s=4, t=1482182860}\n"
            "global {path}\n"
            "pg_tblspc {path}\n"
            "pg_tblspc/1 {link, d={[path]}/ts/1}\n"
            "tablespace_map {file, s=0, t=1482182860}\n");

        testRestoreCompare(
            storagePg(), STRDEF("pg_tblspc/1"), manifest,
            ". {link, d={[path]}/ts/1}\n"
            "16384 {path}\n"
            "16384/PG_VERSION {file, s=4, t=1482182860}\n");

        // PG_VERSION was not restored because delta force relies on time and size which were the same in the manifest and on disk
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storagePg(), STRDEF(PG_FILE_PGVERSION)))), "BOG\n",
            "check PG_VERSION was not restored");

        // Cleanup
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);
        storagePathRemoveP(storageRepoIdxWrite(1), NULL, .recurse = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full restore with force");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
        hrnCfgArgRaw(argList, cfgOptPgPath, pgPath);
        hrnCfgArgRawStrId(argList, cfgOptType, CFGOPTVAL_TYPE_PRESERVE);
        strLstAddZ(argList, "--" CFGOPT_SET "=20161219-212741F");
        strLstAddZ(argList, "--" CFGOPT_FORCE);
        harnessCfgLoad(cfgCmdRestore, argList);

        cmdRestore();

        TEST_RESULT_LOG(
            "P00   INFO: repo1: restore backup set 20161219-212741F\n"
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/ts/1' exists\n"
            "P00   INFO: remove invalid files/links/paths from '{[path]}/pg'\n"
            "P00   INFO: remove invalid files/links/paths from '{[path]}/ts/1'\n"
            "P01   INFO: restore file {[path]}/pg/PG_VERSION (4B, 50%) checksum 797e375b924134687cbf9eacd37a4355f3d825e4\n"
            "P01   INFO: restore file {[path]}/pg/tablespace_map (0B, 50%)\n"
            "P01   INFO: restore file {[path]}/pg/pg_tblspc/1/16384/PG_VERSION (4B, 100%)"
                " checksum 797e375b924134687cbf9eacd37a4355f3d825e4\n"
            "P00   WARN: recovery type is preserve but recovery file does not exist at '{[path]}/pg/recovery.conf'\n"
            "P00 DETAIL: sync path '{[path]}/pg'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc/1'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc/1/16384'\n"
            "P00   WARN: backup does not contain 'global/pg_control' -- cluster will not start\n"
            "P00 DETAIL: sync path '{[path]}/pg/global'");

        testRestoreCompare(
            storagePg(), NULL, manifest,
            ". {path}\n"
            "PG_VERSION {file, s=4, t=1482182860}\n"
            "global {path}\n"
            "pg_tblspc {path}\n"
            "pg_tblspc/1 {link, d={[path]}/ts/1}\n"
            "tablespace_map {file, s=0, t=1482182860}\n");

        testRestoreCompare(
            storagePg(), STRDEF("pg_tblspc/1"), manifest,
            ". {link, d={[path]}/ts/1}\n"
            "16384 {path}\n"
            "16384/PG_VERSION {file, s=4, t=1482182860}\n");

        // PG_VERSION was restored by the force option
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storagePg(), STRDEF(PG_FILE_PGVERSION)))), PG_VERSION_84_STR "\n",
            "check PG_VERSION was restored");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("incremental delta selective restore");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strZ(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        strLstAddZ(argList, "--delta");
        strLstAddZ(argList, "--type=none");
        strLstAddZ(argList, "--link-map=pg_wal=../wal");
        strLstAddZ(argList, "--link-map=postgresql.conf=../config/postgresql.conf");
        strLstAddZ(argList, "--link-map=pg_hba.conf=../config/pg_hba.conf");
        harnessCfgLoad(cfgCmdRestore, argList);

        #define TEST_LABEL                                          "20161219-212741F_20161219-212918I"
        #define TEST_PGDATA                                         MANIFEST_TARGET_PGDATA "/"
        #define TEST_REPO_PATH                                      STORAGE_REPO_BACKUP "/" TEST_LABEL "/" TEST_PGDATA

        MEM_CONTEXT_NEW_BEGIN("Manifest")
        {
            manifest = manifestNewInternal();
            manifest->pub.info = infoNew(NULL);
            manifest->pub.data.backupLabel = strNew(TEST_LABEL);
            manifest->pub.data.pgVersion = PG_VERSION_10;
            manifest->pub.data.pgCatalogVersion = hrnPgCatalogVersion(PG_VERSION_10);
            manifest->pub.data.backupType = backupTypeFull;
            manifest->pub.data.backupTimestampCopyStart = 1482182861; // So file timestamps should be less than this

            // Data directory
            manifestTargetAdd(manifest, &(ManifestTarget){.name = MANIFEST_TARGET_PGDATA_STR, .path = pgPath});
            manifestPathAdd(
                manifest,
                &(ManifestPath){.name = MANIFEST_TARGET_PGDATA_STR, .mode = 0700, .group = groupName(), .user = userName()});
            storagePathCreateP(storagePgWrite(), NULL);

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
            storagePutP(
                storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)), fileBuffer);

            // global/999
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA PG_PATH_GLOBAL "/999"), .size = 0, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = HASH_TYPE_SHA1_ZERO, .reference = STRDEF(TEST_LABEL)});
            storagePutP(storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH PG_PATH_GLOBAL "/999")), NULL);

            // PG_VERSION
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA PG_FILE_PGVERSION), .size = 4, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "8dbabb96e032b8d9f1993c0e4b9141e71ade01a1"});
            storagePutP(
                storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH PG_FILE_PGVERSION)), BUFSTRDEF(PG_VERSION_94_STR "\n"));

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

            // base/1/PG_VERSION
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA "base/1/" PG_FILE_PGVERSION), .size = 4, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(),
                    .checksumSha1 = "8dbabb96e032b8d9f1993c0e4b9141e71ade01a1"});
            storagePutP(
                storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH "base/1/" PG_FILE_PGVERSION)),
                BUFSTRDEF(PG_VERSION_94_STR "\n"));

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
            storagePutP(storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH "base/1/2")), fileBuffer);

            // system db name
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("template1"), .id = 1, .lastSystemId = 12168});

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
            storagePutP(
                storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH "base/16384/" PG_FILE_PGVERSION)),
                BUFSTRDEF(PG_VERSION_94_STR "\n"));

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
            storagePutP(storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH "base/16384/16385")), fileBuffer);

            // base/32768 directory
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("test2"), .id = 32768, .lastSystemId = 12168});
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
            storagePutP(
                storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH "base/32768/" PG_FILE_PGVERSION)),
                BUFSTRDEF(PG_VERSION_94_STR "\n"));

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
            storagePutP(storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH "base/32768/32769")), fileBuffer);

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
            storagePutP(
                storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH "postgresql.conf")), BUFSTRDEF("POSTGRESQL.CONF"));

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
            storagePutP(
                storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH "pg_hba.conf")), BUFSTRDEF("PG_HBA.CONF"));

            // tablespace_map (will be ignored during restore)
            manifestFileAdd(
                manifest,
                &(ManifestFile){
                    .name = STRDEF(TEST_PGDATA PG_FILE_TABLESPACEMAP), .size = 0, .timestamp = 1482182860,
                    .mode = 0600, .group = groupName(), .user = userName(), .checksumSha1 = HASH_TYPE_SHA1_ZERO});
            storagePutP(storageNewWriteP(storageRepoWrite(), STRDEF(TEST_REPO_PATH PG_FILE_TABLESPACEMAP)), NULL);

            // Path link to pg_wal
            name = STRDEF(MANIFEST_TARGET_PGDATA "/pg_wal");
            const String *destination = STRDEF("../wal");

            manifestTargetAdd(manifest, &(ManifestTarget){.type = manifestTargetTypeLink, .name = name, .path = destination});
            manifestPathAdd(manifest, &(ManifestPath){.name = name, .mode = 0700, .group = groupName(), .user = userName()});
            manifestLinkAdd(
                manifest, &(ManifestLink){.name = name, .destination = destination, .group = groupName(), .user = userName()});
            THROW_ON_SYS_ERROR(
                symlink("../wal", strZ(strNewFmt("%s/pg_wal", strZ(pgPath)))) == -1, FileOpenError,
                "unable to create symlink");

            // pg_tblspc/1
            manifestTargetAdd(
                manifest, &(ManifestTarget){
                    .type = manifestTargetTypeLink, .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/1"),
                    .path = strNewFmt("%s/ts/1", testPath()), .tablespaceId = 1, .tablespaceName = STRDEF("ts1")});
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
                    .destination = strNewFmt("%s/ts/1", testPath()), .group = groupName(), .user = userName()});

            // Always sort
            lstSort(manifest->pub.targetList, sortOrderAsc);
            lstSort(manifest->pub.fileList, sortOrderAsc);
            lstSort(manifest->pub.linkList, sortOrderAsc);
            lstSort(manifest->pub.pathList, sortOrderAsc);
        }
        MEM_CONTEXT_NEW_END();

        manifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteP(storageRepoWrite(),
                strNew(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE))));

        // Add a few bogus paths/files/links to be removed in delta
        storagePathCreateP(storagePgWrite(), STRDEF("bogus1/bogus2"));
        storagePathCreateP(storagePgWrite(), STRDEF(PG_PATH_GLOBAL "/bogus3"));

        // Add a few bogus links to be deleted
        THROW_ON_SYS_ERROR(
            symlink("../wal", strZ(strNewFmt("%s/pg_wal2", strZ(pgPath)))) == -1, FileOpenError,
            "unable to create symlink");

        TEST_RESULT_VOID(cmdRestore(), "successful restore");

        TEST_RESULT_LOG(
            "P00   INFO: repo1: restore backup set 20161219-212741F_20161219-212918I\n"
            "P00   INFO: map link 'pg_hba.conf' to '../config/pg_hba.conf'\n"
            "P00   INFO: map link 'pg_wal' to '../wal'\n"
            "P00   INFO: map link 'postgresql.conf' to '../config/postgresql.conf'\n"
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/config' exists\n"
            "P00 DETAIL: check '{[path]}/wal' exists\n"
            "P00 DETAIL: check '{[path]}/ts/1/PG_10_201707211' exists\n"
            "P00 DETAIL: skip 'tablespace_map' -- tablespace links will be created based on mappings\n"
            "P00   INFO: remove invalid files/links/paths from '{[path]}/pg'\n"
            "P00 DETAIL: remove invalid path '{[path]}/pg/bogus1'\n"
            "P00 DETAIL: remove invalid path '{[path]}/pg/global/bogus3'\n"
            "P00 DETAIL: remove invalid link '{[path]}/pg/pg_wal2'\n"
            "P00 DETAIL: remove invalid file '{[path]}/pg/tablespace_map'\n"
            "P00 DETAIL: create path '{[path]}/pg/base'\n"
            "P00 DETAIL: create path '{[path]}/pg/base/1'\n"
            "P00 DETAIL: create path '{[path]}/pg/base/16384'\n"
            "P00 DETAIL: create path '{[path]}/pg/base/32768'\n"
            "P00 DETAIL: create symlink '{[path]}/pg/pg_hba.conf' to '../config/pg_hba.conf'\n"
            "P00 DETAIL: create symlink '{[path]}/pg/postgresql.conf' to '../config/postgresql.conf'\n"
            "P01   INFO: restore file {[path]}/pg/base/32768/32769 (32KB, 49%) checksum a40f0986acb1531ce0cc75a23dcf8aa406ae9081\n"
            "P01   INFO: restore file {[path]}/pg/base/16384/16385 (16KB, 74%) checksum d74e5f7ebe52a3ed468ba08c5b6aefaccd1ca88f\n"
            "P01   INFO: restore file {[path]}/pg/global/pg_control.pgbackrest.tmp (8KB, 87%)"
                " checksum 5e2b96c19c4f5c63a5afa2de504d29fe64a4c908\n"
            "P01   INFO: restore file {[path]}/pg/base/1/2 (8KB, 99%) checksum 4d7b2a36c5387decf799352a3751883b7ceb96aa\n"
            "P01   INFO: restore file {[path]}/pg/postgresql.conf (15B, 99%) checksum 98b8abb2e681e2a5a7d8ab082c0a79727887558d\n"
            "P01   INFO: restore file {[path]}/pg/pg_hba.conf (11B, 99%) checksum 401215e092779574988a854d8c7caed7f91dba4b\n"
            "P01   INFO: restore file {[path]}/pg/base/32768/PG_VERSION (4B, 99%)"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01   INFO: restore file {[path]}/pg/base/16384/PG_VERSION (4B, 99%)"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01   INFO: restore file {[path]}/pg/base/1/PG_VERSION (4B, 99%) checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01   INFO: restore file {[path]}/pg/PG_VERSION (4B, 100%) checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01   INFO: restore file {[path]}/pg/global/999 (0B, 100%)\n"
            "P00 DETAIL: sync path '{[path]}/config'\n"
            "P00 DETAIL: sync path '{[path]}/pg'\n"
            "P00 DETAIL: sync path '{[path]}/pg/base'\n"
            "P00 DETAIL: sync path '{[path]}/pg/base/1'\n"
            "P00 DETAIL: sync path '{[path]}/pg/base/16384'\n"
            "P00 DETAIL: sync path '{[path]}/pg/base/32768'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_wal'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc/1'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc/1/PG_10_201707211'\n"
            "P00   INFO: restore global/pg_control (performed last to ensure aborted restores cannot be started)\n"
            "P00 DETAIL: sync path '{[path]}/pg/global'");

        testRestoreCompare(
            storagePg(), NULL, manifest,
            ". {path}\n"
            "PG_VERSION {file, s=4, t=1482182860}\n"
            "base {path}\n"
            "base/1 {path}\n"
            "base/1/2 {file, s=8192, t=1482182860}\n"
            "base/1/PG_VERSION {file, s=4, t=1482182860}\n"
            "base/16384 {path}\n"
            "base/16384/16385 {file, s=16384, t=1482182860}\n"
            "base/16384/PG_VERSION {file, s=4, t=1482182860}\n"
            "base/32768 {path}\n"
            "base/32768/32769 {file, s=32768, t=1482182860}\n"
            "base/32768/PG_VERSION {file, s=4, t=1482182860}\n"
            "global {path}\n"
            "global/999 {file, s=0, t=1482182860}\n"
            "global/pg_control {file, s=8192, t=1482182860}\n"
            "pg_hba.conf {link, d=../config/pg_hba.conf}\n"
            "pg_tblspc {path}\n"
            "pg_tblspc/1 {link, d={[path]}/ts/1}\n"
            "pg_wal {link, d=../wal}\n"
            "postgresql.conf {link, d=../config/postgresql.conf}\n");

        testRestoreCompare(
            storagePg(), STRDEF("pg_tblspc/1"), manifest,
            ". {link, d={[path]}/ts/1}\n"
            "16384 {path}\n"
            "16384/PG_VERSION {file, s=4, t=1482182860}\n"
            "PG_10_201707211 {path}\n");

        testRestoreCompare(
            storagePg(), STRDEF("../wal"), manifest,
            ". {path}\n");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("incremental delta selective restore");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, "/repo-bogus");
        hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath);
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pgPath)));
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH_SPOOL);
        strLstAddZ(argList, "--delta");
        strLstAddZ(argList, "--type=preserve");
        strLstAddZ(argList, "--link-map=pg_wal=../wal");
        strLstAddZ(argList, "--link-map=postgresql.conf=../config/postgresql.conf");
        strLstAddZ(argList, "--link-map=pg_hba.conf=../config/pg_hba.conf");
        strLstAddZ(argList, "--db-include=16384");
        harnessCfgLoad(cfgCmdRestore, argList);

        // Move pg1-path and put a link in its place. This tests that restore works when pg1-path is a symlink yet should be
        // completely invisible in the manifest and logging.
        TEST_SYSTEM_FMT("mv %s %s-data", strZ(pgPath), strZ(pgPath));
        TEST_SYSTEM_FMT("ln -s %s-data %s ", strZ(pgPath), strZ(pgPath));

        // Create the stanza archive pool path to check that it gets removed
        HRN_STORAGE_PUT_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE "/empty.txt");

        // Write recovery.conf so we don't get a preserve warning
        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR), BUFSTRDEF("Some Settings"));

        TEST_RESULT_VOID(cmdRestore(), "successful restore");

        TEST_RESULT_LOG(
            "P00   INFO: repo2: restore backup set 20161219-212741F_20161219-212918I\n"
            "P00   INFO: map link 'pg_hba.conf' to '../config/pg_hba.conf'\n"
            "P00   INFO: map link 'pg_wal' to '../wal'\n"
            "P00   INFO: map link 'postgresql.conf' to '../config/postgresql.conf'\n"
            "P00 DETAIL: databases found for selective restore (1, 16384, 32768)\n"
            "P00 DETAIL: databases excluded (zeroed) from selective restore (32768)\n"
            "P00 DETAIL: check '{[path]}/pg' exists\n"
            "P00 DETAIL: check '{[path]}/config' exists\n"
            "P00 DETAIL: check '{[path]}/wal' exists\n"
            "P00 DETAIL: check '{[path]}/ts/1/PG_10_201707211' exists\n"
            "P00 DETAIL: skip 'tablespace_map' -- tablespace links will be created based on mappings\n"
            "P00 DETAIL: remove 'global/pg_control' so cluster will not start if restore does not complete\n"
            "P00   INFO: remove invalid files/links/paths from '{[path]}/pg'\n"
            "P00   INFO: remove invalid files/links/paths from '{[path]}/wal'\n"
            "P00   INFO: remove invalid files/links/paths from '{[path]}/ts/1/PG_10_201707211'\n"
            "P01 DETAIL: restore zeroed file {[path]}/pg/base/32768/32769 (32KB, 49%)\n"
            "P01 DETAIL: restore file {[path]}/pg/base/16384/16385 - exists and matches backup (16KB, 74%)"
                " checksum d74e5f7ebe52a3ed468ba08c5b6aefaccd1ca88f\n"
            "P01   INFO: restore file {[path]}/pg/global/pg_control.pgbackrest.tmp (8KB, 87%)"
                " checksum 5e2b96c19c4f5c63a5afa2de504d29fe64a4c908\n"
            "P01 DETAIL: restore file {[path]}/pg/base/1/2 - exists and matches backup (8KB, 99%)"
                " checksum 4d7b2a36c5387decf799352a3751883b7ceb96aa\n"
            "P01 DETAIL: restore file {[path]}/pg/postgresql.conf - exists and matches backup (15B, 99%)"
                " checksum 98b8abb2e681e2a5a7d8ab082c0a79727887558d\n"
            "P01 DETAIL: restore file {[path]}/pg/pg_hba.conf - exists and matches backup (11B, 99%)"
                " checksum 401215e092779574988a854d8c7caed7f91dba4b\n"
            "P01 DETAIL: restore file {[path]}/pg/base/32768/PG_VERSION - exists and matches backup (4B, 99%)"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file {[path]}/pg/base/16384/PG_VERSION - exists and matches backup (4B, 99%)"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file {[path]}/pg/base/1/PG_VERSION - exists and matches backup (4B, 99%)"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file {[path]}/pg/PG_VERSION - exists and matches backup (4B, 100%)"
                " checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P01 DETAIL: restore file {[path]}/pg/global/999 - exists and is zero size (0B, 100%)\n"
            "P00 DETAIL: sync path '{[path]}/config'\n"
            "P00 DETAIL: sync path '{[path]}/pg'\n"
            "P00 DETAIL: sync path '{[path]}/pg/base'\n"
            "P00 DETAIL: sync path '{[path]}/pg/base/1'\n"
            "P00 DETAIL: sync path '{[path]}/pg/base/16384'\n"
            "P00 DETAIL: sync path '{[path]}/pg/base/32768'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_wal'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc/1'\n"
            "P00 DETAIL: sync path '{[path]}/pg/pg_tblspc/1/PG_10_201707211'\n"
            "P00   INFO: restore global/pg_control (performed last to ensure aborted restores cannot be started)\n"
            "P00 DETAIL: sync path '{[path]}/pg/global'");

        // Check stanza archive spool path was removed
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_PATH_ARCHIVE);

        // -------------------------------------------------------------------------------------------------------------------------
        // Keep this test at the end since is corrupts the repo
        TEST_TITLE("remove a repo file so a restore job errors");

        storageRemoveP(storageRepoWrite(), STRDEF(TEST_REPO_PATH PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL), .errorOnMissing = true);
        storageRemoveP(storagePgWrite(), STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL), .errorOnMissing = true);

        // Set log level to warn
        harnessLogLevelSet(logLevelWarn);

        TEST_ERROR_FMT(
            cmdRestore(), FileMissingError,
            "raised from local-1 shim protocol: unable to open missing file"
                " '%s/repo/backup/test1/20161219-212741F_20161219-212918I/pg_data/global/pg_control' for read",
            testPath());

        // Free local processes that were not freed because of the error
        protocolFree();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
