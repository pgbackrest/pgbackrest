/***********************************************************************************************************************************
Test Backup Command
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/io.h"
#include "storage/helper.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

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
        storagePutNP(storageNewWriteNP(storagePgWrite(), pgFile), BUFSTRDEF("atestfile"));

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
                storageExistsNP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    copy file to repo success");

        TEST_RESULT_VOID(storageRemoveNP(storageRepoWrite(), backupPathFile), "    remove repo file");

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
                storageExistsNP(storageRepo(), backupPathFile)),
            true,"    copy file to repo success");
        TEST_RESULT_PTR_NE(result.pageChecksumResult, NULL, "    pageChecksumResult is set");
        TEST_RESULT_BOOL(
            varBool(kvGet(result.pageChecksumResult, VARSTRDEF("valid"))), false, "    pageChecksumResult valid=false");
        TEST_RESULT_VOID(storageRemoveNP(storageRepoWrite(), backupPathFile), "    remove repo file");

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
                storageExistsNP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
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
                storageExistsNP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
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
                storageExistsNP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    copy");

        // -------------------------------------------------------------------------------------------------------------------------
        // File exists in repo and db, checksum not same in repo, delta set, ignoreMissing false, no hasReference - RECOPY
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageRepoWrite(), backupPathFile), BUFSTRDEF("adifferentfile")),
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
                storageExistsNP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
            true, "    recopy");

        // -------------------------------------------------------------------------------------------------------------------------
        // File exists in repo but missing from db, checksum same in repo, delta set, ignoreMissing true, no hasReference - SKIP
        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageRepoWrite(), backupPathFile), BUFSTRDEF("adifferentfile")),
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
            (result.copyChecksum == NULL && !storageExistsNP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
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
                storageExistsNP(storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strPtr(backupLabel), strPtr(pgFile))) &&
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
                storageExistsNP(storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strPtr(backupLabel), strPtr(pgFile))) &&
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

        TEST_RESULT_BOOL(
            backupProtocol(PROTOCOL_COMMAND_BACKUP_FILE_STR, paramList, server), true, "protocol backup file - copy, compress");
        TEST_RESULT_STR(
            strPtr(strNewBuf(serverWrite)), "{\"out\":[0,9,29,\"9bc8ab2dda60ef4beed07d1e19ce0676d5edde67\",null]}\n",
            "    check result");
        bufUsedSet(serverWrite, 0);

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a zero sized file - checksum will be set but in backupManifestUpdate it will not be copied
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("zerofile")), BUFSTRDEF(""));

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
            (storageExistsNP(storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/zerofile", strPtr(backupLabel))) &&
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
        storagePutNP(storageNewWriteNP(storagePgWrite(), pgFile), BUFSTRDEF("atestfile"));

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
            storageExistsNP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
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
                storageExistsNP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
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
                storageExistsNP(storageRepo(), backupPathFile) && result.pageChecksumResult == NULL),
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
        // const String *pgPath = strNewFmt("%s/pg", testPath());
        // const String *repoPath = strNewFmt("%s/repo", testPath());
        //
        // // -------------------------------------------------------------------------------------------------------------------------
        // TEST_TITLE("PLACEHOLDER TEST");
        //
        // StringList *argList = strLstNew();
        // strLstAddZ(argList, "pgbackrest");
        // strLstAddZ(argList, "--stanza=test1");
        // strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        // strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        // strLstAddZ(argList, "--repo1-retention-full=1");
        // strLstAddZ(argList, "backup");
        // harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        //
        // TEST_RESULT_VOID(cmdBackup(), "backup");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
