/***********************************************************************************************************************************
Test Restore Command
***********************************************************************************************************************************/
#include "common/compress/gzip/compress.h"
#include "common/crypto/cipherBlock.h"
#include "common/io/io.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "postgres/version.h"
#include "storage/posix/storage.h"
#include "storage/helper.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessStorage.h"

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
    "\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,\"backup-type\":\"incr\",\"db-id\":1,"             \
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
        storageInfoListP(storage, pgPath, hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc),
        "pg path info list for restore compare");

    // Compare
    TEST_RESULT_STR_Z(callbackData.content, compare, "    compare result manifest");

    FUNCTION_HARNESS_RESULT_VOID();
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
        result->info = infoNew(NULL);

        result->data.backupLabel = strDup(label);
        result->data.pgVersion = pgVersion;

        if (strEndsWithZ(label, "I"))
            result->data.backupType = backupTypeIncr;
        else if (strEndsWithZ(label, "D"))
            result->data.backupType = backupTypeDiff;
        else
            result->data.backupType = backupTypeFull;

        ManifestTarget targetBase = {.name = MANIFEST_TARGET_PGDATA_STR, .path = pgPath};
        manifestTargetAdd(result, &targetBase);
        ManifestPath pathBase = {.name = MANIFEST_TARGET_PGDATA_STR, .mode = 0700, .group = groupName(), .user = userName()};
        manifestPathAdd(result, &pathBase);
        ManifestFile fileVersion = {
            .name = STRDEF("pg_data/" PG_FILE_PGVERSION), .mode = 0600, .group = groupName(), .user = userName()};
        manifestFileAdd(result, &fileVersion);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_HARNESS_RESULT(MANIFEST, result);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("restoreFile()"))
    {
        const String *repoFileReferenceFull = strNew("20190509F");
        const String *repoFile1 = strNew("pg_data/testfile");

        // Start a protocol server to test the protocol directly
        Buffer *serverWrite = bufNew(8192);
        IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
        ioWriteOpen(serverWriteIo);

        ProtocolServer *server = protocolServerNew(
            strNew("test"), strNew("test"), ioBufferReadNew(bufNew(0)), serverWriteIo);

        bufUsedSet(serverWrite, 0);

        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create the pg path
        storagePathCreateP(storagePgWrite(), NULL, .mode = 0700);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("sparse-zero"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                true, 0x10000000000UL, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            false, "zero sparse 1TB file");
        TEST_RESULT_UINT(storageInfoNP(storagePg(), strNew("sparse-zero")).size, 0x10000000000UL, "    check size");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("normal-zero"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 0, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, false, false, NULL),
            true, "zero-length file");
        TEST_RESULT_UINT(storageInfoNP(storagePg(), strNew("normal-zero")).size, 0, "    check size");

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a compressed encrypted repo file
        StorageWrite *ceRepoFile = storageNewWriteNP(
            storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strPtr(repoFileReferenceFull), strPtr(repoFile1)));
        IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(ceRepoFile));
        ioFilterGroupAdd(filterGroup, gzipCompressNew(3, false));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRDEF("badpass"), NULL));

        storagePutNP(ceRepoFile, BUFSTRDEF("acefile"));

        TEST_ERROR(
            restoreFile(
                repoFile1, repoFileReferenceFull, true, strNew("normal"), strNew("ffffffffffffffffffffffffffffffffffffffff"),
                false, 7, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, false, false, strNew("badpass")),
            ChecksumError,
            "error restoring 'normal': actual checksum 'd1cd8a7d11daa26814b93eb604e1d49ab4b43770' does not match expected checksum"
                " 'ffffffffffffffffffffffffffffffffffffffff'");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, true, strNew("normal"), strNew("d1cd8a7d11daa26814b93eb604e1d49ab4b43770"),
                false, 7, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, false, false, strNew("badpass")),
            true, "copy file");

        StorageInfo info = storageInfoNP(storagePg(), strNew("normal"));
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_UINT(info.size, 7, "    check size");
        TEST_RESULT_UINT(info.mode, 0600, "    check mode");
        TEST_RESULT_UINT(info.timeModified, 1557432154, "    check time");
        TEST_RESULT_STR(strPtr(info.user), testUser(), "    check user");
        TEST_RESULT_STR(strPtr(info.group), testGroup(), "    check group");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("normal"))))), "acefile", "    check contents");

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a repo file
        storagePutNP(
            storageNewWriteNP(
                storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strPtr(repoFileReferenceFull), strPtr(repoFile1))),
            BUFSTRDEF("atestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta missing");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        size_t oldBufferSize = ioBufferSize();
        ioBufferSizeSet(4);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            false, "sha1 delta existing");

        ioBufferSizeSet(oldBufferSize);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432155, true, true, NULL),
            false, "sha1 delta force existing");

        // Change the existing file so it no longer matches by size
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("atestfile2"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta existing, size differs");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("atestfile2"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432155, true, true, NULL),
            true, "delta force existing, size differs");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        // Change the existing file so it no longer matches by content
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("btestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta existing, content differs");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("btestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432155, true, true, NULL),
            true, "delta force existing, timestamp differs");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432153, true, true, NULL),
            true, "delta force existing, timestamp after copy time");

        // Change the existing file to zero-length
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF(""));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 0, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            false, "sha1 delta existing, content differs");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ("protocol"));
        varLstAdd(paramList, varNewUInt64(9));
        varLstAdd(paramList, varNewUInt64(1557432100));
        varLstAdd(paramList, varNewStrZ("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewStr(repoFile1));
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewStrZ("0677"));
        varLstAdd(paramList, varNewStrZ(testUser()));
        varLstAdd(paramList, varNewStrZ(testGroup()));
        varLstAdd(paramList, varNewUInt64(1557432200));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewStr(repoFileReferenceFull));
        varLstAdd(paramList, varNewBool(false));

        TEST_RESULT_BOOL(restoreProtocol(PROTOCOL_COMMAND_RESTORE_FILE_STR, paramList, server), true, "protocol restore file");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":true}\n", "    check result");
        bufUsedSet(serverWrite, 0);

        info = storageInfoNP(storagePg(), strNew("protocol"));
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_UINT(info.size, 9, "    check size");
        TEST_RESULT_UINT(info.mode, 0677, "    check mode");
        TEST_RESULT_UINT(info.timeModified, 1557432100, "    check time");
        TEST_RESULT_STR(strPtr(info.user), testUser(), "    check user");
        TEST_RESULT_STR(strPtr(info.group), testGroup(), "    check group");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("protocol"))))), "atestfile", "    check contents");

        paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ("protocol"));
        varLstAdd(paramList, varNewUInt64(9));
        varLstAdd(paramList, varNewUInt64(1557432100));
        varLstAdd(paramList, varNewStrZ("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewStr(repoFile1));
        varLstAdd(paramList, varNewStr(repoFileReferenceFull));
        varLstAdd(paramList, varNewStrZ("0677"));
        varLstAdd(paramList, varNewStrZ(testUser()));
        varLstAdd(paramList, varNewStrZ(testGroup()));
        varLstAdd(paramList, varNewUInt64(1557432200));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, NULL);

        TEST_RESULT_BOOL(restoreProtocol(PROTOCOL_COMMAND_RESTORE_FILE_STR, paramList, server), true, "protocol restore file");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":false}\n", "    check result");
        bufUsedSet(serverWrite, 0);

        // Check invalid protocol function
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(restoreProtocol(strNew(BOGUS_STR), paramList, server), false, "invalid function");
    }

    // *****************************************************************************************************************************
    if (testBegin("restorePathValidate()"))
    {
        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        // Path missing
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(restorePathValidate(), PathMissingError, "$PGDATA directory '%s/pg' does not exist", testPath());

        // Create PGDATA
        storagePathCreateNP(storagePgWrite(), NULL);

        // postmaster.pid is present
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("postmaster.pid")), NULL);

        TEST_ERROR_FMT(
            restorePathValidate(), PostmasterRunningError,
            "unable to restore while PostgreSQL is running\n"
                "HINT: presence of 'postmaster.pid' in '%s/pg' indicates PostgreSQL is running.\n"
                "HINT: remove 'postmaster.pid' only if PostgreSQL is not running.",
            testPath());

        storageRemoveP(storagePgWrite(), strNew("postmaster.pid"), .errorOnMissing = true);

        // PGDATA directory does not look valid
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--delta");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restorePathValidate(), "restore --delta with invalid PGDATA");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptDelta), false, "--delta set to false");
        TEST_RESULT_LOG_FMT(
            "P00   WARN: --delta or --force specified but unable to find 'PG_VERSION' or 'backup.manifest' in '%s/pg' to"
                " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                " exist in the destination directories the restore will be aborted.",
            testPath());

        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("backup.manifest")), NULL);
        TEST_RESULT_VOID(restorePathValidate(), "restore --delta with valid PGDATA");
        storageRemoveP(storagePgWrite(), strNew("backup.manifest"), .errorOnMissing = true);

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAddZ(argList, "--force");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restorePathValidate(), "restore --force with invalid PGDATA");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptForce), false, "--force set to false");
        TEST_RESULT_LOG_FMT(
            "P00   WARN: --delta or --force specified but unable to find 'PG_VERSION' or 'backup.manifest' in '%s/pg' to"
                " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                " exist in the destination directories the restore will be aborted.",
            testPath());

        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew(PG_FILE_PGVERSION)), NULL);
        TEST_RESULT_VOID(restorePathValidate(), "restore --force with valid PGDATA");
        storageRemoveP(storagePgWrite(), strNew(PG_FILE_PGVERSION), .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreBackupSet()"))
    {
        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Error when no backups are present then add backups to backup.info
        // -------------------------------------------------------------------------------------------------------------------------
        InfoBackup *infoBackup = infoBackupNewLoad(ioBufferReadNew(harnessInfoChecksumZ(TEST_RESTORE_BACKUP_INFO_DB)));

        TEST_ERROR_FMT(restoreBackupSet(infoBackup), BackupSetInvalidError, "no backup sets to restore");

        // Error on invalid backup set
        // -------------------------------------------------------------------------------------------------------------------------
        infoBackup = infoBackupNewLoad(
            ioBufferReadNew(harnessInfoChecksumZ(TEST_RESTORE_BACKUP_INFO "\n" TEST_RESTORE_BACKUP_INFO_DB)));

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--set=BOGUS");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(restoreBackupSet(infoBackup), BackupSetInvalidError, "backup set BOGUS is not valid");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreManifestValidate()"))
    {
        // Error on mismatched label
        // -------------------------------------------------------------------------------------------------------------------------
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

        // Remap data directory
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreManifestMap(manifest), "base directory is not remapped");
        TEST_RESULT_STR_STR(
            manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path, pgPath, "base directory is not remapped");

        // Now change pg1-path so the data directory gets remapped
        pgPath = strNewFmt("%s/pg2", testPath());

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreManifestMap(manifest), "base directory is remapped");
        TEST_RESULT_STR_STR(manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path, pgPath, "base directory is remapped");
        TEST_RESULT_LOG_FMT("P00   INFO: remap data directory to '%s/pg2'", testPath());

        // Remap tablespaces
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--tablespace-map=bogus=/bogus");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--tablespace-map=2=/2");
        strLstAddZ(argList, "--tablespace-map=ts2=/ts2");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(
            restoreManifestMap(manifest), TablespaceMapError, "tablespace remapped by name 'ts2' and id 2 with different paths");

        // Remap one tablespace using the id and another with the name
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--tablespace-map=1=/1-2");
        strLstAddZ(argList, "--tablespace-map=ts2=/2-2");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--tablespace-map=2=/2-3");
        strLstAddZ(argList, "--tablespace-map-all=/all");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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
        manifest->data.pgVersion = PG_VERSION_92;

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--tablespace-map-all=/all2");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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

        // Remap links
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--link-map=bogus=bogus");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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

        // Remap both links
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--link-map=pg_hba.conf=../conf2/pg_hba2.conf");
        strLstAddZ(argList, "--link-map=pg_wal=/wal2");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--link-all");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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

        // Owner is not root and all ownership is good
        // -------------------------------------------------------------------------------------------------------------------------
        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);
        TEST_RESULT_VOID(restoreManifestOwner(manifest), "non-root user with good ownership");

        // Owner is not root but has no user name
        // -------------------------------------------------------------------------------------------------------------------------
        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);

        userLocalData.groupName = NULL;
        userLocalData.userName = NULL;

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "non-root user with no username");

        TEST_RESULT_LOG_FMT(
            "P00   WARN: unknown user '%s' in backup manifest mapped to current user\n"
            "P00   WARN: unknown group '%s' in backup manifest mapped to current group",
            testUser(), testUser());

        userInitInternal();

        // Owner is not root and some ownership is bad
        // -------------------------------------------------------------------------------------------------------------------------
        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);

        ManifestPath path = {.name = STRDEF("pg_data/bogus_path"), .user = STRDEF("path-user-bogus")};
        manifestPathAdd(manifest, &path);
        ManifestFile file = {.name = STRDEF("pg_data/bogus_file"), .mode = 0600, .group = STRDEF("file-group-bogus")};
        manifestFileAdd(manifest, &file);
        ManifestLink link = {.name = STRDEF("pg_data/bogus_link"), .destination = STRDEF("/"), .group = STRDEF("link-group-bogus")};
        manifestLinkAdd(manifest, &link);

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "non-root user with some bad ownership");

        TEST_RESULT_LOG(
            "P00   WARN: unknown user in backup manifest mapped to current user\n"
            "P00   WARN: unknown user 'path-user-bogus' in backup manifest mapped to current user\n"
            "P00   WARN: unknown group in backup manifest mapped to current group\n"
            "P00   WARN: unknown group 'file-group-bogus' in backup manifest mapped to current group\n"
            "P00   WARN: unknown group 'link-group-bogus' in backup manifest mapped to current group");

        // Owner is root and ownership is good
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-path=/repo");
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);
        userLocalData.userRoot = true;

        storagePathCreateP(storagePgWrite(), NULL, .mode = 0700);

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "root user with good ownership");

        // Owner is root and user is bad
        // -------------------------------------------------------------------------------------------------------------------------
        manifestPathAdd(manifest, &path);
        // manifestFileAdd(manifest, &file);

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "root user with bad user");

        TEST_RESULT_LOG_FMT("P00   WARN: unknown group in backup manifest mapped to '%s'", testUser());

        // Owner is root and group is bad
        // -------------------------------------------------------------------------------------------------------------------------
        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);
        userLocalData.userRoot = true;

        manifestFileAdd(manifest, &file);
        manifestLinkAdd(manifest, &link);

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "root user with bad user");

        TEST_RESULT_LOG_FMT("P00   WARN: unknown user in backup manifest mapped to '%s'", testUser());

        // Owner is root and ownership of pg_data is bad
        // -------------------------------------------------------------------------------------------------------------------------
        manifestPathAdd(manifest, &path);
        manifestFileAdd(manifest, &file);

        TEST_SYSTEM_FMT("sudo chown 77777:77777 %s", strPtr(pgPath));

        userLocalData.userName = STRDEF("root");
        userLocalData.groupName = STRDEF("root");

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "root user with bad ownership of pg_data");

        TEST_RESULT_LOG(
            "P00   WARN: unknown user in backup manifest mapped to 'root'\n"
            "P00   WARN: unknown group in backup manifest mapped to 'root'");
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

        TEST_RESULT_LOG_FMT("P00 DETAIL: update ownership for '%s'", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("restoreCleanOwnership() update to bogus (new)");

        // Will succeed because bogus will be remapped to the current user/group
        restoreCleanOwnership(STR(testPath()), STRDEF("bogus"), STRDEF("bogus"), 0, 0, true);

        // Test again with only group for coverage
        restoreCleanOwnership(STR(testPath()), STRDEF("bogus"), STRDEF("bogus"), userId(), 0, true);

        // Directory with bad permissions/mode
        // -------------------------------------------------------------------------------------------------------------------------
        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());
        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);

        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        storagePathCreateP(storagePgWrite(), NULL, .mode = 0600);

        userLocalData.userId = getuid() + 1;

        TEST_ERROR_FMT(
            restoreClean(manifest), PathOpenError, "unable to restore to path '%s/pg' not owned by current user", testPath());

        TEST_RESULT_LOG_FMT("P00 DETAIL: check '%s/pg' exists", testPath());

        userLocalData.userRoot = true;

        TEST_ERROR_FMT(
            restoreClean(manifest), PathOpenError, "unable to restore to path '%s/pg' without rwx permissions", testPath());

        TEST_RESULT_LOG_FMT("P00 DETAIL: check '%s/pg' exists", testPath());

        userInitInternal();

        TEST_ERROR_FMT(
            restoreClean(manifest), PathOpenError, "unable to restore to path '%s/pg' without rwx permissions", testPath());

        TEST_RESULT_LOG_FMT("P00 DETAIL: check '%s/pg' exists", testPath());

        storagePathRemoveNP(storagePgWrite(), NULL);
        storagePathCreateP(storagePgWrite(), NULL, .mode = 0700);

        // Fail on restore with directory not empty
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(storageNewWriteNP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR), NULL);

        TEST_ERROR_FMT(
            restoreClean(manifest), PathNotEmptyError,
            "unable to restore to path '%s/pg' because it contains files\n"
                "HINT: try using --delta if this is what you intended.",
            testPath());

        TEST_RESULT_LOG_FMT("P00 DETAIL: check '%s/pg' exists", testPath());

        // Succeed when all directories empty
        // -------------------------------------------------------------------------------------------------------------------------
        storageRemoveNP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR);

        manifestTargetAdd(
            manifest, &(ManifestTarget){
                .name = STRDEF("pg_data/pg_hba.conf"), .path = STRDEF("../conf"), .file = STRDEF("pg_hba.conf"),
                .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_hba.conf"), .destination = STRDEF("../conf/pg_hba.conf")});

        storagePathCreateP(storageTest, STRDEF("conf"), .mode = 0700);

        TEST_RESULT_VOID(restoreClean(manifest), "normal restore");

        TEST_RESULT_LOG_FMT(
            "P00 DETAIL: check '%s/pg' exists\n"
            "P00 DETAIL: check '%s/conf' exists\n"
            "P00 DETAIL: create symlink '%s/pg/pg_hba.conf' to '../conf/pg_hba.conf'",
            testPath(), testPath(), testPath());

        TEST_SYSTEM_FMT("rm -rf %s/*", strPtr(pgPath));

        // Succeed when all directories empty and ignore recovery.conf
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--type=preserve");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreClean(manifest), "normal restore no recovery.conf");

        TEST_RESULT_LOG_FMT(
            "P00 DETAIL: check '%s/pg' exists\n"
            "P00 DETAIL: check '%s/conf' exists\n"
            "P00 DETAIL: create symlink '%s/pg/pg_hba.conf' to '../conf/pg_hba.conf'",
            testPath(), testPath(), testPath());

        TEST_SYSTEM_FMT("rm -rf %s/*", strPtr(pgPath));

        storagePutNP(storageNewWriteNP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR), NULL);
        TEST_RESULT_VOID(restoreClean(manifest), "normal restore ignore recovery.conf");

        TEST_RESULT_LOG_FMT(
            "P00 DETAIL: check '%s/pg' exists\n"
            "P00 DETAIL: check '%s/conf' exists\n"
            "P00 DETAIL: create symlink '%s/pg/pg_hba.conf' to '../conf/pg_hba.conf'",
            testPath(), testPath(), testPath());
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreSelectiveExpression()"))
    {
        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // No valid databases
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-path=/repo");
        strLstAddZ(argList, "--pg1-path=/pg");
        strLstAddZ(argList, "--db-include=test1");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        Manifest *manifest = NULL;

        MEM_CONTEXT_NEW_BEGIN("Manifest")
        {
            manifest = manifestNewInternal();
            manifest->data.pgVersion = PG_VERSION_84;

            manifestFileAdd(manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_NEW_END();

        TEST_ERROR(
            restoreSelectiveExpression(manifest), FormatError,
            "no databases found for selective restore\n"
            "HINT: is this a valid cluster?");

        // Database id is missing on disk
        // -------------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_BEGIN(manifest->memContext)
        {
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("postgres"), .id = 12173, .lastSystemId = 12168});
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("template0"), .id = 12168, .lastSystemId = 12168});
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("template1"), .id = 1, .lastSystemId = 12168});
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("test1"), .id = 16384, .lastSystemId = 12168});
            manifestFileAdd(
                manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/1/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_END();

        TEST_ERROR(restoreSelectiveExpression(manifest), DbMissingError, "database to include 'test1' does not exist");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1)");

        // All databases selected
        // -------------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_BEGIN(manifest->memContext)
        {
            manifestFileAdd(
                manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/16384/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_END();

        TEST_RESULT_PTR(restoreSelectiveExpression(manifest), NULL, "all databases selected");

        TEST_RESULT_LOG(
            "P00 DETAIL: databases found for selective restore (1, 16384)\n"
            "P00   INFO: nothing to filter - all user databases have been selected");

        // One database selected without tablespace id
        // -------------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_BEGIN(manifest->memContext)
        {
            manifestDbAdd(manifest, &(ManifestDb){.name = STRDEF("test2"), .id = 32768, .lastSystemId = 12168});
            manifestTargetAdd(
                manifest, &(ManifestTarget){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/16387"), .tablespaceId = 16387, .tablespaceName = STRDEF("ts1"),
                    .path = STRDEF("/ts1")});
            manifestFileAdd(
                manifest, &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/32768/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_END();

        TEST_RESULT_STR_Z(
            restoreSelectiveExpression(manifest), "(^pg_data/base/32768/)|(^pg_tblspc/16387/32768/)", "one database selected");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 16384, 32768)");

        // One database selected with tablespace id
        // -------------------------------------------------------------------------------------------------------------------------
        manifest->data.pgVersion = PG_VERSION_94;

        MEM_CONTEXT_BEGIN(manifest->memContext)
        {
            manifestFileAdd(
                manifest, &(ManifestFile){
                    .name = STRDEF(MANIFEST_TARGET_PGTBLSPC "/16387/PG_9.4_201409291/65536/" PG_FILE_PGVERSION)});
        }
        MEM_CONTEXT_END();

        TEST_RESULT_STR_Z(
            restoreSelectiveExpression(manifest),
            "(^pg_data/base/32768/)|(^pg_tblspc/16387/PG_9.4_201409291/32768/)|(^pg_data/base/65536/)"
                "|(^pg_tblspc/16387/PG_9.4_201409291/65536/)",
            "one database selected");

        TEST_RESULT_LOG("P00 DETAIL: databases found for selective restore (1, 16384, 32768, 65536)");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreRecoveryConf()"))
    {
        StringList *argBaseList = strLstNew();
        strLstAddZ(argBaseList, "pgbackrest");
        strLstAddZ(argBaseList, "--stanza=test1");
        strLstAddZ(argBaseList, "--repo1-path=/repo");
        strLstAddZ(argBaseList, "--pg1-path=/pg");
        strLstAddZ(argBaseList, "restore");

        // No recovery conf
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--type=none");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_PTR(restoreRecoveryConf(PG_VERSION_94), NULL, "recovery type is none");

        // User-specified options
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--recovery-option=a-setting=a");
        strLstAddZ(argList, "--recovery-option=b_setting=b");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94),
            "a_setting = 'a'\n"
                "b_setting = 'b'\n"
                "restore_command = 'pgbackrest --pg1-path=/pg --repo1-path=/repo --stanza=test1 archive-get %f \"%p\"'\n",
            "user-specified options");

        // Override restore_command
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argBaseList, "--recovery-option=restore-command=my_restore_command");
        argList = strLstDup(argBaseList);
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94),
            "restore_command = 'my_restore_command'\n",
            "override restore command");

        // Recovery target immediate
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--type=immediate");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94),
            "restore_command = 'my_restore_command'\n"
            "recovery_target = 'immediate'\n",
            "recovery target immediate");

        // Recovery target time with timeline
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--type=time");
        strLstAddZ(argList, "--target=TIME");
        strLstAddZ(argList, "--target-timeline=3");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94),
            "restore_command = 'my_restore_command'\n"
            "recovery_target_time = 'TIME'\n"
            "recovery_target_timeline = '3'\n",
            "recovery target time with timeline");

        // Recovery target inclusive
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--type=time");
        strLstAddZ(argList, "--target=TIME");
        strLstAddZ(argList, "--target-exclusive");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94),
            "restore_command = 'my_restore_command'\n"
            "recovery_target_time = 'TIME'\n"
            "recovery_target_inclusive = 'false'\n",
            "recovery target time inclusive");

        // Recovery target action = shutdown
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--type=immediate");
        strLstAddZ(argList, "--target-action=shutdown");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_95),
            "restore_command = 'my_restore_command'\n"
            "recovery_target = 'immediate'\n"
            "recovery_target_action = 'shutdown'\n",
            "recovery target action shutdown");

        TEST_ERROR(
            restoreRecoveryConf(PG_VERSION_94), OptionInvalidError,
            "target-action=shutdown is only available in PostgreSQL >= 9.5");

        // Recovery target action = pause
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "--type=immediate");
        strLstAddZ(argList, "--target-action=promote");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR_Z(
            restoreRecoveryConf(PG_VERSION_94),
            "restore_command = 'my_restore_command'\n"
            "recovery_target = 'immediate'\n"
            "pause_at_recovery_target = 'false'\n",
            "recovery target action pause");

        TEST_ERROR(
            restoreRecoveryConf(PG_VERSION_90), OptionInvalidError,
            "target-action option is only available in PostgreSQL >= 9.1");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdRestore()"))
    {
        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // Locality error
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--pg1-host=pg1");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdRestore(), HostInvalidError, "restore command must be run on the PostgreSQL host");

        // Write backup info
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR),
            harnessInfoChecksumZ(TEST_RESTORE_BACKUP_INFO "\n" TEST_RESTORE_BACKUP_INFO_DB));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full restore without delta");

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--set=20161219-212741F");
        strLstAddZ(argList, "--type=preserve");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        #define TEST_LABEL                                          "20161219-212741F"
        #define TEST_PGDATA                                         MANIFEST_TARGET_PGDATA "/"
        #define TEST_REPO_PATH                                      STORAGE_REPO_BACKUP "/" TEST_LABEL "/" TEST_PGDATA

        Manifest *manifest = NULL;

        MEM_CONTEXT_NEW_BEGIN("Manifest")
        {
            manifest = manifestNewInternal();
            manifest->info = infoNew(NULL);
            manifest->data.backupLabel = strNew(TEST_LABEL);
            manifest->data.pgVersion = PG_VERSION_84;
            manifest->data.backupType = backupTypeFull;
            manifest->data.backupTimestampCopyStart = 1482182861; // So file timestamps should be less than this

            // Data directory
            manifestTargetAdd(manifest, &(ManifestTarget){.name = MANIFEST_TARGET_PGDATA_STR, .path = pgPath});
            manifestPathAdd(
                manifest,
                &(ManifestPath){.name = MANIFEST_TARGET_PGDATA_STR, .mode = 0700, .group = groupName(), .user = userName()});
            storagePathCreateNP(storagePgWrite(), NULL);

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
            storagePutNP(
                storageNewWriteNP(storageRepoWrite(), STRDEF(TEST_REPO_PATH PG_FILE_PGVERSION)), BUFSTRDEF(PG_VERSION_84_STR "\n"));
        }
        MEM_CONTEXT_NEW_END();

        manifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteNP(storageRepoWrite(),
                strNew(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE))));

        TEST_RESULT_VOID(cmdRestore(), "successful restore");

        TEST_RESULT_LOG_FMT(
            "P00   INFO: restore backup set 20161219-212741F\n"
            "P00 DETAIL: check '%s/pg' exists\n"
            "P00 DETAIL: update mode for '%s/pg' to 0700\n"
            "P00 DETAIL: create path '%s/pg/global'\n"
            "P01   INFO: restore file %s/pg/PG_VERSION (4B, 100%%) checksum 797e375b924134687cbf9eacd37a4355f3d825e4\n"
            "P00   WARN: recovery type is preserve but recovery file does not exist at '%s/pg/recovery.conf'\n"
            "P00 DETAIL: sync path '%s/pg'\n"
            "P00   WARN: backup does not contain 'global/pg_control' -- cluster will not start",
            testPath(), testPath(), testPath(), testPath(), testPath(), testPath());

        testRestoreCompare(
            storagePg(), NULL, manifest,
            ". {path}\n"
            "PG_VERSION {file, s=4, t=1482182860}\n"
            "global {path}");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full restore with force");

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--set=20161219-212741F");
        strLstAddZ(argList, "--delta");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // MEM_CONTEXT_BEGIN(manifest->memContext)
        // {
        //     ManifestFile *file = (ManifestFile *)manifestFileFind(manifest, STRDEF(TEST_PGDATA PG_FILE_PGVERSION));
        //     file->user = strNew("bogus");
        //     file->group = strNew("root");
        // }
        // MEM_CONTEXT_END();

        #undef TEST_LABEL
        #undef TEST_PGDATA
        #undef TEST_REPO_PATH

        cmdRestore();

        TEST_RESULT_LOG_FMT(
            "P00   INFO: restore backup set 20161219-212741F\n"
            "P00 DETAIL: check '%s/pg' exists\n"
            "P00   INFO: remove invalid files/links/paths from '%s/pg'\n"
            "P01 DETAIL: restore file %s/pg/PG_VERSION - exists and matches backup (4B, 100%%)"
                " checksum 797e375b924134687cbf9eacd37a4355f3d825e4\n"
            "P00 DETAIL: sync path '%s/pg'\n"
            "P00   WARN: backup does not contain 'global/pg_control' -- cluster will not start",
            testPath(), testPath(), testPath(), testPath());

        // Remove recovery.conf before file comparison since it will have a new timestamp.  Make sure it existed, though.
        storageRemoveP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR, .errorOnMissing = true);

        testRestoreCompare(
            storagePg(), NULL, manifest,
            ". {path}\n"
            "PG_VERSION {file, s=4, t=1482182860}\n"
            "global {path}");

        // Prepare manifest and backup directory for incremental delta restore
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--delta");
        strLstAddZ(argList, "--type=none");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        #define TEST_LABEL                                          "20161219-212741F_20161219-212918I"
        #define TEST_PGDATA                                         MANIFEST_TARGET_PGDATA "/"
        #define TEST_REPO_PATH                                      STORAGE_REPO_BACKUP "/" TEST_LABEL "/" TEST_PGDATA

        MEM_CONTEXT_NEW_BEGIN("Manifest")
        {
            manifest = manifestNewInternal();
            manifest->info = infoNew(NULL);
            manifest->data.backupLabel = strNew(TEST_LABEL);
            manifest->data.pgVersion = PG_VERSION_84;
            manifest->data.backupType = backupTypeFull;
            manifest->data.backupTimestampCopyStart = 1482182861; // So file timestamps should be less than this

            // Data directory
            manifestTargetAdd(manifest, &(ManifestTarget){.name = MANIFEST_TARGET_PGDATA_STR, .path = pgPath});
            manifestPathAdd(
                manifest,
                &(ManifestPath){.name = MANIFEST_TARGET_PGDATA_STR, .mode = 0700, .group = groupName(), .user = userName()});
            storagePathCreateNP(storagePgWrite(), NULL);

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
                    .checksumSha1 = "8dbabb96e032b8d9f1993c0e4b9141e71ade01a1"});
            storagePutNP(
                storageNewWriteNP(storageRepoWrite(), STRDEF(TEST_REPO_PATH PG_FILE_PGVERSION)), BUFSTRDEF(PG_VERSION_94_STR "\n"));
        }
        MEM_CONTEXT_NEW_END();

        manifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteNP(storageRepoWrite(),
                strNew(STORAGE_REPO_BACKUP "/" TEST_LABEL "/" BACKUP_MANIFEST_FILE))));

        // Add a few bogus paths/files/links to be removed in delta
        storagePathCreateNP(storagePgWrite(), STRDEF("bogus1/bogus2"));
        storagePathCreateNP(storagePgWrite(), STRDEF(PG_PATH_GLOBAL "/bogus3"));

        // Incremental Delta Restore
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdRestore(), "successful restore");

        TEST_RESULT_LOG_FMT(
            "P00   INFO: restore backup set 20161219-212741F_20161219-212918I\n"
            "P00 DETAIL: check '%s/pg' exists\n"
            "P00   INFO: remove invalid files/links/paths from '%s/pg'\n"
            "P00 DETAIL: remove invalid path '%s/pg/bogus1'\n"
            "P00 DETAIL: remove invalid path '%s/pg/global/bogus3'\n"
            "P01   INFO: restore file %s/pg/PG_VERSION (4B, 100%%) checksum 8dbabb96e032b8d9f1993c0e4b9141e71ade01a1\n"
            "P00 DETAIL: sync path '%s/pg'\n"
            "P00   WARN: backup does not contain 'global/pg_control' -- cluster will not start",
            testPath(), testPath(), testPath(), testPath(), testPath(), testPath());
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
