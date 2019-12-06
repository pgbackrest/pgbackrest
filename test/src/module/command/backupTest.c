/***********************************************************************************************************************************
Test Backup Command
***********************************************************************************************************************************/
#include "command/stanza/create.h"
#include "command/stanza/upgrade.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/io.h"
#include "storage/helper.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessPq.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test backup to be sure all files are correct
***********************************************************************************************************************************/
static void
testBackupCompare(const Storage *storage, const String *path, bool fileCompressed, const char *compare)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STORAGE, storage);
        FUNCTION_HARNESS_PARAM(STRING, path);
        FUNCTION_HARNESS_PARAM(BOOL, fileCompressed);
        FUNCTION_HARNESS_PARAM(STRINGZ, compare);
    FUNCTION_HARNESS_END();

    // Get the pg-path as a string
    HarnessStorageInfoListCallbackData callbackData =
    {
        .storage = storage,
        .path = path,
        .content = strNew(""),
        .modeOmit = true,
        .modePath = 0750,
        .modeFile = 0640,
        .userOmit = true,
        .groupOmit = true,
        .timestampOmit = true,
        .rootPathOmit = true,
        .fileCompressed = fileCompressed,
    };

    TEST_RESULT_VOID(
        storageInfoListP(storage, path, hrnStorageInfoListCallback, &callbackData, .recurse = true, .sortOrder = sortOrderAsc),
        "path info list for backup compare");

    // Compare
    TEST_RESULT_STR_Z(callbackData.content, hrnReplaceKey(compare), "    compare file list");

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Generate pq scripts for versions of PostgreSQL
***********************************************************************************************************************************/
typedef struct TestBackupPqScriptParam
{
    VAR_PARAM_HEADER;
    bool startFast;
    bool backupStandby;
    bool noPathEnforce;
} TestBackupPqScriptParam;

#define testBackupPqScriptP(pgVersion, backupStartTime, ...)                                                                                           \
    testBackupPqScript(pgVersion, backupStartTime, (TestBackupPqScriptParam){VAR_PARAM_INIT, __VA_ARGS__})

