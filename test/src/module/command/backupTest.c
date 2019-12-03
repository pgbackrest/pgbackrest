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
    if (testBegin("cmdBackup()"))
    {
        const String *pg1Path = strNewFmt("%s/pg1", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // Add log replacements
        hrnLogReplaceAdd("[0-9]{8}-[0-9]{6}F_[0-9]{8}-[0-9]{6}I", NULL, "INCR", true);
        hrnLogReplaceAdd("[0-9]{8}-[0-9]{6}F_[0-9]{8}-[0-9]{6}D", NULL, "DIFF", true);
        hrnLogReplaceAdd("[0-9]{8}-[0-9]{6}F", NULL, "FULL", true);
        hrnLogReplaceAdd(", [0-9]{1,3}%\\)", "[0-9]+%", "PCT", false);
        hrnLogReplaceAdd("\\) checksum [a-f0-9]{40}", "[a-f0-9]{40}$", "SHA1", false);

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
        TEST_TITLE("offline full backup (changed from incr)");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        strLstAddZ(argList, "--no-" CFGOPT_COMPRESS);
        strLstAddZ(argList, "--" CFGOPT_FORCE);
        harnessCfgLoad(cfgCmdBackup, argList);

        storagePutP(
            storageNewWriteP(storagePgWrite(), STRDEF("postgresql.conf")), BUFSTRDEF("CONFIGSTUFF"));

        TEST_RESULT_VOID(cmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   WARN: no prior backup exists, incr backup has been changed to full\n"
            "P00   WARN: --no-online passed and postmaster.pid exists but --force was passed so backup will continue though it"
                " looks like the postmaster is running and the backup will probably not be consistent\n"
            "P01   INFO: backup file {[path]}/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
            "P01   INFO: backup file {[path]}/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
            "P00   INFO: full backup size = 8KB\n"
            "P00   INFO: new backup label = [FULL-1]");

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
        harnessCfgLoad(cfgCmdBackup, argList);

        TEST_ERROR(cmdBackup(), FileMissingError, "no files have changed since the last backup - this seems unlikely");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P00   WARN: incr backup cannot alter compress option to 'true', reset to value in [FULL-1]\n"
            "P00   WARN: incr backup cannot alter hardlink option to 'true', reset to value in [FULL-1]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("offline diff backup");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        strLstAddZ(argList, "--no-" CFGOPT_COMPRESS);
        strLstAddZ(argList, "--" CFGOPT_CHECKSUM_PAGE);
        strLstAddZ(argList, "--" CFGOPT_TYPE "=" BACKUP_TYPE_DIFF);
        harnessCfgLoad(cfgCmdBackup, argList);

        storagePutP(storageNewWriteP(storagePgWrite(), PG_FILE_PGVERSION_STR), BUFSTRDEF("VER"));

        TEST_RESULT_VOID(cmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P00   WARN: diff backup cannot alter 'checksum-page' option to 'true', reset to 'false' from [FULL-1]\n"
            "P00   WARN: backup '[INCR-1]' cannot be resumed: new backup type 'diff' does not match resumable backup type 'incr'\n"
            "P01   INFO: backup file /home/vagrant/test/test-0/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-1]\n"
            "P00 DETAIL: reference pg_data/postgresql.conf to [FULL-1]\n"
            "P00   INFO: diff backup size = 3B\n"
            "P00   INFO: new backup label = [DIFF-1]");

        // -------------------------------------------------------------------------------------------------------------------------
        // Update pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_95, .systemId = 1000000000000000950}));

        // Upgrade stanza
        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        harnessCfgLoad(cfgCmdStanzaUpgrade, argList);

        cmdStanzaUpgrade();

        // We've tested backup size enough so add a replacement to reduce log churn
        hrnLogReplaceAdd(" backup size = [0-9]+[A-Z]+", "[^ ]+$", "SIZE", false);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 9.5 full backup");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        harnessCfgLoad(cfgCmdBackup, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_95, strPtr(pg1Path), false, NULL, NULL),

            // Get start time
            HRNPQ_MACRO_TIME_QUERY(1, 1575392544000),

            // Start backup
            HRNPQ_MACRO_ADVISORY_LOCK(1, true),
            HRNPQ_MACRO_START_BACKUP_84_95(1, "0/1", "000000010000000000000000"),
            HRNPQ_MACRO_DATABASE_LIST_1(1, "test1"),
            HRNPQ_MACRO_TABLESPACE_LIST_0(1),

            // Get copy start time
            HRNPQ_MACRO_TIME_QUERY(1, 1575392544999),
            HRNPQ_MACRO_TIME_QUERY(1, 1575392545000),

            // Stop backup
            HRNPQ_MACRO_STOP_BACKUP_LE_95(1, "0/1000001", "000000010000000000000001"),

            // Get stop time
            HRNPQ_MACRO_TIME_QUERY(1, 1575392550000),

            // Close primary connection
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   WARN: no prior backup exists, incr backup has been changed to full\n"
            "P00   INFO: execute exclusive pg_start_backup(): backup begins after the next regular checkpoint completes\n"
            "P00   INFO: backup start archive = 000000010000000000000000, lsn = 0/1\n"
            "P00   WARN: file 'PG_VERSION' has timestamp in the future, enabling delta checksum\n"
            "P01   INFO: backup file /home/vagrant/test/test-0/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
            "P01   INFO: backup file /home/vagrant/test/test-0/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
            "P01   INFO: backup file /home/vagrant/test/test-0/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
            "P00   INFO: full backup size = [SIZE]\n"
            "P00   INFO: execute exclusive pg_stop_backup() and wait for all WAL segments to archive\n"
            "P00   INFO: backup stop archive = 000000010000000000000001, lsn = 0/1000001\n"
            "P00   INFO: new backup label = [FULL-2]");

        // -------------------------------------------------------------------------------------------------------------------------
        // Update pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pg1Path))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 1000000000000000960}));

        // Upgrade stanza
        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--no-" CFGOPT_ONLINE);
        harnessCfgLoad(cfgCmdStanzaUpgrade, argList);

        cmdStanzaUpgrade();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 9.6 full backup");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--" CFGOPT_PG1_PATH "=%s", strPtr(pg1Path)));
        strLstAddZ(argList, "--" CFGOPT_REPO1_RETENTION_FULL "=1");
        harnessCfgLoad(cfgCmdBackup, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_96, strPtr(pg1Path), false, NULL, NULL),

            // Get start time
            HRNPQ_MACRO_TIME_QUERY(1, 1575392588000),

            // Start backup
            HRNPQ_MACRO_ADVISORY_LOCK(1, true),
            HRNPQ_MACRO_START_BACKUP_GE_96(1, "0/1", "000000010000000000000000"),
            HRNPQ_MACRO_DATABASE_LIST_1(1, "test1"),
            HRNPQ_MACRO_TABLESPACE_LIST_0(1),

            // Get copy start time
            HRNPQ_MACRO_TIME_QUERY(1, 1575392588999),
            HRNPQ_MACRO_TIME_QUERY(1, 1575392589000),

            // Stop backup
            HRNPQ_MACRO_STOP_BACKUP_96(1, "0/1000001", "000000010000000000000001"),
            HRNPQ_MACRO_TIME_QUERY(1, 1575392590000),

            // Get stop time
            HRNPQ_MACRO_TIME_QUERY(1, 1575392590000),

            // Close primary connection
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   WARN: no prior backup exists, incr backup has been changed to full\n"
            "P00   INFO: execute non-exclusive pg_start_backup(): backup begins after the next regular checkpoint completes\n"
            "P00   INFO: backup start archive = 000000010000000000000000, lsn = 0/1\n"
            "P00   WARN: file 'PG_VERSION' has timestamp in the future, enabling delta checksum\n"
            "P01   INFO: backup file /home/vagrant/test/test-0/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
            "P01   INFO: backup file /home/vagrant/test/test-0/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
            "P01   INFO: backup file /home/vagrant/test/test-0/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
            "P00   INFO: full backup size = [SIZE]\n"
            "P00   INFO: execute non-exclusive pg_stop_backup() and wait for all WAL segments to archive\n"
            "P00 DETAIL: wrote 'backup_label' file returned from pg_stop_backup()\n"
            "P00   INFO: backup stop archive = 000000010000000000000001, lsn = 0/1000001\n"
            "P00   INFO: new backup label = [FULL-3]");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