static void
testBackupPqScript(unsigned int pgVersion, time_t backupTimeStart, TestBackupPqScriptParam param)
{
    const char *pg1Path = strPtr(strNewFmt("%s/pg1", testPath()));
    const char *pg2Path = strPtr(strNewFmt("%s/pg2", testPath()));

    if (pgVersion == PG_VERSION_95)
    {
        ASSERT(!param.backupStandby);

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_95, pg1Path, false, NULL, NULL),

            // Get start time
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000),

            // Start backup
            HRNPQ_MACRO_ADVISORY_LOCK(1, true),
            HRNPQ_MACRO_START_BACKUP_84_95(1, param.startFast, "0/1", "000000010000000000000000"),
            HRNPQ_MACRO_DATABASE_LIST_1(1, "test1"),
            HRNPQ_MACRO_TABLESPACE_LIST_0(1),

            // Get copy start time
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 999),
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 1000),

            // Stop backup
            HRNPQ_MACRO_STOP_BACKUP_LE_95(1, "0/1000001", "000000010000000000000001"),

            // Get stop time
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 2000),

            HRNPQ_MACRO_DONE()
        });
    }
    else if (pgVersion == PG_VERSION_96)
    {
        ASSERT(param.backupStandby);

            harnessPqScriptSet((HarnessPq [])
            {
                // Connect to primary
                HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_96, pg1Path, false, NULL, NULL),

                // Connect to standby
                HRNPQ_MACRO_OPEN_GE_92(2, "dbname='postgres' port=5433", PG_VERSION_96, pg2Path, true, NULL, NULL),

                // Get start time
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000),

                // Start backup
                HRNPQ_MACRO_ADVISORY_LOCK(1, true),
                HRNPQ_MACRO_START_BACKUP_GE_96(1, true, "0/1", "000000010000000000000000"),
                HRNPQ_MACRO_DATABASE_LIST_1(1, "test1"),
                HRNPQ_MACRO_TABLESPACE_LIST_0(1),

                // Wait for standby to sync
                HRNPQ_MACRO_REPLAY_WAIT(2, "0/1"),

                // Get copy start time
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 999),
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 1000),

                // Stop backup
                HRNPQ_MACRO_STOP_BACKUP_96(1, "0/1000001", "000000010000000000000001"),
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 2000),

                // Get stop time
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 2000),

                HRNPQ_MACRO_DONE()
            });
    }
    else
        THROW_FMT(AssertError, "unsupported version %u", pgVersion);
};

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // Start a protocol server to test the protocol directly
    Buffer *serverWrite = bufNew(8192);
    IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
    ioWriteOpen(serverWriteIo);

    ProtocolServer *server = protocolServerNew(strNew("test"), strNew("test"), ioBufferReadNew(bufNew(0)), serverWriteIo);
    bufUsedSet(serverWrite, 0);

    const String *pgFile = strNew("testfile");
    const String *missingFile = strNew("missing");
    const String *backupLabel = strNew("20190718-155825F");
    const String *backupPathFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strPtr(backupLabel), strPtr(pgFile));
    BackupFileResult result = {0};
    VariantList *paramList = varLstNew();

    // *****************************************************************************************************************************
    if (testBegin("segmentNumber()"))
    {
        TEST_RESULT_UINT(segmentNumber(pgFile), 0, "No segment number");
        TEST_RESULT_UINT(segmentNumber(strNewFmt("%s.123", strPtr(pgFile))), 123, "Segment number");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupFile(), backupProtocol"))
    {
        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAddZ(argList, "--repo1-retention-full=1");
        harnessCfgLoad(cfgCmdBackup, argList);

        // Create the pg path
        storagePathCreateP(storagePgWrite(), NULL, .mode = 0700);

        // Pg file missing - ignoreMissing=true
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            result,
            backupFile(
                missingFile, true, 0, NULL, false, 0, missingFile, false, false, 1, backupLabel, false, cipherTypeNone, NULL),
            "pg file missing, ignoreMissing=true, no delta");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 0, "    copy/repo size 0");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultSkip, "    skip file");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        // NULL, zero param values, ignoreMissing=true
        varLstAdd(paramList, varNewStr(missingFile));       // pgFile
        varLstAdd(paramList, varNewBool(true));             // pgFileIgnoreMissing
        varLstAdd(paramList, varNewUInt64(0));              // pgFileSize
        varLstAdd(paramList, NULL);                         // pgFileChecksum
        varLstAdd(paramList, varNewBool(false));            // pgFileChecksumPage
        varLstAdd(paramList, varNewUInt64(0));              // pgFileChecksumPageLsnLimit 1
        varLstAdd(paramList, varNewUInt64(0));              // pgFileChecksumPageLsnLimit 2
        varLstAdd(paramList, varNewStr(missingFile));       // repoFile
        varLstAdd(paramList, varNewBool(false));            // repoFileHasReference
        varLstAdd(paramList, varNewBool(false));            // repoFileCompress
        varLstAdd(paramList, varNewUInt(0));                // repoFileCompressLevel
        varLstAdd(paramList, varNewStr(backupLabel));       // backupLabel
        varLstAdd(paramList, varNewBool(false));            // delta
        varLstAdd(paramList, NULL);                         // cipherSubPass

        TEST_RESULT_BOOL(
            backupProtocol(PROTOCOL_COMMAND_BACKUP_FILE_STR, paramList, server), true, "protocol backup file - skip");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":[3,0,0,null,null]}\n", "    check result");
        bufUsedSet(serverWrite, 0);

        // Pg file missing - ignoreMissing=false
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            backupFile(
                missingFile, false, 0, NULL, false, 0, missingFile, false, false, 1, backupLabel, false, cipherTypeNone, NULL),
            FileMissingError, "unable to open missing file '%s/pg/missing' for read", testPath());

        // Create a pg file to backup
        storagePutP(storageNewWriteP(storagePgWrite(), pgFile), BUFSTRDEF("atestfile"));

        // -------------------------------------------------------------------------------------------------------------------------
        // No prior checksum, no compression, no pageChecksum, no delta, no hasReference

        // With the expected backupCopyResultCopy, unset the storageFeatureCompress bit for the storageRepo for code coverage
        uint64_t feature = storageRepo()->interface.feature;
        ((Storage *)storageRepo())->interface.feature = feature && ((1 << storageFeatureCompress) ^ 0xFFFFFFFFFFFFFFFF);

        TEST_ASSIGN(
            result,
            backupFile(pgFile, false, 9, NULL, false, 0, pgFile, false, false, 1, backupLabel, false, cipherTypeNone, NULL),
            "pg file exists, no repo file, no ignoreMissing, no pageChecksum, no delta, no hasReference");

        ((Storage *)storageRepo())->interface.feature = feature;

        TEST_RESULT_UINT(result.copySize + result.repoSize, 18, "    copy=repo=pgFile size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "    copy file");
        TEST_RESULT_BOOL(
            (strEqZ(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67") &&
                storageExistsP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    copy file to repo success");

        TEST_RESULT_VOID(storageRemoveP(storageRepoWrite(), backupPathFile), "    remove repo file");

        // -------------------------------------------------------------------------------------------------------------------------
        // Test pagechecksum
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, NULL, true, 0xFFFFFFFFFFFFFFFF, pgFile, false, false, 1, backupLabel, false, cipherTypeNone,
                NULL),
            "file checksummed with pageChecksum enabled");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 18, "    copy=repo=pgFile size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "    copy file");
        TEST_RESULT_BOOL(
            (strEqZ(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67") &&
                storageExistsP(storageRepo(), backupPathFile)),
            true,"    copy file to repo success");
        TEST_RESULT_PTR_NE(result.pageChecksumResult, NULL, "    pageChecksumResult is set");
        TEST_RESULT_BOOL(
            varBool(kvGet(result.pageChecksumResult, VARSTRDEF("valid"))), false, "    pageChecksumResult valid=false");
        TEST_RESULT_VOID(storageRemoveP(storageRepoWrite(), backupPathFile), "    remove repo file");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        // pgFileSize, ignoreMissing=false, backupLabel, pgFileChecksumPage, pgFileChecksumPageLsnLimit
        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(pgFile));            // pgFile
        varLstAdd(paramList, varNewBool(false));            // pgFileIgnoreMissing
        varLstAdd(paramList, varNewUInt64(9));              // pgFileSize
        varLstAdd(paramList, NULL);                         // pgFileChecksum
        varLstAdd(paramList, varNewBool(true));             // pgFileChecksumPage
        varLstAdd(paramList, varNewUInt64(0xFFFFFFFF));     // pgFileChecksumPageLsnLimit 1
        varLstAdd(paramList, varNewUInt64(0xFFFFFFFF));     // pgFileChecksumPageLsnLimit 2
        varLstAdd(paramList, varNewStr(pgFile));            // repoFile
        varLstAdd(paramList, varNewBool(false));            // repoFileHasReference
        varLstAdd(paramList, varNewBool(false));            // repoFileCompress
        varLstAdd(paramList, varNewUInt(1));                // repoFileCompressLevel
        varLstAdd(paramList, varNewStr(backupLabel));       // backupLabel
        varLstAdd(paramList, varNewBool(false));            // delta
        varLstAdd(paramList, NULL);                         // cipherSubPass

        TEST_RESULT_BOOL(
            backupProtocol(PROTOCOL_COMMAND_BACKUP_FILE_STR, paramList, server), true, "protocol backup file - pageChecksum");
        TEST_RESULT_STR(
            strPtr(strNewBuf(serverWrite)),
            "{\"out\":[1,9,9,\"9bc8ab2dda60ef4beed07d1e19ce0676d5edde67\",{\"align\":false,\"valid\":false}]}\n",
            "    check result");
        bufUsedSet(serverWrite, 0);

        // -------------------------------------------------------------------------------------------------------------------------
        // File exists in repo and db, checksum match, delta set, ignoreMissing false, hasReference - NOOP
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, true, false, 1, backupLabel,
                true, cipherTypeNone, NULL),
            "file in db and repo, checksum equal, no ignoreMissing, no pageChecksum, delta, hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "    copy size set");
        TEST_RESULT_UINT(result.repoSize, 0, "    repo size not set since already exists in repo");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultNoOp, "    noop file");
        TEST_RESULT_BOOL(
            (strEqZ(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67") &&
                storageExistsP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    noop");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        // pgFileChecksum, hasReference, delta
        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(pgFile));            // pgFile
        varLstAdd(paramList, varNewBool(false));            // pgFileIgnoreMissing
        varLstAdd(paramList, varNewUInt64(9));              // pgFileSize
        varLstAdd(paramList, varNewStrZ("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"));   // pgFileChecksum
        varLstAdd(paramList, varNewBool(false));            // pgFileChecksumPage
        varLstAdd(paramList, varNewUInt64(0));              // pgFileChecksumPageLsnLimit 1
        varLstAdd(paramList, varNewUInt64(0));              // pgFileChecksumPageLsnLimit 2
        varLstAdd(paramList, varNewStr(pgFile));            // repoFile
        varLstAdd(paramList, varNewBool(true));             // repoFileHasReference
        varLstAdd(paramList, varNewBool(false));            // repoFileCompress
        varLstAdd(paramList, varNewUInt(1));                // repoFileCompressLevel
        varLstAdd(paramList, varNewStr(backupLabel));       // backupLabel
        varLstAdd(paramList, varNewBool(true));             // delta
        varLstAdd(paramList, NULL);                         // cipherSubPass

        TEST_RESULT_BOOL(
            backupProtocol(PROTOCOL_COMMAND_BACKUP_FILE_STR, paramList, server), true, "protocol backup file - noop");
        TEST_RESULT_STR(
            strPtr(strNewBuf(serverWrite)), "{\"out\":[4,9,0,\"9bc8ab2dda60ef4beed07d1e19ce0676d5edde67\",null]}\n",
            "    check result");
        bufUsedSet(serverWrite, 0);

        // -------------------------------------------------------------------------------------------------------------------------
        // File exists in repo and db, pg checksum mismatch, delta set, ignoreMissing false, hasReference - COPY
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, strNew("1234567890123456789012345678901234567890"), false, 0, pgFile, true, false, 1, backupLabel,
                true, cipherTypeNone, NULL),
            "file in db and repo, pg checksum not equal, no ignoreMissing, no pageChecksum, delta, hasReference");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 18, "    copy=repo=pgFile size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "    copy file");
        TEST_RESULT_BOOL(
            (strEqZ(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67") &&
                storageExistsP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    copy");

        // -------------------------------------------------------------------------------------------------------------------------
        // File exists in repo and db, pg checksum same, pg size different, delta set, ignoreMissing false, hasReference - COPY
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 8, strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, true, false, 1, backupLabel,
                true, cipherTypeNone, NULL),
            "db & repo file, pg checksum same, pg size different, no ignoreMissing, no pageChecksum, delta, hasReference");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 18, "    copy=repo=pgFile size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "    copy file");
        TEST_RESULT_BOOL(
            (strEqZ(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67") &&
                storageExistsP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    copy");

        // -------------------------------------------------------------------------------------------------------------------------
        // File exists in repo and db, checksum not same in repo, delta set, ignoreMissing false, no hasReference - RECOPY
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageRepoWrite(), backupPathFile), BUFSTRDEF("adifferentfile")),
            "create different file (size and checksum) with same name in repo");
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, false, false, 1,
                backupLabel, true, cipherTypeNone, NULL),
            "    db & repo file, pgFileMatch, repo checksum no match, no ignoreMissing, no pageChecksum, delta, no hasReference");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 18, "    copy=repo=pgFile size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultReCopy, "    recopy file");
        TEST_RESULT_BOOL(
            (strEqZ(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67") &&
                storageExistsP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    recopy");

        // -------------------------------------------------------------------------------------------------------------------------
        // File exists in repo but missing from db, checksum same in repo, delta set, ignoreMissing true, no hasReference - SKIP
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageRepoWrite(), backupPathFile), BUFSTRDEF("adifferentfile")),
            "create different file with same name in repo");
        TEST_ASSIGN(
            result,
            backupFile(
                missingFile, true, 9, strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, false, false, 1,
                backupLabel, true, cipherTypeNone, NULL),
            "    file in repo only, checksum in repo equal, ignoreMissing=true, no pageChecksum, delta, no hasReference");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 0, "    copy=repo=0 size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultSkip, "    skip file");
        TEST_RESULT_BOOL(
            (result.copyChecksum == NULL && !storageExistsP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    skip and remove file from repo");

        // -------------------------------------------------------------------------------------------------------------------------
        // No prior checksum, compression, no page checksum, no pageChecksum, no delta, no hasReference
        TEST_ASSIGN(
            result,
            backupFile(pgFile, false, 9, NULL, false, 0, pgFile, false, true, 3, backupLabel, false, cipherTypeNone, NULL),
            "pg file exists, no checksum, no ignoreMissing, compression, no pageChecksum, no delta, no hasReference");

        TEST_RESULT_UINT(result.copySize, 9, "    copy=pgFile size");
        TEST_RESULT_UINT(result.repoSize, 29, "    repo compress size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "    copy file");
        TEST_RESULT_BOOL(
            (strEqZ(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67") &&
                storageExistsP(storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strPtr(backupLabel), strPtr(pgFile))) &&
                result.pageChecksumResult == NULL),
            true, "    copy file to repo compress success");

        // -------------------------------------------------------------------------------------------------------------------------
        // Pg and repo file exist & match, prior checksum, compression, no page checksum, no pageChecksum, no delta, no hasReference
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, false, true, 3, backupLabel,
                false, cipherTypeNone, NULL),
            "pg file & repo exists, match, checksum, no ignoreMissing, compression, no pageChecksum, no delta, no hasReference");

        TEST_RESULT_UINT(result.copySize, 9, "    copy=pgFile size");
        TEST_RESULT_UINT(result.repoSize, 29, "    repo compress size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultChecksum, "    checksum file");
        TEST_RESULT_BOOL(
            (strEqZ(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67") &&
                storageExistsP(storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strPtr(backupLabel), strPtr(pgFile))) &&
                result.pageChecksumResult == NULL),
            true, "    compressed repo file matches");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        // compression
        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(pgFile));            // pgFile
        varLstAdd(paramList, varNewBool(false));            // pgFileIgnoreMissing
        varLstAdd(paramList, varNewUInt64(9));              // pgFileSize
        varLstAdd(paramList, varNewStrZ("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"));   // pgFileChecksum
        varLstAdd(paramList, varNewBool(false));            // pgFileChecksumPage
        varLstAdd(paramList, varNewUInt64(0));              // pgFileChecksumPageLsnLimit 1
        varLstAdd(paramList, varNewUInt64(0));              // pgFileChecksumPageLsnLimit 2
        varLstAdd(paramList, varNewStr(pgFile));            // repoFile
        varLstAdd(paramList, varNewBool(false));            // repoFileHasReference
        varLstAdd(paramList, varNewBool(true));             // repoFileCompress
        varLstAdd(paramList, varNewUInt(3));                // repoFileCompressLevel
        varLstAdd(paramList, varNewStr(backupLabel));       // backupLabel
        varLstAdd(paramList, varNewBool(false));            // delta
        varLstAdd(paramList, NULL);                         // cipherSubPass

        TEST_RESULT_BOOL(
            backupProtocol(PROTOCOL_COMMAND_BACKUP_FILE_STR, paramList, server), true, "protocol backup file - copy, compress");
        TEST_RESULT_STR(
            strPtr(strNewBuf(serverWrite)), "{\"out\":[0,9,29,\"9bc8ab2dda60ef4beed07d1e19ce0676d5edde67\",null]}\n",
            "    check result");
        bufUsedSet(serverWrite, 0);

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a zero sized file - checksum will be set but in backupManifestUpdate it will not be copied
        storagePutP(storageNewWriteP(storagePgWrite(), strNew("zerofile")), BUFSTRDEF(""));

        // No prior checksum, no compression, no pageChecksum, no delta, no hasReference
        TEST_ASSIGN(
            result,
            backupFile(
                strNew("zerofile"), false, 0, NULL, false, 0, strNew("zerofile"), false, false, 1, backupLabel, false,
                cipherTypeNone, NULL),
            "zero-sized pg file exists, no repo file, no ignoreMissing, no pageChecksum, no delta, no hasReference");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 0, "    copy=repo=pgFile size 0");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "    copy file");
        TEST_RESULT_PTR_NE(result.copyChecksum, NULL, "    checksum set");
        TEST_RESULT_BOOL(
            (storageExistsP(storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/zerofile", strPtr(backupLabel))) &&
                result.pageChecksumResult == NULL),
            true, "    copy zero file to repo success");

        // Check invalid protocol function
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(backupProtocol(strNew(BOGUS_STR), paramList, server), false, "invalid function");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupFile() - encrypt"))
    {
        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--repo1-cipher-type=aes-256-cbc");
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "12345678", true);
        harnessCfgLoad(cfgCmdBackup, argList);
        unsetenv("PGBACKREST_REPO1_CIPHER_PASS");

        // Create the pg path
        storagePathCreateP(storagePgWrite(), NULL, .mode = 0700);

        // Create a pg file to backup
        storagePutP(storageNewWriteP(storagePgWrite(), pgFile), BUFSTRDEF("atestfile"));

        // -------------------------------------------------------------------------------------------------------------------------
        // No prior checksum, no compression, no pageChecksum, no delta, no hasReference
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, NULL, false, 0, pgFile, false, false, 1, backupLabel, false, cipherTypeAes256Cbc,
                strNew("12345678")),
            "pg file exists, no repo file, no ignoreMissing, no pageChecksum, no delta, no hasReference");

        TEST_RESULT_UINT(result.copySize, 9, "    copy size set");
        TEST_RESULT_UINT(result.repoSize, 32, "    repo size set");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "    copy file");
        TEST_RESULT_BOOL(
            (strEqZ(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67") &&
            storageExistsP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    copy file to encrypted repo success");

        // -------------------------------------------------------------------------------------------------------------------------
        // Delta but pgMatch false (pg File size different), prior checksum, no compression, no pageChecksum, delta, no hasReference
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 8, strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, false, false, 1,
                backupLabel, true, cipherTypeAes256Cbc, strNew("12345678")),
            "pg and repo file exists, pgFileMatch false, no ignoreMissing, no pageChecksum, delta, no hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "    copy size set");
        TEST_RESULT_UINT(result.repoSize, 32, "    repo size set");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "    copy file");
        TEST_RESULT_BOOL(
            (strEqZ(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67") &&
                storageExistsP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    copy file (size missmatch) to encrypted repo success");

        // -------------------------------------------------------------------------------------------------------------------------
        // Check repo with cipher filter.
        // pg/repo file size same but checksum different, prior checksum, no compression, no pageChecksum, no delta, no hasReference
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, strNew("1234567890123456789012345678901234567890"), false, 0, pgFile, false, false, 0,
                backupLabel, false, cipherTypeAes256Cbc, strNew("12345678")),
            "pg and repo file exists, repo checksum no match, no ignoreMissing, no pageChecksum, no delta, no hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "    copy size set");
        TEST_RESULT_UINT(result.repoSize, 32, "    repo size set");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultReCopy, "    recopy file");
        TEST_RESULT_BOOL(
            (strEqZ(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67") &&
                storageExistsP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    recopy file to encrypted repo success");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        // cipherType, cipherPass
        paramList = varLstNew();
        varLstAdd(paramList, varNewStr(pgFile));                // pgFile
        varLstAdd(paramList, varNewBool(false));                // pgFileIgnoreMissing
        varLstAdd(paramList, varNewUInt64(9));                  // pgFileSize
        varLstAdd(paramList, varNewStrZ("1234567890123456789012345678901234567890"));   // pgFileChecksum
        varLstAdd(paramList, varNewBool(false));                // pgFileChecksumPage
        varLstAdd(paramList, varNewUInt64(0));                  // pgFileChecksumPageLsnLimit 1
        varLstAdd(paramList, varNewUInt64(0));                  // pgFileChecksumPageLsnLimit 2
        varLstAdd(paramList, varNewStr(pgFile));                // repoFile
        varLstAdd(paramList, varNewBool(false));                // repoFileHasReference
        varLstAdd(paramList, varNewBool(false));                // repoFileCompress
        varLstAdd(paramList, varNewUInt(0));                    // repoFileCompressLevel
        varLstAdd(paramList, varNewStr(backupLabel));           // backupLabel
        varLstAdd(paramList, varNewBool(false));                // delta
        varLstAdd(paramList, varNewStrZ("12345678"));           // cipherPass

        TEST_RESULT_BOOL(
            backupProtocol(PROTOCOL_COMMAND_BACKUP_FILE_STR, paramList, server), true, "protocol backup file - recopy, encrypt");
        TEST_RESULT_STR(
            strPtr(strNewBuf(serverWrite)), "{\"out\":[2,9,32,\"9bc8ab2dda60ef4beed07d1e19ce0676d5edde67\",null]}\n",
            "    check result");
        bufUsedSet(serverWrite, 0);
    }

    // *****************************************************************************************************************************
    if (testBegin("backupLabelCreate()"))
    {
        const String *pg1Path = strNewFmt("%s/pg1", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        harnessCfgLoad(cfgCmdBackup, argList);

        time_t timestamp = 1575401652;
        String *backupLabel = backupLabelFormat(backupTypeFull, NULL, timestamp);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("label advanced on history conflict");

        storagePutP(
            storageNewWriteP(
                storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/backup.history/2019/%s.manifest.gz", strPtr(backupLabel))),
            NULL);

        TEST_RESULT_STR_Z(backupLabelCreate(backupTypeFull, NULL, timestamp), "20191203-193413F", "create label");

        storageRemoveP(
            storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/backup.history/2019/%s.manifest.gz", strPtr(backupLabel)),
            .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("label not advanced -- no conflict");

        TEST_RESULT_STR_Z(backupLabelCreate(backupTypeFull, NULL, timestamp), "20191203-193412F", "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("label advanced on backup conflict");

        storagePathCreateP(storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s", strPtr(backupLabel)));

        TEST_RESULT_STR_Z(backupLabelCreate(backupTypeFull, NULL, timestamp), "20191203-193413F", "create label");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupInit()"))
    {
        const String *pg1Path = strNewFmt("%s/pg1", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when backup from standby is not supported");

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--" CFGOPT_BACKUP_STANDBY);
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_ERROR(
            backupInit(infoBackupNew(PG_VERSION_91, 1000000000000000910, NULL)), ConfigError,
             "option 'backup-standby' not valid for PostgreSQL < 9.2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("warn and reset when backup from standby used in offline mode");

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 1000000000000000920}));

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--" CFGOPT_BACKUP_STANDBY);
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_RESULT_VOID(backupInit(infoBackupNew(PG_VERSION_92, 1000000000000000920, NULL)), "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptBackupStandby), false, "    check backup-standby");

        TEST_RESULT_LOG(
            "P00   WARN: option backup-standby is enabled but backup is offline - backups will be performed from the primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when pg_control does not match stanza");

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 1000000000000001000}));

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_ERROR(
            backupInit(infoBackupNew(PG_VERSION_11, 1000000000000001100, NULL)), BackupMismatchError,
            "PostgreSQL version 10, system-id 1000000000000001000 do not match stanza version 11, system-id 1000000000000001100");
        TEST_ERROR(
            backupInit(infoBackupNew(PG_VERSION_10, 1000000000000001100, NULL)), BackupMismatchError,
            "PostgreSQL version 10, system-id 1000000000000001000 do not match stanza version 10, system-id 1000000000000001100");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("reset start-fast when PostgreSQL < 8.4");

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_83, .systemId = 1000000000000000830}));

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        strLstAddZ(argList, "--" CFGOPT_START_FAST);
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_RESULT_VOID(backupInit(infoBackupNew(PG_VERSION_83, 1000000000000000830, NULL)), "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptStartFast), false, "    check start-fast");

        TEST_RESULT_LOG("P00   WARN: start-fast option is only available in PostgreSQL >= 8.4");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("reset stop-auto when PostgreSQL < 9.3 or PostgreSQL > 9.5");

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_84, .systemId = 1000000000000000840}));

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        strLstAddZ(argList, "--" CFGOPT_STOP_AUTO);
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_RESULT_VOID(backupInit(infoBackupNew(PG_VERSION_84, 1000000000000000840, NULL)), "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptStopAuto), false, "    check stop-auto");

        TEST_RESULT_LOG("P00   WARN: stop-auto option is only available in PostgreSQL >= 9.3 and <= 9.5");

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 1000000000000000960}));

        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_RESULT_VOID(backupInit(infoBackupNew(PG_VERSION_96, 1000000000000000960, NULL)), "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptStopAuto), false, "    check stop-auto");

        TEST_RESULT_LOG("P00   WARN: stop-auto option is only available in PostgreSQL >= 9.3 and <= 9.5");

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_95, .systemId = 1000000000000000950}));

        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_RESULT_VOID(backupInit(infoBackupNew(PG_VERSION_95, 1000000000000000950, NULL)), "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptStopAuto), true, "    check stop-auto");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("reset checksum-page when the cluster does not have checksums enabled");

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_93, .systemId = PG_VERSION_93}));

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--" CFGOPT_CHECKSUM_PAGE);
        harnessCfgLoad(cfgCmdBackup, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_96, strPtr(pg1Path), false, NULL, NULL),

            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(dbFree(backupInit(infoBackupNew(PG_VERSION_93, PG_VERSION_93, NULL))->dbPrimary), "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "    check checksum-page");

        TEST_RESULT_LOG(
            "P00   WARN: checksum-page option set to true but checksums are not enabled on the cluster, resetting to false");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ok if cluster checksums are enabled and checksum-page is any value");

        // Create pg_control with page checksums
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_93, .systemId = PG_VERSION_93, .pageChecksum = true}));

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--no-" CFGOPT_CHECKSUM_PAGE);
        harnessCfgLoad(cfgCmdBackup, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_96, strPtr(pg1Path), false, NULL, NULL),

            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(dbFree(backupInit(infoBackupNew(PG_VERSION_93, PG_VERSION_93, NULL))->dbPrimary), "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "    check checksum-page");

        // Create pg_control without page checksums
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_93, .systemId = PG_VERSION_93}));

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_96, strPtr(pg1Path), false, NULL, NULL),

            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(dbFree(backupInit(infoBackupNew(PG_VERSION_93, PG_VERSION_93, NULL))->dbPrimary), "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "    check checksum-page");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupTime()"))
    {
        const String *pg1Path = strNewFmt("%s/pg1", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when second does not advance after sleep");

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        harnessCfgLoad(cfgCmdBackup, argList);

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_93, .systemId = PG_VERSION_93}));

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_96, strPtr(pg1Path), false, NULL, NULL),

            // Don't advance time after wait
            HRNPQ_MACRO_TIME_QUERY(1, 1575392588998),
            HRNPQ_MACRO_TIME_QUERY(1, 1575392588999),

            HRNPQ_MACRO_DONE()
        });

        BackupData *backupData = backupInit(infoBackupNew(PG_VERSION_93, PG_VERSION_93, NULL));

        TEST_ERROR(backupTime(backupData, true), AssertError, "invalid sleep for online backup time with wait remainder");
        dbFree(backupData->dbPrimary);
    }

    // *****************************************************************************************************************************
    if (testBegin("backupResumeFind()"))
    {
        const String *repoPath = strNewFmt("%s/repo", testPath());

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAddZ(argList, "--" CFGOPT_PG1_PATH "=/pg");
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        harnessCfgLoad(cfgCmdBackup, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cannot resume empty directory");

        // String *
        storagePathCreateP(storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/20191201F"));

        // TEST_RESULT_PTR(manifestResumeFind((Manifest *)1, NULL, )
    }

    // Offline tests should only be used to test offline functionality and errors easily tested in offline mode
    // *****************************************************************************************************************************
    if (testBegin("cmdBackup() offline"))
    {
        const String *pg1Path = strNewFmt("%s/pg1", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // Replace backup labels since the times are not deterministic
        hrnLogReplaceAdd("[0-9]{8}-[0-9]{6}F_[0-9]{8}-[0-9]{6}I", NULL, "INCR", true);
        hrnLogReplaceAdd("[0-9]{8}-[0-9]{6}F_[0-9]{8}-[0-9]{6}D", NULL, "DIFF", true);
        hrnLogReplaceAdd("[0-9]{8}-[0-9]{6}F", NULL, "FULL", true);

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_84, .systemId = 1000000000000000840}));

        // Create stanza
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        cmdStanzaCreate();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when postmaster.pid exists");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        harnessCfgLoad(cfgCmdBackup, argList);

        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_POSTMASTERPID_STR), BUFSTRDEF("PID"));

        TEST_ERROR(
            cmdBackup(), PostmasterRunningError,
            "--no-online passed but postmaster.pid exists - looks like the postmaster is running. Shutdown the postmaster and try"
                " again, or use --force.");

        TEST_RESULT_LOG("P00   WARN: no prior backup exists, incr backup has been changed to full");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("offline full backup");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        strLstAddZ(argList, "--no-" CFGOPT_COMPRESS);
        strLstAddZ(argList, "--" CFGOPT_FORCE);
        harnessCfgLoad(cfgCmdBackup, argList);

        storagePutP(storageNewWriteP(storagePgWrite(), STRDEF("postgresql.conf")), BUFSTRDEF("CONFIGSTUFF"));

        TEST_RESULT_VOID(cmdBackup(), "backup");

        TEST_RESULT_LOG_FMT(
            "P00   WARN: no prior backup exists, incr backup has been changed to full\n"
            "P00   WARN: --no-online passed and postmaster.pid exists but --force was passed so backup will continue though it"
                " looks like the postmaster is running and the backup will probably not be consistent\n"
            "P01   INFO: backup file {[path]}/pg1/global/pg_control (8KB, 99%%) checksum %s\n"
            "P01   INFO: backup file {[path]}/pg1/postgresql.conf (11B, 100%%) checksum e3db315c260e79211b7b52587123b7aa060f30ab\n"
            "P00   INFO: full backup size = 8KB\n"
            "P00   INFO: new backup label = [FULL-1]",
            TEST_64BIT() ? "21e2ddc99cdf4cfca272eee4f38891146092e358" : "8bb70506d988a8698d9e8cf90736ada23634571b");

        // Remove postmaster.pid
        storageRemoveP(storagePgWrite(), PG_FILE_POSTMASTERPID_STR, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when no files have changed");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        strLstAddZ(argList, "--" CFGOPT_COMPRESS);
        strLstAddZ(argList, "--" CFGOPT_REPO1_HARDLINK);
        strLstAddZ(argList, "--" CFGOPT_TYPE "=" BACKUP_TYPE_DIFF);
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_ERROR(cmdBackup(), FileMissingError, "no files have changed since the last backup - this seems unlikely");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P00   WARN: diff backup cannot alter compress option to 'true', reset to value in [FULL-1]\n"
            "P00   WARN: diff backup cannot alter hardlink option to 'true', reset to value in [FULL-1]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("offline incr backup to test unresumable backup");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        strLstAddZ(argList, "--no-" CFGOPT_COMPRESS);
        strLstAddZ(argList, "--" CFGOPT_CHECKSUM_PAGE);
        strLstAddZ(argList, "--" CFGOPT_TYPE "=" BACKUP_TYPE_INCR);
        harnessCfgLoad(cfgCmdBackup, argList);

        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_PGVERSION_STR), BUFSTRDEF("VER"));

        TEST_RESULT_VOID(cmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P00   WARN: incr backup cannot alter 'checksum-page' option to 'true', reset to 'false' from [FULL-1]\n"
            "P00   WARN: backup '[DIFF-1]' cannot be resumed: new backup type 'incr' does not match resumable backup type 'diff'\n"
            "P01   INFO: backup file {[path]}/pg1/PG_VERSION (3B, 100%) checksum c8663c2525f44b6d9c687fbceb4aafc63ed8b451\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-1]\n"
            "P00 DETAIL: reference pg_data/postgresql.conf to [FULL-1]\n"
            "P00   INFO: incr backup size = 3B\n"
            "P00   INFO: new backup label = [INCR-1]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("offline diff backup to test prior backup must be full");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        strLstAddZ(argList, "--no-" CFGOPT_COMPRESS);
        strLstAddZ(argList, "--" CFGOPT_TYPE "=" BACKUP_TYPE_DIFF);
        harnessCfgLoad(cfgCmdBackup, argList);

        sleepMSec(MSEC_PER_SEC - (timeMSec() % MSEC_PER_SEC));
        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_PGVERSION_STR), BUFSTRDEF("VR2"));

        TEST_RESULT_VOID(cmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P01   INFO: backup file {[path]}/pg1/PG_VERSION (3B, 100%) checksum 6f1894088c578e4f0b9888e8e8a997d93cbbc0c5\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-1]\n"
            "P00 DETAIL: reference pg_data/postgresql.conf to [FULL-1]\n"
            "P00   INFO: diff backup size = 3B\n"
            "P00   INFO: new backup label = [DIFF-2]");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdBackup() online"))
    {
        const String *pg1Path = strNewFmt("%s/pg1", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());
        const String *pg2Path = strNewFmt("%s/pg2", testPath());

        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // Replace percent complete and backup size since they can cause a lot of churn when files are added/removed
        hrnLogReplaceAdd(", [0-9]{1,3}%\\)", "[0-9]+%", "PCT", false);
        hrnLogReplaceAdd(" backup size = [0-9]+[A-Z]+", "[^ ]+$", "SIZE", false);

        // Replace checksums since they can differ between architectures (e.g. 32/64 bit)
        hrnLogReplaceAdd("\\) checksum [a-f0-9]{40}", "[a-f0-9]{40}$", "SHA1", false);

        // Backup start time epoch.  The idea is to not have backup times (and therefore labels) ever change.  Each backup added
        // should be separated by 100,000 seconds (1,000,000 after stanza-upgrade) but after the initial assignments this will only
        // be possible at the beginning and the end, so new backups added in the middle will average the start times of the prior
        // and next backup to get their start time.  Backups added to the beginning of the test will need to subtract from the
        // epoch.
        #define BACKUP_EPOCH                                        1570000000

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 9.5 resume uncompressed full backup");

        time_t backupTimeStart = BACKUP_EPOCH;

        {
            // Create pg_control
            storagePutP(
                storageNewWriteP(
                    storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path)),
                    .timeModified = backupTimeStart),
                pgControlTestToBuffer((PgControl){.version = PG_VERSION_95, .systemId = 1000000000000000950}));

            // Create stanza
            StringList *argList = strLstNew();
            strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
            strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
            strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
            strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
            harnessCfgLoad(cfgCmdStanzaCreate, argList);

            cmdStanzaCreate();

            // Load options
            argList = strLstNew();
            strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
            strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
            strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
            strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
            strLstAddZ(argList, "--" CFGOPT_TYPE "=" BACKUP_TYPE_FULL);
            strLstAddZ(argList, "--no-" CFGOPT_COMPRESS);
            harnessCfgLoad(cfgCmdBackup, argList);

            // Add files
            storagePutP(
                storageNewWriteP(storagePgWrite(), STRDEF("postgresql.conf"), .timeModified = backupTimeStart),
                BUFSTRDEF("CONFIGSTUFF"));
            storagePutP(
                storageNewWriteP(storagePgWrite(), PG_FILE_PGVERSION_STR, .timeModified = backupTimeStart),
                BUFSTRDEF(PG_VERSION_95_STR));

            // Create a backup manifest that looks like a halted backup manifest
            Manifest *manifestResume = manifestNewBuild(storagePg(), PG_VERSION_95, true, false, NULL, NULL);
            ManifestData *manifestResumeData = (ManifestData *)manifestData(manifestResume);

            manifestResumeData->backupType = backupTypeFull;
            const String *resumeLabel = backupLabelCreate(backupTypeFull, NULL, backupTimeStart);
            manifestBackupLabelSet(manifestResume, resumeLabel);

            // Copy a file to be resumed that has not changed in the repo
            storageCopy(
                storageNewReadP(storagePg(), PG_FILE_PGVERSION_STR),
                storageNewWriteP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/PG_VERSION", strPtr(resumeLabel))));

            strcpy(
                ((ManifestFile *)manifestFileFind(manifestResume, STRDEF("pg_data/PG_VERSION")))->checksumSha1,
                "06d06bb31b570b94d7b4325f511f853dbe771c21");

            // Save the resume manifest
            manifestSave(
                manifestResume,
                storageWriteIo(
                    storageNewWriteP(
                        storageRepoWrite(),
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strPtr(resumeLabel)))));

            // Run backup
            testBackupPqScriptP(PG_VERSION_95, backupTimeStart);
            TEST_RESULT_VOID(cmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: execute exclusive pg_start_backup(): backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 000000010000000000000000, lsn = 0/1\n"
                "P00   WARN: resumable backup 20191002-070640F of same type exists -- remove invalid files and resume\n"
                "P01   INFO: backup file {[path]}/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01   INFO: backup file {[path]}/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: checksum resumed file {[path]}/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
                "P00   INFO: full backup size = [SIZE]\n"
                "P00   INFO: execute exclusive pg_stop_backup() and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 000000010000000000000001, lsn = 0/1000001\n"
                "P00   INFO: new backup label = 20191002-070640F");

            testBackupCompare(
                storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest/pg_data"), false,
                "PG_VERSION {file, s=3}\n"
                "global {path}\n"
                "global/pg_control {file, s=8192}\n"
                "postgresql.conf {file, s=11}\n");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online resumed compressed 9.5 full backup");

        // Backup start time
        backupTimeStart = BACKUP_EPOCH + 100000;

        {
            // Load options
            StringList *argList = strLstNew();
            strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
            strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
            strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
            strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
            strLstAddZ(argList, "--" CFGOPT_TYPE "=" BACKUP_TYPE_FULL);
            harnessCfgLoad(cfgCmdBackup, argList);

            // Create a backup manifest that looks like a halted backup manifest
            Manifest *manifestResume = manifestNewBuild(storagePg(), PG_VERSION_95, true, false, NULL, NULL);
            ManifestData *manifestResumeData = (ManifestData *)manifestData(manifestResume);

            manifestResumeData->backupType = backupTypeFull;
            const String *resumeLabel = backupLabelCreate(backupTypeFull, NULL, backupTimeStart);
            manifestBackupLabelSet(manifestResume, resumeLabel);

            // File exists in cluster and repo but not in the resume manifest
            storagePutP(
                storageNewWriteP(storagePgWrite(), STRDEF("not-in-resume"), .timeModified = backupTimeStart), BUFSTRDEF("TEST"));
            storagePutP(
                storageNewWriteP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/not-in-resume.gz", strPtr(resumeLabel))),
                NULL);

            // Remove checksum from file so it won't be resumed
            storagePutP(
                storageNewWriteP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/global/pg_control.gz", strPtr(resumeLabel))),
                NULL);

            ((ManifestFile *)manifestFileFind(manifestResume, STRDEF("pg_data/global/pg_control")))->checksumSha1[0] = 0;

            // Size does not match between cluster and resume manifest
            storagePutP(
                storageNewWriteP(storagePgWrite(), STRDEF("size-mismatch"), .timeModified = backupTimeStart), BUFSTRDEF("TEST"));
            storagePutP(
                storageNewWriteP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/size-mismatch.gz", strPtr(resumeLabel))),
                NULL);
            manifestFileAdd(
                manifestResume, &(ManifestFile){
                    .name = STRDEF("pg_data/size-mismatch"), .checksumSha1 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", .size = 33});

            // Time does not match between cluster and resume manifest
            storagePutP(
                storageNewWriteP(storagePgWrite(), STRDEF("time-mismatch"), .timeModified = backupTimeStart), BUFSTRDEF("TEST"));
            storagePutP(
                storageNewWriteP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/time-mismatch.gz", strPtr(resumeLabel))),
                NULL);
            manifestFileAdd(
                manifestResume, &(ManifestFile){
                    .name = STRDEF("pg_data/time-mismatch"), .checksumSha1 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", .size = 4,
                    .timestamp = backupTimeStart - 1});

            // Size is zero in cluster and resume manifest. ??? We'd like to remove this requirement after the migration.
            storagePutP(storageNewWriteP(storagePgWrite(), STRDEF("zero-size"), .timeModified = backupTimeStart), NULL);
            storagePutP(
                storageNewWriteP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/zero-size.gz", strPtr(resumeLabel))),
                BUFSTRDEF("ZERO-SIZE"));
            manifestFileAdd(
                manifestResume, &(ManifestFile){.name = STRDEF("pg_data/zero-size"), .size = 0, .timestamp = backupTimeStart});

            // Path is not in manifest
            storagePathCreateP(storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/bogus_path", strPtr(resumeLabel)));

            // File is not in manifest
            storagePutP(
                storageNewWriteP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/global/bogus", strPtr(resumeLabel))),
                NULL);

            // Save the resume manifest
            manifestSave(
                manifestResume,
                storageWriteIo(
                    storageNewWriteP(
                        storageRepoWrite(),
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strPtr(resumeLabel)))));

            // Run backup
            testBackupPqScriptP(PG_VERSION_95, backupTimeStart);
            TEST_RESULT_VOID(cmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: execute exclusive pg_start_backup(): backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 000000010000000000000000, lsn = 0/1\n"
                "P00   WARN: resumable backup 20191003-105320F of same type exists -- remove invalid files and resume\n"
                "P00 DETAIL: remove path '{[path]}/repo/backup/test1/20191003-105320F/pg_data/bogus_path' from resumed backup\n"
                "P00 DETAIL: remove file '{[path]}/repo/backup/test1/20191003-105320F/pg_data/global/bogus' from resumed backup"
                    " (missing in manifest)\n"
                "P00 DETAIL: remove file '{[path]}/repo/backup/test1/20191003-105320F/pg_data/global/pg_control.gz' from resumed"
                    " backup (no checksum in resumed manifest)\n"
                "P00 DETAIL: remove file '{[path]}/repo/backup/test1/20191003-105320F/pg_data/not-in-resume.gz' from resumed backup"
                    " (missing in resumed manifest)\n"
                "P00 DETAIL: remove file '{[path]}/repo/backup/test1/20191003-105320F/pg_data/size-mismatch.gz' from resumed backup"
                    " (mismatched size)\n"
                "P00 DETAIL: remove file '{[path]}/repo/backup/test1/20191003-105320F/pg_data/time-mismatch.gz' from resumed backup"
                    " (mismatched timestamp)\n"
                "P00 DETAIL: remove file '{[path]}/repo/backup/test1/20191003-105320F/pg_data/zero-size.gz' from resumed backup"
                    " (zero size)\n"
                "P01   INFO: backup file {[path]}/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01   INFO: backup file {[path]}/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01   INFO: backup file {[path]}/pg1/time-mismatch (4B, [PCT]) checksum [SHA1]\n"
                "P01   INFO: backup file {[path]}/pg1/size-mismatch (4B, [PCT]) checksum [SHA1]\n"
                "P01   INFO: backup file {[path]}/pg1/not-in-resume (4B, [PCT]) checksum [SHA1]\n"
                "P01   INFO: backup file {[path]}/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
                "P01   INFO: backup file {[path]}/pg1/zero-size (0B, [PCT])\n"
                "P00   INFO: full backup size = [SIZE]\n"
                "P00   INFO: execute exclusive pg_stop_backup() and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 000000010000000000000001, lsn = 0/1000001\n"
                "P00   INFO: new backup label = 20191003-105320F");

            testBackupCompare(
                storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest/pg_data"), true,
                "PG_VERSION.gz {file, s=3}\n"
                "global {path}\n"
                "global/pg_control.gz {file, s=8192}\n"
                "not-in-resume.gz {file, s=4}\n"
                "postgresql.conf.gz {file, s=11}\n"
                "size-mismatch.gz {file, s=4}\n"
                "time-mismatch.gz {file, s=4}\n"
                "zero-size.gz {file, s=0}\n");

            // Remove files used to test resume
            storageRemoveP(storagePgWrite(), STRDEF("not-in-resume"), .errorOnMissing = true);
            storageRemoveP(storagePgWrite(), STRDEF("size-mismatch"), .errorOnMissing = true);
            storageRemoveP(storagePgWrite(), STRDEF("time-mismatch"), .errorOnMissing = true);
            storageRemoveP(storagePgWrite(), STRDEF("zero-size"), .errorOnMissing = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online resumed compressed 9.5 diff backup");

        backupTimeStart = BACKUP_EPOCH + 200000;

        {
            StringList *argList = strLstNew();
            strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
            strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
            strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
            strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
            strLstAddZ(argList, "--" CFGOPT_TYPE "=" BACKUP_TYPE_DIFF);
            harnessCfgLoad(cfgCmdBackup, argList);

            // Load the previous manifest and null out the checksum-page option to be sure it gets set to false in this backup
            const String *manifestPriorFile = STRDEF(STORAGE_REPO_BACKUP "/latest/" BACKUP_MANIFEST_FILE);
            Manifest *manifestPrior = manifestNewLoad(storageReadIo(storageNewReadP(storageRepo(), manifestPriorFile)));
            ((ManifestData *)manifestData(manifestPrior))->backupOptionChecksumPage = NULL;
            manifestSave(manifestPrior, storageWriteIo(storageNewWriteP(storageRepoWrite(), manifestPriorFile)));

            // Create a backup manifest that looks like a halted backup manifest
            Manifest *manifestResume = manifestNewBuild(storagePg(), PG_VERSION_95, true, false, NULL, NULL);
            ManifestData *manifestResumeData = (ManifestData *)manifestData(manifestResume);

            manifestResumeData->backupType = backupTypeDiff;
            manifestResumeData->backupLabelPrior = manifestData(manifestPrior)->backupLabel;
            const String *resumeLabel = backupLabelCreate(backupTypeDiff, manifestData(manifestPrior)->backupLabel, backupTimeStart);
            manifestBackupLabelSet(manifestResume, resumeLabel);

            // Reference in manifest
            storagePutP(
                storageNewWriteP(storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/PG_VERSION.gz", strPtr(resumeLabel))),
                NULL);

            // Reference in resumed manifest
            storagePutP(storageNewWriteP(storagePgWrite(), STRDEF("resume-ref"), .timeModified = backupTimeStart), NULL);
            storagePutP(
                storageNewWriteP(storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/resume-ref.gz", strPtr(resumeLabel))),
                NULL);
            manifestFileAdd(
                manifestResume, &(ManifestFile){.name = STRDEF("pg_data/resume-ref"), .size = 0, .reference = STRDEF("BOGUS")});

            // Time does not match between cluster and resume manifest (but resume because time is in future so delta enabled).  Note
            // also that the repo file is intenionally corrupt to generate a warning about corruption in the repository.
            storagePutP(
                storageNewWriteP(storagePgWrite(), STRDEF("time-mismatch2"), .timeModified = backupTimeStart + 100), BUFSTRDEF("TEST"));
            storagePutP(
                storageNewWriteP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/time-mismatch2.gz", strPtr(resumeLabel))),
                NULL);
            manifestFileAdd(
                manifestResume, &(ManifestFile){
                    .name = STRDEF("pg_data/time-mismatch2"), .checksumSha1 = "984816fd329622876e14907634264e6f332e9fb3", .size = 4,
                    .timestamp = backupTimeStart});

            // Links are always removed on resume
            THROW_ON_SYS_ERROR(
                symlink(
                    "..",
                    strPtr(storagePathP(storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/link", strPtr(resumeLabel))))) == -1,
                FileOpenError, "unable to create symlink");

            // Special files should not be in the repo
            TEST_SYSTEM_FMT(
                "mkfifo -m 666 %s",
                strPtr(storagePathP(storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/pipe", strPtr(resumeLabel)))));

            // Save the resume manifest
            manifestSave(
                manifestResume,
                storageWriteIo(
                    storageNewWriteP(
                        storageRepoWrite(),
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strPtr(resumeLabel)))));

            // Run backup
            testBackupPqScriptP(PG_VERSION_95, backupTimeStart);
            TEST_RESULT_VOID(cmdBackup(), "backup");

            // Check log
            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191003-105320F, version = " PROJECT_VERSION "\n"
                "P00   INFO: execute exclusive pg_start_backup(): backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 000000010000000000000000, lsn = 0/1\n"
                "P00   WARN: file 'time-mismatch2' has timestamp in the future, enabling delta checksum\n"
                "P00   WARN: resumable backup 20191003-105320F_20191004-144000D of same type exists"
                    " -- remove invalid files and resume\n"
                "P00 DETAIL: remove file '{[path]}/repo/backup/test1/20191003-105320F_20191004-144000D/pg_data/PG_VERSION.gz'"
                    " from resumed backup (reference in manifest)\n"
                "P00   WARN: remove special file '{[path]}/repo/backup/test1/20191003-105320F_20191004-144000D/pg_data/pipe'"
                    " from resumed backup\n"
                "P00 DETAIL: remove file '{[path]}/repo/backup/test1/20191003-105320F_20191004-144000D/pg_data/resume-ref.gz'"
                    " from resumed backup (reference in resumed manifest)\n"
                "P01 DETAIL: match file from prior backup {[path]}/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup {[path]}/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P00   WARN: resumed backup file pg_data/time-mismatch2 does not have expected checksum"
                    " 984816fd329622876e14907634264e6f332e9fb3. The file will be recopied and backup will continue but this may be an"
                    " issue unless the resumed backup path in the repository is known to be corrupted.\n"
                "            NOTE: this does not indicate a problem with the PostgreSQL page checksums.\n"
                "P01   INFO: backup file {[path]}/pg1/time-mismatch2 (4B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup {[path]}/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
                "P01   INFO: backup file {[path]}/pg1/resume-ref (0B, [PCT])\n"
                "P00 DETAIL: reference pg_data/PG_VERSION to 20191003-105320F\n"
                "P00 DETAIL: reference pg_data/global/pg_control to 20191003-105320F\n"
                "P00 DETAIL: reference pg_data/postgresql.conf to 20191003-105320F\n"
                "P00   INFO: diff backup size = [SIZE]\n"
                "P00   INFO: execute exclusive pg_stop_backup() and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 000000010000000000000001, lsn = 0/1000001\n"
                "P00   INFO: new backup label = 20191003-105320F_20191004-144000D");

            // Check repo directory
            testBackupCompare(
                storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest/pg_data"), true,
                "resume-ref.gz {file, s=0}\n"
                "time-mismatch2.gz {file, s=4}\n");

            storageRemoveP(storagePgWrite(), STRDEF("resume-ref"), .errorOnMissing = true);
            storageRemoveP(storagePgWrite(), STRDEF("time-mismatch2"), .errorOnMissing = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 9.6 back-standby full backup");

        backupTimeStart = BACKUP_EPOCH + 1200000;

        {
            // Update pg_control
            storagePutP(
                storageNewWriteP(
                    storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path)),
                    .timeModified = backupTimeStart),
                pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 1000000000000000960}));

            // Update version
            storagePutP(
                storageNewWriteP(storagePgWrite(), PG_FILE_PGVERSION_STR, .timeModified = backupTimeStart),
                BUFSTRDEF(PG_VERSION_96_STR));

            // Upgrade stanza
            StringList *argList = strLstNew();
            strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
            strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
            strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
            strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
            harnessCfgLoad(cfgCmdStanzaUpgrade, argList);

            cmdStanzaUpgrade();

            // Load options
            argList = strLstNew();
            strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
            strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
            strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
            strLstAdd(argList, strNewFmt("--" CFGOPT_PG2_PATH "=%s", strPtr(pg2Path)));
            strLstAddZ(argList, "--" CFGOPT_PG2_PORT "=5433");
            strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
            strLstAddZ(argList, "--no-" CFGOPT_COMPRESS);
            strLstAddZ(argList, "--" CFGOPT_BACKUP_STANDBY);
            strLstAddZ(argList, "--" CFGOPT_START_FAST);
            harnessCfgLoad(cfgCmdBackup, argList);

            // Create files to copy from the standby.  The files will be zero-size on the primary and non-zero on the standby to test
            // that they were copied from the right place.
            storagePutP(storageNewWriteP(storagePgIdWrite(1), STRDEF(PG_PATH_BASE "/1/1"), .timeModified = backupTimeStart), NULL);
            storagePutP(storageNewWriteP(storagePgIdWrite(2), STRDEF(PG_PATH_BASE "/1/1")), BUFSTRDEF("DATA"));

            // Set log level to warn because the following test uses multiple processes so the log order will not be deterministic
            harnessLogLevelSet(logLevelWarn);

            // Run backup
            testBackupPqScriptP(PG_VERSION_96, backupTimeStart, .backupStandby = true);
            TEST_RESULT_VOID(cmdBackup(), "backup");

            // Set log level back to detail
            harnessLogLevelSet(logLevelDetail);

            TEST_RESULT_LOG(
                "P00   WARN: no prior backup exists, incr backup has been changed to full");

            testBackupCompare(
                storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest/pg_data"), false,
                "PG_VERSION {file, s=3}\n"
                "backup_label {file, s=17}\n"
                "base {path}\n"
                "base/1 {path}\n"
                "base/1/1 {file, s=4}\n"
                "global {path}\n"
                "global/pg_control {file, s=8192}\n"
                "postgresql.conf {file, s=11}\n");
        }
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
