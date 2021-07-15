/***********************************************************************************************************************************
Test Backup Command
***********************************************************************************************************************************/
#include "command/stanza/create.h"
#include "command/stanza/upgrade.h"
#include "common/crypto/hash.h"
#include "common/io/bufferWrite.h"
#include "postgres/interface/static.vendor.h"
#include "storage/helper.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessPostgres.h"
#include "common/harnessPq.h"
#include "common/harnessProtocol.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Get a list of all files in the backup and a redacted version of the manifest that can be tested against a static string
***********************************************************************************************************************************/
typedef struct TestBackupValidateCallbackData
{
    const Storage *storage;                                         // Storage object when needed (e.g. fileCompressed = true)
    const String *path;                                             // Subpath when storage is specified
    const Manifest *manifest;                                       // Manifest to check for files/links/paths
    const ManifestData *manifestData;                               // Manifest data
    String *content;                                                // String where content should be added
} TestBackupValidateCallbackData;

void
testBackupValidateCallback(void *callbackData, const StorageInfo *info)
{
    TestBackupValidateCallbackData *data = callbackData;

    // Don't include . when it is a path (we'll still include it when it is a link so we can see the destination)
    if (info->type == storageTypePath && strEq(info->name, DOT_STR))
        return;

    // Don't include backup.manifest or copy.  We'll test that they are present elsewhere
    if (info->type == storageTypeFile &&
        (strEqZ(info->name, BACKUP_MANIFEST_FILE) || strEqZ(info->name, BACKUP_MANIFEST_FILE INFO_COPY_EXT)))
        return;

    // Get manifest name
    const String *manifestName = info->name;

    strCatFmt(data->content, "%s {", strZ(info->name));

    switch (info->type)
    {
        case storageTypeFile:
        {
            strCatZ(data->content, "file");

            // Calculate checksum/size and decompress if needed
            // ---------------------------------------------------------------------------------------------------------------------
            StorageRead *read = storageNewReadP(
                data->storage, data->path != NULL ? strNewFmt("%s/%s", strZ(data->path), strZ(info->name)) : info->name);

            if (data->manifestData->backupOptionCompressType != compressTypeNone)
            {
                ioFilterGroupAdd(
                    ioReadFilterGroup(storageReadIo(read)), decompressFilter(data->manifestData->backupOptionCompressType));
                manifestName = strSubN(
                    info->name, 0, strSize(info->name) - strSize(compressExtStr(data->manifestData->backupOptionCompressType)));
            }

            ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), cryptoHashNew(HASH_TYPE_SHA1_STR));

            uint64_t size = bufUsed(storageGetP(read));
            const String *checksum = varStr(
                ioFilterGroupResult(ioReadFilterGroup(storageReadIo(read)), CRYPTO_HASH_FILTER_TYPE_STR));

            strCatFmt(data->content, ", s=%" PRIu64, size);

            // Check against the manifest
            // ---------------------------------------------------------------------------------------------------------------------
            const ManifestFile *file = manifestFileFind(data->manifest, manifestName);

            // Test size and repo-size. If compressed then set the repo-size to size so it will not be in test output. Even the same
            // compression algorithm can give slightly different results based on the version so repo-size is not deterministic for
            // compression.
            if (size != file->size)
                THROW_FMT(AssertError, "'%s' size does match manifest", strZ(manifestName));

            if (info->size != file->sizeRepo)
                THROW_FMT(AssertError, "'%s' repo size does match manifest", strZ(manifestName));

            if (data->manifestData->backupOptionCompressType != compressTypeNone)
                ((ManifestFile *)file)->sizeRepo = file->size;

            // Test the checksum. pg_control and WAL headers have different checksums depending on cpu architecture so remove
            // the checksum from the test output.
            if (!strEqZ(checksum, file->checksumSha1))
                THROW_FMT(AssertError, "'%s' checksum does match manifest", strZ(manifestName));

            if (strEqZ(manifestName, MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL) ||
                strBeginsWith(
                    manifestName, strNewFmt(MANIFEST_TARGET_PGDATA "/%s/", strZ(pgWalPath(data->manifestData->pgVersion)))))
            {
                ((ManifestFile *)file)->checksumSha1[0] = '\0';
            }

            // Test mode, user, group. These values are not in the manifest but we know what they should be based on the default
            // mode and current user/group.
            if (info->mode != 0640)
                THROW_FMT(AssertError, "'%s' mode is not 0640", strZ(manifestName));

            if (!strEq(info->user, TEST_USER_STR))
                THROW_FMT(AssertError, "'%s' user should be '" TEST_USER "'", strZ(manifestName));

            if (!strEq(info->group, TEST_GROUP_STR))
                THROW_FMT(AssertError, "'%s' group should be '" TEST_GROUP "'", strZ(manifestName));

            break;
        }

        case storageTypeLink:
            strCatFmt(data->content, "link, d=%s", strZ(info->linkDestination));
            break;

        case storageTypePath:
        {
            strCatZ(data->content, "path");

            // Check against the manifest
            // ---------------------------------------------------------------------------------------------------------------------
            manifestPathFind(data->manifest, info->name);

            // Test mode, user, group. These values are not in the manifest but we know what they should be based on the default
            // mode and current user/group.
            if (info->mode != 0750)
                THROW_FMT(AssertError, "'%s' mode is not 00750", strZ(info->name));

            if (!strEq(info->user, TEST_USER_STR))
                THROW_FMT(AssertError, "'%s' user should be '" TEST_USER "'", strZ(info->name));

            if (!strEq(info->group, TEST_GROUP_STR))
                THROW_FMT(AssertError, "'%s' group should be '" TEST_GROUP "'", strZ(info->name));

            break;
        }

        case storageTypeSpecial:
            THROW_FMT(AssertError, "unexpected special file '%s'", strZ(info->name));
    }

    strCatZ(data->content, "}\n");
}

static String *
testBackupValidate(const Storage *storage, const String *path)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STORAGE, storage);
        FUNCTION_HARNESS_PARAM(STRING, path);
    FUNCTION_HARNESS_END();

    String *result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build a list of files in the backup path and verify against the manifest
        // -------------------------------------------------------------------------------------------------------------------------
        Manifest *manifest = manifestLoadFile(storage, strNewFmt("%s/" BACKUP_MANIFEST_FILE, strZ(path)), cipherTypeNone, NULL);

        TestBackupValidateCallbackData callbackData =
        {
            .storage = storage,
            .path = path,
            .content = result,
            .manifest = manifest,
            .manifestData = manifestData(manifest),
        };

        storageInfoListP(storage, path, testBackupValidateCallback, &callbackData, .recurse = true, .sortOrder = sortOrderAsc);

        // Make sure both backup.manifest files exist since we skipped them in the callback above
        if (!storageExistsP(storage, strNewFmt("%s/" BACKUP_MANIFEST_FILE, strZ(path))))
            THROW(AssertError, BACKUP_MANIFEST_FILE " is missing");

        if (!storageExistsP(storage, strNewFmt("%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(path))))
            THROW(AssertError, BACKUP_MANIFEST_FILE INFO_COPY_EXT " is missing");

        // Output the manifest to a string and exclude sections that don't need validation. Note that each of these sections should
        // be considered from automatic validation but adding them to the output will make the tests too noisy. One good technique
        // would be to remove it from the output only after validation so new values will cause changes in the output.
        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *manifestSaveBuffer = bufNew(0);
        manifestSave(manifest, ioBufferWriteNew(manifestSaveBuffer));

        String *manifestEdit = strNew();
        StringList *manifestLine = strLstNewSplitZ(strTrim(strNewBuf(manifestSaveBuffer)), "\n");
        bool bSkipSection = false;

        for (unsigned int lineIdx = 0; lineIdx < strLstSize(manifestLine); lineIdx++)
        {
            const String *line = strTrim(strLstGet(manifestLine, lineIdx));

            if (strChr(line, '[') == 0)
            {
                const String *section = strSubN(line, 1, strSize(line) - 2);

                if (strEq(section, INFO_SECTION_BACKREST_STR) ||
                    strEq(section, MANIFEST_SECTION_BACKUP_STR) ||
                    strEq(section, MANIFEST_SECTION_BACKUP_DB_STR) ||
                    strEq(section, MANIFEST_SECTION_BACKUP_OPTION_STR) ||
                    strEq(section, MANIFEST_SECTION_DB_STR) ||
                    strEq(section, MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR) ||
                    strEq(section, MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR) ||
                    strEq(section, MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR))
                {
                    bSkipSection = true;
                }
                else
                    bSkipSection = false;
            }

            if (!bSkipSection)
                strCatFmt(manifestEdit, "%s\n", strZ(line));
        }

        strCatFmt(result, "--------\n%s\n", strZ(strTrim(manifestEdit)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Generate pq scripts for versions of PostgreSQL
***********************************************************************************************************************************/
typedef struct TestBackupPqScriptParam
{
    VAR_PARAM_HEADER;
    bool startFast;
    bool backupStandby;
    bool errorAfterStart;
    bool noWal;                                                     // Don't write test WAL segments
    CompressType walCompressType;                                   // Compress type for the archive files
    unsigned int walTotal;                                          // Total WAL to write
    unsigned int timeline;                                          // Timeline to use for WAL files
} TestBackupPqScriptParam;

#define testBackupPqScriptP(pgVersion, backupStartTime, ...)                                                                       \
    testBackupPqScript(pgVersion, backupStartTime, (TestBackupPqScriptParam){VAR_PARAM_INIT, __VA_ARGS__})

static void
testBackupPqScript(unsigned int pgVersion, time_t backupTimeStart, TestBackupPqScriptParam param)
{
    const char *pg1Path = TEST_PATH "/pg1";
    const char *pg2Path = TEST_PATH "/pg2";

    // If no timeline specified then use timeline 1
    param.timeline = param.timeline == 0 ? 1 : param.timeline;

    // Read pg_control to get info about the cluster
    PgControl pgControl = pgControlFromFile(storagePg());

    // Set archive timeout really small to save time on errors
    cfgOptionSet(cfgOptArchiveTimeout, cfgSourceParam, varNewInt64(100));

    uint64_t lsnStart = ((uint64_t)backupTimeStart & 0xFFFFFF00) << 28;
    uint64_t lsnStop =
        lsnStart + ((param.walTotal == 0 ? 0 : param.walTotal - 1) * pgControl.walSegmentSize) + (pgControl.walSegmentSize / 2);

    const char *lsnStartStr = strZ(pgLsnToStr(lsnStart));
    const char *walSegmentStart = strZ(pgLsnToWalSegment(param.timeline, lsnStart, pgControl.walSegmentSize));
    const char *lsnStopStr = strZ(pgLsnToStr(lsnStop));
    const char *walSegmentStop = strZ(pgLsnToWalSegment(param.timeline, lsnStop, pgControl.walSegmentSize));

    // Write WAL segments to the archive
    // -----------------------------------------------------------------------------------------------------------------------------
    if (!param.noWal)
    {
        InfoArchive *infoArchive = infoArchiveLoadFile(storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, cipherTypeNone, NULL);
        const String *archiveId = infoArchiveId(infoArchive);
        StringList *walSegmentList = pgLsnRangeToWalSegmentList(
            pgControl.version, param.timeline, lsnStart, lsnStop, pgControl.walSegmentSize);

        Buffer *walBuffer = bufNew((size_t)pgControl.walSegmentSize);
        bufUsedSet(walBuffer, bufSize(walBuffer));
        memset(bufPtr(walBuffer), 0, bufSize(walBuffer));
        hrnPgWalToBuffer((PgWal){.version = pgControl.version, .systemId = pgControl.systemId}, walBuffer);
        const String *walChecksum = bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer));

        for (unsigned int walSegmentIdx = 0; walSegmentIdx < strLstSize(walSegmentList); walSegmentIdx++)
        {
            StorageWrite *write = storageNewWriteP(
                storageRepoWrite(),
                strNewFmt(
                    STORAGE_REPO_ARCHIVE "/%s/%s-%s%s", strZ(archiveId), strZ(strLstGet(walSegmentList, walSegmentIdx)),
                    strZ(walChecksum), strZ(compressExtStr(param.walCompressType))));

            if (param.walCompressType != compressTypeNone)
                ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(write)), compressFilter(param.walCompressType, 1));

            storagePutP(write, walBuffer);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (pgVersion == PG_VERSION_95)
    {
        ASSERT(!param.backupStandby);
        ASSERT(!param.errorAfterStart);

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_95, pg1Path, false, NULL, NULL),

            // Get start time
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000),

            // Start backup
            HRNPQ_MACRO_ADVISORY_LOCK(1, true),
            HRNPQ_MACRO_IS_IN_BACKUP(1, false),
            HRNPQ_MACRO_START_BACKUP_84_95(1, param.startFast, lsnStartStr, walSegmentStart),
            HRNPQ_MACRO_DATABASE_LIST_1(1, "test1"),
            HRNPQ_MACRO_TABLESPACE_LIST_0(1),

            // Get copy start time
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 999),
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 1000),

            // Stop backup
            HRNPQ_MACRO_STOP_BACKUP_LE_95(1, lsnStopStr, walSegmentStop),

            // Get stop time
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 2000),

            HRNPQ_MACRO_DONE()
        });
    }
    // -----------------------------------------------------------------------------------------------------------------------------
    else if (pgVersion == PG_VERSION_96)
    {
        ASSERT(param.backupStandby);
        ASSERT(!param.errorAfterStart);

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_96, pg1Path, false, NULL, NULL),

            // Connect to standby
            HRNPQ_MACRO_OPEN_GE_96(2, "dbname='postgres' port=5433", PG_VERSION_96, pg2Path, true, NULL, NULL),

            // Get start time
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000),

            // Start backup
            HRNPQ_MACRO_ADVISORY_LOCK(1, true),
            HRNPQ_MACRO_START_BACKUP_96(1, true, lsnStartStr, walSegmentStart),
            HRNPQ_MACRO_DATABASE_LIST_1(1, "test1"),
            HRNPQ_MACRO_TABLESPACE_LIST_0(1),

            // Wait for standby to sync
            HRNPQ_MACRO_REPLAY_WAIT_96(2, lsnStartStr),

            // Get copy start time
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 999),
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 1000),

            // Stop backup
            HRNPQ_MACRO_STOP_BACKUP_96(1, lsnStopStr, walSegmentStop, false),

            // Get stop time
            HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 2000),

            HRNPQ_MACRO_DONE()
        });
    }
    // -----------------------------------------------------------------------------------------------------------------------------
    else if (pgVersion == PG_VERSION_11)
    {
        ASSERT(!param.backupStandby);

        if (param.errorAfterStart)
        {
            harnessPqScriptSet((HarnessPq [])
            {
                // Connect to primary
                HRNPQ_MACRO_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_11, pg1Path, false, NULL, NULL),

                // Get start time
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000),

                // Start backup
                HRNPQ_MACRO_ADVISORY_LOCK(1, true),
                HRNPQ_MACRO_START_BACKUP_GE_10(1, param.startFast, lsnStartStr, walSegmentStart),
                HRNPQ_MACRO_DATABASE_LIST_1(1, "test1"),
                HRNPQ_MACRO_TABLESPACE_LIST_1(1, 32768, "tblspc32768"),

                // Get copy start time
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 999),
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 1000),

                HRNPQ_MACRO_DONE()
            });
        }
        else
        {
            harnessPqScriptSet((HarnessPq [])
            {
                // Connect to primary
                HRNPQ_MACRO_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_11, pg1Path, false, NULL, NULL),

                // Get start time
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000),

                // Start backup
                HRNPQ_MACRO_ADVISORY_LOCK(1, true),
                HRNPQ_MACRO_START_BACKUP_GE_10(1, param.startFast, lsnStartStr, walSegmentStart),
                HRNPQ_MACRO_DATABASE_LIST_1(1, "test1"),
                HRNPQ_MACRO_TABLESPACE_LIST_1(1, 32768, "tblspc32768"),

                // Get copy start time
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 999),
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 1000),

                // Stop backup
                HRNPQ_MACRO_STOP_BACKUP_GE_10(1, lsnStopStr, walSegmentStop, false),

                // Get stop time
                HRNPQ_MACRO_TIME_QUERY(1, (int64_t)backupTimeStart * 1000 + 2000),

                HRNPQ_MACRO_DONE()
            });
        }
    }
    else
        THROW_FMT(AssertError, "unsupported test version %u", pgVersion);           // {uncoverable - no invalid versions in tests}
};

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Install local command handler shim
    static const ProtocolServerHandler testLocalHandlerList[] = {PROTOCOL_SERVER_HANDLER_BACKUP_LIST};
    hrnProtocolLocalShimInstall(testLocalHandlerList, PROTOCOL_SERVER_HANDLER_LIST_SIZE(testLocalHandlerList));

    // The tests expect the timezone to be UTC
    setenv("TZ", "UTC", true);

    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    const String *pgFile = STRDEF("testfile");
    const String *missingFile = STRDEF("missing");
    const String *backupLabel = STRDEF("20190718-155825F");
    const String *backupPathFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strZ(backupLabel), strZ(pgFile));
    BackupFileResult result = {0};

    // *****************************************************************************************************************************
    if (testBegin("segmentNumber()"))
    {
        TEST_RESULT_UINT(segmentNumber(pgFile), 0, "No segment number");
        TEST_RESULT_UINT(segmentNumber(strNewFmt("%s.123", strZ(pgFile))), 123, "Segment number");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupFile()"))
    {
        // Load Parameters
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        // Create the pg path
        HRN_STORAGE_PATH_CREATE(storagePgWrite(), NULL, .mode = 0700);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg file missing - ignoreMissing=true");

        TEST_ASSIGN(
            result,
            backupFile(
                missingFile, true, 0, true, NULL, false, 0, missingFile, false, compressTypeNone, 1, backupLabel, false,
                cipherTypeNone, NULL),
            "pg file missing, ignoreMissing=true, no delta");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 0, "copy/repo size 0");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultSkip, "skip file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg file missing - ignoreMissing=false");

        TEST_ERROR(
            backupFile(
                missingFile, false, 0, true, NULL, false, 0, missingFile, false, compressTypeNone, 1, backupLabel, false,
                cipherTypeNone, NULL),
            FileMissingError, "unable to open missing file '" TEST_PATH "/pg/missing' for read");

        // Create a pg file to backup
        HRN_STORAGE_PUT_Z(storagePgWrite(), strZ(pgFile), "atestfile");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("copy file to repo success - no prior checksum, no compression, no pageChecksum, no delta, no hasReference");

        // With the expected backupCopyResultCopy, unset the storageFeatureCompress bit for the storageRepo for code coverage
        uint64_t feature = storageRepo()->pub.interface.feature;
        ((Storage *)storageRepo())->pub.interface.feature = feature & ((1 << storageFeatureCompress) ^ 0xFFFFFFFFFFFFFFFF);

        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9999999, true, NULL, false, 0, pgFile, false, compressTypeNone, 1, backupLabel, false,
                cipherTypeNone, NULL),
            "pg file exists and shrunk, no repo file, no ignoreMissing, no pageChecksum, no delta, no hasReference");

        ((Storage *)storageRepo())->pub.interface.feature = feature;

        TEST_RESULT_UINT(result.copySize, 9, "copy=pgFile size");
        TEST_RESULT_UINT(result.repoSize, 9, "repo=pgFile size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum matches");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_EXISTS(storageRepo(), strZ(backupPathFile));

        // Remove repo file
        HRN_STORAGE_REMOVE(storageRepoWrite(), strZ(backupPathFile));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("test pagechecksum while db file grows");

        // Increase the file size but most of the following tests will still treat the file as size 9.  This tests the common case
        // where a file grows while a backup is running.
        HRN_STORAGE_PUT_Z(storagePgWrite(), strZ(pgFile), "atestfile###");

        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, true, NULL, true, 0xFFFFFFFFFFFFFFFF, pgFile, false, compressTypeNone, 1, backupLabel, false,
                cipherTypeNone, NULL),
            "file checksummed with pageChecksum enabled");
        TEST_RESULT_UINT(result.copySize, 9, "copy=pgFile size");
        TEST_RESULT_UINT(result.repoSize, 9, "repo=pgFile size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum matches");
        TEST_RESULT_PTR_NE(result.pageChecksumResult, NULL, "pageChecksumResult is set");
        TEST_RESULT_BOOL(varBool(kvGet(result.pageChecksumResult, VARSTRDEF("valid"))), false, "pageChecksumResult valid=false");
        TEST_STORAGE_EXISTS(storageRepoWrite(), strZ(backupPathFile), .remove = true, .comment = "check exists in repo, remove");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pgFileSize, ignoreMissing=false, backupLabel, pgFileChecksumPage, pgFileChecksumPageLsnLimit");

        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(pgFile));            // pgFile
        varLstAdd(paramList, varNewBool(false));            // pgFileIgnoreMissing
        varLstAdd(paramList, varNewUInt64(8));              // pgFileSize
        varLstAdd(paramList, varNewBool(false));            // pgFileCopyExactSize
        varLstAdd(paramList, NULL);                         // pgFileChecksum
        varLstAdd(paramList, varNewBool(true));             // pgFileChecksumPage
        varLstAdd(paramList, varNewUInt64(0xFFFFFFFFFFFFFFFF)); // pgFileChecksumPageLsnLimit
        varLstAdd(paramList, varNewStr(pgFile));            // repoFile
        varLstAdd(paramList, varNewBool(false));            // repoFileHasReference
        varLstAdd(paramList, varNewUInt(compressTypeNone)); // repoFileCompress
        varLstAdd(paramList, varNewInt(1));                 // repoFileCompressLevel
        varLstAdd(paramList, varNewStr(backupLabel));       // backupLabel
        varLstAdd(paramList, varNewBool(false));            // delta
        varLstAdd(paramList, varNewUInt64(cipherTypeNone)); // cipherType
        varLstAdd(paramList, NULL);                         // cipherSubPass

        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 8, false, NULL, true, 0xFFFFFFFFFFFFFFFF, pgFile, false, compressTypeNone, 1, backupLabel, false,
                cipherTypeNone, NULL),
            "backup file");
        TEST_RESULT_UINT(result.copySize, 12, "copy size");
        TEST_RESULT_UINT(result.repoSize, 12, "repo size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(result.copyChecksum, "c3ae4687ea8ccd47bfdb190dbe7fd3b37545fdb9", "checksum");
        TEST_RESULT_STR_Z(jsonFromKv(result.pageChecksumResult), "{\"align\":false,\"valid\":false}", "page checksum");
        TEST_STORAGE_GET(storageRepo(), strZ(backupPathFile), "atestfile###");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists in repo and db, checksum match - NOOP");

        // File exists in repo and db, pg checksum match, delta set, ignoreMissing false, hasReference - NOOP
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, true, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, true,
                compressTypeNone, 1, backupLabel, true, cipherTypeNone, NULL),
            "file in db and repo, checksum equal, no ignoreMissing, no pageChecksum, delta, hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "copy size set");
        TEST_RESULT_UINT(result.repoSize, 0, "repo size not set since already exists in repo");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultNoOp, "noop file");
        TEST_RESULT_STR_Z(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum matches");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_GET(storageRepo(), strZ(backupPathFile), "atestfile###", .comment = "file not modified");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists in repo and db, checksum mismatch - COPY");

        // File exists in repo and db, pg checksum mismatch, delta set, ignoreMissing false, hasReference - COPY
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, true, STRDEF("1234567890123456789012345678901234567890"), false, 0, pgFile, true,
                compressTypeNone, 1, backupLabel, true, cipherTypeNone, NULL),
            "file in db and repo, pg checksum not equal, no ignoreMissing, no pageChecksum, delta, hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "copy 9 bytes");
        TEST_RESULT_UINT(result.repoSize, 9, "repo=copy size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum for file size 9");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_GET(storageRepo(), strZ(backupPathFile), "atestfile", .comment = "9 bytes copied");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists in repo and pg, copy only exact file even if size passed is greater - COPY");

        // File exists in repo and pg, pg checksum same, pg size passed is different, delta set, ignoreMissing false, hasReference
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9999999, true, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, true,
                compressTypeNone, 1, backupLabel, true, cipherTypeNone, NULL),
            "db & repo file, pg checksum same, pg size different, no ignoreMissing, no pageChecksum, delta, hasReference");
        TEST_RESULT_UINT(result.copySize, 12, "copy=pgFile size");
        TEST_RESULT_UINT(result.repoSize, 12, "repo=pgFile size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(result.copyChecksum, "c3ae4687ea8ccd47bfdb190dbe7fd3b37545fdb9", "copy checksum updated");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_GET(storageRepo(), strZ(backupPathFile), "atestfile###", .comment = "confirm contents");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("resumed file is missing in repo but present in resumed manifest, file same name in repo - RECOPY");

        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP "/20190718-155825F", "testfile\n", .comment = "resumed file is missing in repo");
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, true, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, STRDEF(BOGUS_STR), false,
                compressTypeNone, 1, backupLabel, true, cipherTypeNone, NULL),
            "backup 9 bytes of pgfile to file to resume in repo");
        TEST_RESULT_UINT(result.copySize, 9, "copy 9 bytes");
        TEST_RESULT_UINT(result.repoSize, 9, "repo=copy size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultReCopy, "check recopy result");
        TEST_RESULT_STR_Z(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum for file size 9");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_GET(
            storageRepo(), strZ(backupPathFile), "atestfile###", .comment = "existing file with same name as pgFile not modified");
        TEST_STORAGE_GET(
            storageRepo(), STORAGE_REPO_BACKUP "/20190718-155825F/" BOGUS_STR, "atestfile", .comment = "resumed file copied");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists in repo & db, checksum not same in repo - RECOPY");

        HRN_STORAGE_PUT_Z(
            storageRepoWrite(), strZ(backupPathFile), "adifferentfile",
            .comment = "create different file (size and checksum) with same name in repo");

        // Delta set, ignoreMissing false, no hasReference
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, true, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, false,
                compressTypeNone, 1, backupLabel, true, cipherTypeNone, NULL),
            "db & repo file, pgFileMatch, repo checksum no match, no ignoreMissing, no pageChecksum, delta, no hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "copy 9 bytes");
        TEST_RESULT_UINT(result.repoSize, 9, "repo=copy size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultReCopy, "recopy file");
        TEST_RESULT_STR_Z(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum for file size 9");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_GET(storageRepo(), strZ(backupPathFile), "atestfile", .comment = "existing file recopied");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists in repo but missing from db, checksum same in repo - SKIP");

        TEST_ASSIGN(
            result,
            backupFile(
                missingFile, true, 9, true, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, false,
                compressTypeNone, 1, backupLabel, true, cipherTypeNone, NULL),
            "file in repo only, checksum in repo equal, ignoreMissing=true, no pageChecksum, delta, no hasReference");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 0, "copy=repo=0 size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultSkip, "skip file");
        TEST_RESULT_PTR(result.copyChecksum, NULL, "copy checksum NULL");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP "/20190718-155825F", BOGUS_STR "\n", .comment = "file removed from repo");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("compression set, all other boolean parameters false - COPY");

        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, true, NULL, false, 0, pgFile, false, compressTypeGz, 3, backupLabel, false, cipherTypeNone, NULL),
            "pg file exists, no checksum, no ignoreMissing, compression, no pageChecksum, no delta, no hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "copy=pgFile size");
        TEST_RESULT_UINT(result.repoSize, 29, "repo compress size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_EXISTS(
            storageRepo(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strZ(backupLabel), strZ(pgFile))),
            .comment = "copy file to repo compress success");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg and repo file exist & match, prior checksum, compression - COPY CHECKSUM");

        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, true, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, false, compressTypeGz,
                3, backupLabel, false, cipherTypeNone, NULL),
            "pg file & repo exists, match, checksum, no ignoreMissing, compression, no pageChecksum, no delta, no hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "copy=pgFile size");
        TEST_RESULT_UINT(result.repoSize, 29, "repo compress size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultChecksum, "checksum file");
        TEST_RESULT_STR_Z(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "compressed repo file checksum matches");
        TEST_STORAGE_EXISTS(
            storageRepo(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strZ(backupLabel), strZ(pgFile))),
            .comment = "compressed file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create a zero sized file - checksum will be set but in backupManifestUpdate it will not be copied");

        // Create zero sized file in pg
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "zerofile");

        // No prior checksum, no compression, no pageChecksum, no delta, no hasReference
        TEST_ASSIGN(
            result,
            backupFile(
                STRDEF("zerofile"), false, 0, true, NULL, false, 0, STRDEF("zerofile"), false, compressTypeNone, 1, backupLabel,
                false, cipherTypeNone, NULL),
            "zero-sized pg file exists, no repo file, no ignoreMissing, no pageChecksum, no delta, no hasReference");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 0, "copy=repo=pgFile size 0");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_PTR_NE(result.copyChecksum, NULL, "checksum set");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP "/20190718-155825F",
            BOGUS_STR "\n"
            "testfile.gz\n"
            "zerofile\n",
            .comment = "copy zero file to repo success");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupFile() - encrypt"))
    {
        // Load Parameters
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawStrId(argList, cfgOptRepoCipherType, cipherTypeAes256Cbc);
        hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
        HRN_CFG_LOAD(cfgCmdBackup, argList);
        hrnCfgEnvRemoveRaw(cfgOptRepoCipherPass);

        // Create the pg path and pg file to backup
        HRN_STORAGE_PUT_Z(storagePgWrite(), strZ(pgFile), "atestfile");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("copy file to encrypted repo");

        // No prior checksum, no compression, no pageChecksum, no delta, no hasReference
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, true, NULL, false, 0, pgFile, false, compressTypeNone, 1, backupLabel, false, cipherTypeAes256Cbc,
                STRDEF(TEST_CIPHER_PASS)),
            "pg file exists, no repo file, no ignoreMissing, no pageChecksum, no delta, no hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "copy size set");
        TEST_RESULT_UINT(result.repoSize, 32, "repo size set");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum NULL");
        TEST_STORAGE_GET(
            storageRepo(), strZ(backupPathFile), "atestfile", .cipherType = cipherTypeAes256Cbc,
            .comment = "copy file to encrypted repo success");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("delta, copy file (size mismatch) to encrypted repo");

        // Delta but pgFile does not match size passed, prior checksum, no compression, no pageChecksum, delta, no hasReference
        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 8, true, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, false,
                compressTypeNone, 1, backupLabel, true, cipherTypeAes256Cbc, STRDEF(TEST_CIPHER_PASS)),
            "pg and repo file exists, pgFileMatch false, no ignoreMissing, no pageChecksum, delta, no hasReference");
        TEST_RESULT_UINT(result.copySize, 8, "copy size set");
        TEST_RESULT_UINT(result.repoSize, 32, "repo size set");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(result.copyChecksum, "acc972a8319d4903b839c64ec217faa3e77b4fcb", "copy checksum for size passed");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum NULL");
        TEST_STORAGE_GET(
            storageRepo(), strZ(backupPathFile), "atestfil", .cipherType = cipherTypeAes256Cbc,
            .comment = "delta, copy file (size missmatch) to encrypted repo success");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no delta, recopy (size mismatch) file to encrypted repo");

        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, true, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"), false, 0, pgFile, false,
                compressTypeNone, 0, backupLabel, false, cipherTypeAes256Cbc, STRDEF(TEST_CIPHER_PASS)),
            "pg and repo file exists, checksum mismatch, no ignoreMissing, no pageChecksum, no delta, no hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "copy size set");
        TEST_RESULT_UINT(result.repoSize, 32, "repo size set");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultReCopy, "recopy file");
        TEST_RESULT_STR_Z(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum NULL");
        TEST_STORAGE_GET(
            storageRepoWrite(), strZ(backupPathFile), "atestfile", .cipherType = cipherTypeAes256Cbc,
            .comment = "recopy file to encrypted repo success");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no delta, recopy (checksum mismatch), file to encrypted repo");

        TEST_ASSIGN(
            result,
            backupFile(
                pgFile, false, 9, true, STRDEF("1234567890123456789012345678901234567890"), false, 0, pgFile, false,
                compressTypeNone, 0, backupLabel, false, cipherTypeAes256Cbc, STRDEF(TEST_CIPHER_PASS)),
            "backup file");

        TEST_RESULT_UINT(result.copySize, 9, "copy size set");
        TEST_RESULT_UINT(result.repoSize, 32, "repo size set");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultReCopy, "recopy file");
        TEST_RESULT_STR_Z(result.copyChecksum, "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum for size passed");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum NULL");
        TEST_STORAGE_GET(
            storageRepo(), strZ(backupPathFile), "atestfile",
            .cipherType = cipherTypeAes256Cbc, .comment = "recopy file to encrypted repo, success");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupLabelCreate()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        time_t timestamp = 1575401652;
        String *backupLabel = backupLabelFormat(backupTypeFull, NULL, timestamp);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("assign label when no history");

        HRN_STORAGE_PATH_CREATE(storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2019");

        TEST_RESULT_STR(backupLabelCreate(backupTypeFull, NULL, timestamp), backupLabel, "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("assign label when history is older");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), strZ(
                strNewFmt(STORAGE_REPO_BACKUP "/backup.history/2019/%s.manifest.gz",
                strZ(backupLabelFormat(backupTypeFull, NULL, timestamp - 4)))));

        TEST_RESULT_STR(backupLabelCreate(backupTypeFull, NULL, timestamp), backupLabel, "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("assign label when backup is older");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), strZ(
                strNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(backupLabelFormat(backupTypeFull, NULL, timestamp - 2)))));

        TEST_RESULT_STR(backupLabelCreate(backupTypeFull, NULL, timestamp), backupLabel, "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("advance time when backup is same");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), strZ(
                strNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(backupLabelFormat(backupTypeFull, NULL, timestamp)))));

        TEST_RESULT_STR_Z(backupLabelCreate(backupTypeFull, NULL, timestamp), "20191203-193413F", "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when new label is in the past even with advanced time");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), strZ(
                strNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(backupLabelFormat(backupTypeFull, NULL, timestamp + 1)))));

        TEST_ERROR(
            backupLabelCreate(backupTypeFull, NULL, timestamp), FormatError,
            "new backup label '20191203-193413F' is not later than latest backup label '20191203-193413F'\n"
            "HINT: has the timezone changed?\n"
            "HINT: is there clock skew?");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupInit()"))
    {
        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when backup from standby is not supported");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptBackupStandby, true);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_ERROR(
            backupInit(infoBackupNew(PG_VERSION_91, 1000000000000000910, hrnPgCatalogVersion(PG_VERSION_91), NULL)), ConfigError,
             "option 'backup-standby' not valid for PostgreSQL < 9.2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("warn and reset when backup from standby used in offline mode");

        // Create pg_control
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 1000000000000000920}));

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptBackupStandby, true);
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_RESULT_VOID(
            backupInit(infoBackupNew(PG_VERSION_92, 1000000000000000920, hrnPgCatalogVersion(PG_VERSION_92), NULL)),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptBackupStandby), false, "check backup-standby");

        TEST_RESULT_LOG(
            "P00   WARN: option backup-standby is enabled but backup is offline - backups will be performed from the primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when pg_control does not match stanza");

        // Create pg_control
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 1000000000000001000}));

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_ERROR(
            backupInit(infoBackupNew(PG_VERSION_11, 1000000000000001100, hrnPgCatalogVersion(PG_VERSION_11), NULL)),
            BackupMismatchError,
            "PostgreSQL version 10, system-id 1000000000000001000 do not match stanza version 11, system-id 1000000000000001100\n"
            "HINT: is this the correct stanza?");
        TEST_ERROR(
            backupInit(infoBackupNew(PG_VERSION_10, 1000000000000001100, hrnPgCatalogVersion(PG_VERSION_10), NULL)),
            BackupMismatchError,
            "PostgreSQL version 10, system-id 1000000000000001000 do not match stanza version 10, system-id 1000000000000001100\n"
            "HINT: is this the correct stanza?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("reset start-fast when PostgreSQL < 8.4");

        // Create pg_control
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_83, .systemId = 1000000000000000830}));

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        hrnCfgArgRawBool(argList, cfgOptStartFast, true);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_RESULT_VOID(
            backupInit(infoBackupNew(PG_VERSION_83, 1000000000000000830, hrnPgCatalogVersion(PG_VERSION_83), NULL)),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptStartFast), false, "check start-fast");

        TEST_RESULT_LOG("P00   WARN: start-fast option is only available in PostgreSQL >= 8.4");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("reset stop-auto when PostgreSQL < 9.3");

        // Create pg_control
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_84, .systemId = 1000000000000000840}));

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        hrnCfgArgRawBool(argList, cfgOptStopAuto, true);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_RESULT_VOID(
            backupInit(infoBackupNew(PG_VERSION_84, 1000000000000000840, hrnPgCatalogVersion(PG_VERSION_84), NULL)),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptStopAuto), false, "check stop-auto");

        TEST_RESULT_LOG("P00   WARN: stop-auto option is only available in PostgreSQL >= 9.3");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("reset checksum-page when the cluster does not have checksums enabled");

        // Create pg_control
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_93, .systemId = PG_VERSION_93}));

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptChecksumPage, true);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_93, TEST_PATH "/pg1", false, NULL, NULL),

            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(
            dbFree(backupInit(infoBackupNew(PG_VERSION_93, PG_VERSION_93, hrnPgCatalogVersion(PG_VERSION_93), NULL))->dbPrimary),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "check checksum-page");

        TEST_RESULT_LOG(
            "P00   WARN: checksum-page option set to true but checksums are not enabled on the cluster, resetting to false");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ok if cluster checksums are enabled and checksum-page is any value");

        // Create pg_control with page checksums
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_93, .systemId = PG_VERSION_93, .pageChecksum = true}));

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptChecksumPage, false);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_93, TEST_PATH "/pg1", false, NULL, NULL),

            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(
            dbFree(backupInit(infoBackupNew(PG_VERSION_93, PG_VERSION_93, hrnPgCatalogVersion(PG_VERSION_93), NULL))->dbPrimary),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "check checksum-page");

        // Create pg_control without page checksums
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_93, .systemId = PG_VERSION_93}));

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_93, TEST_PATH "/pg1", false, NULL, NULL),

            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(
            dbFree(backupInit(infoBackupNew(PG_VERSION_93, PG_VERSION_93, hrnPgCatalogVersion(PG_VERSION_93), NULL))->dbPrimary),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "check checksum-page");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupTime()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sleep retries and stall error");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        // Create pg_control
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_93, .systemId = PG_VERSION_93}));

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_93, TEST_PATH "/pg1", false, NULL, NULL),

            // Advance the time slowly to force retries
            HRNPQ_MACRO_TIME_QUERY(1, 1575392588998),
            HRNPQ_MACRO_TIME_QUERY(1, 1575392588999),
            HRNPQ_MACRO_TIME_QUERY(1, 1575392589001),

            // Stall time to force an error
            HRNPQ_MACRO_TIME_QUERY(1, 1575392589998),
            HRNPQ_MACRO_TIME_QUERY(1, 1575392589997),
            HRNPQ_MACRO_TIME_QUERY(1, 1575392589998),
            HRNPQ_MACRO_TIME_QUERY(1, 1575392589999),

            HRNPQ_MACRO_DONE()
        });

        BackupData *backupData = backupInit(
            infoBackupNew(PG_VERSION_93, PG_VERSION_93, hrnPgCatalogVersion(PG_VERSION_93), NULL));

        TEST_RESULT_INT(backupTime(backupData, true), 1575392588, "multiple tries for sleep");
        TEST_ERROR(backupTime(backupData, true), KernelError, "PostgreSQL clock has not advanced to the next second after 3 tries");

        dbFree(backupData->dbPrimary);
    }

    // *****************************************************************************************************************************
    if (testBegin("backupResumeFind()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
        hrnCfgArgRawBool(argList, cfgOptCompress, false);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cannot resume when manifest and copy are missing");

        HRN_STORAGE_PATH_CREATE(storageRepoWrite(), STORAGE_REPO_BACKUP "/20191003-105320F");

        TEST_RESULT_PTR(backupResumeFind((Manifest *)1, NULL), NULL, "find resumable backup");

        TEST_RESULT_LOG(
            "P00   WARN: backup '20191003-105320F' cannot be resumed: partially deleted by prior resume or invalid");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cannot resume when resume is disabled");

        HRN_STORAGE_PATH_CREATE(storageRepoWrite(), STORAGE_REPO_BACKUP "/20191003-105320F");

        cfgOptionSet(cfgOptResume, cfgSourceParam, BOOL_FALSE_VAR);

        HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), STORAGE_REPO_BACKUP "/20191003-105320F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT);

        TEST_RESULT_PTR(backupResumeFind((Manifest *)1, NULL), NULL, "find resumable backup");

        TEST_RESULT_LOG("P00   WARN: backup '20191003-105320F' cannot be resumed: resume is disabled");

        TEST_STORAGE_LIST_EMPTY(storageRepo(), STORAGE_REPO_BACKUP, .comment = "check backup path removed");

        cfgOptionSet(cfgOptResume, cfgSourceParam, BOOL_TRUE_VAR);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cannot resume when pgBackRest version has changed");

        Manifest *manifestResume = manifestNewInternal();
        manifestResume->pub.info = infoNew(NULL);
        manifestResume->pub.data.backupType = backupTypeFull;
        manifestResume->pub.data.backupLabel = STRDEF("20191003-105320F");
        manifestResume->pub.data.pgVersion = PG_VERSION_12;

        manifestTargetAdd(manifestResume, &(ManifestTarget){.name = MANIFEST_TARGET_PGDATA_STR, .path = STRDEF("/pg")});
        manifestPathAdd(manifestResume, &(ManifestPath){.name = MANIFEST_TARGET_PGDATA_STR});
        manifestFileAdd(manifestResume, &(ManifestFile){.name = STRDEF("pg_data/" PG_FILE_PGVERSION)});

        manifestSave(
            manifestResume,
            storageWriteIo(
                storageNewWriteP(
                    storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/20191003-105320F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT))));

        Manifest *manifest = manifestNewInternal();
        manifest->pub.data.backupType = backupTypeFull;
        manifest->pub.data.backrestVersion = STRDEF("BOGUS");

        TEST_RESULT_PTR(backupResumeFind(manifest, NULL), NULL, "find resumable backup");

        TEST_RESULT_LOG(
            "P00   WARN: backup '20191003-105320F' cannot be resumed:"
                " new pgBackRest version 'BOGUS' does not match resumable pgBackRest version '" PROJECT_VERSION "'");

        TEST_STORAGE_LIST_EMPTY(storageRepo(), STORAGE_REPO_BACKUP, .comment = "check backup path removed");

        manifest->pub.data.backrestVersion = STRDEF(PROJECT_VERSION);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cannot resume when backup labels do not match (resumable is null)");

        manifest->pub.data.backupType = backupTypeFull;
        manifest->pub.data.backupLabelPrior = STRDEF("20191003-105320F");

        manifestSave(
            manifestResume,
            storageWriteIo(
                storageNewWriteP(
                    storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/20191003-105320F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT))));

        TEST_RESULT_PTR(backupResumeFind(manifest, NULL), NULL, "find resumable backup");

        TEST_RESULT_LOG(
            "P00   WARN: backup '20191003-105320F' cannot be resumed:"
                " new prior backup label '<undef>' does not match resumable prior backup label '20191003-105320F'");

        TEST_STORAGE_LIST_EMPTY(storageRepo(), STORAGE_REPO_BACKUP, .comment = "check backup path removed");

        manifest->pub.data.backupLabelPrior = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cannot resume when backup labels do not match (new is null)");

        manifest->pub.data.backupType = backupTypeFull;
        manifestResume->pub.data.backupLabelPrior = STRDEF("20191003-105320F");

        manifestSave(
            manifestResume,
            storageWriteIo(
                storageNewWriteP(
                    storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/20191003-105320F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT))));

        TEST_RESULT_PTR(backupResumeFind(manifest, NULL), NULL, "find resumable backup");

        TEST_RESULT_LOG(
            "P00   WARN: backup '20191003-105320F' cannot be resumed:"
                " new prior backup label '20191003-105320F' does not match resumable prior backup label '<undef>'");

        TEST_STORAGE_LIST_EMPTY(storageRepo(), STORAGE_REPO_BACKUP, .comment = "check backup path removed");

        manifestResume->pub.data.backupLabelPrior = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cannot resume when compression does not match");

        manifestResume->pub.data.backupOptionCompressType = compressTypeGz;

        manifestSave(
            manifestResume,
            storageWriteIo(
                storageNewWriteP(
                    storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/20191003-105320F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT))));

        TEST_RESULT_PTR(backupResumeFind(manifest, NULL), NULL, "find resumable backup");

        TEST_RESULT_LOG(
            "P00   WARN: backup '20191003-105320F' cannot be resumed:"
                " new compression 'none' does not match resumable compression 'gz'");

        TEST_STORAGE_LIST_EMPTY(storageRepo(), STORAGE_REPO_BACKUP, .comment = "check backup path removed");

        manifestResume->pub.data.backupOptionCompressType = compressTypeNone;
    }

    // *****************************************************************************************************************************
    if (testBegin("backupJobResult()"))
    {
        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("report job error");

        ProtocolParallelJob *job = protocolParallelJobNew(VARSTRDEF("key"), protocolCommandNew(strIdFromZ(stringIdBit5, "x")));
        protocolParallelJobErrorSet(job, errorTypeCode(&AssertError), STRDEF("error message"));

        TEST_ERROR(backupJobResult((Manifest *)1, NULL, STRDEF("log"), strLstNew(), job, 0, 0), AssertError, "error message");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("report host/100% progress on noop result");

        // Create job that skips file
        job = protocolParallelJobNew(VARSTRDEF("pg_data/test"), protocolCommandNew(strIdFromZ(stringIdBit5, "x")));

        PackWrite *const resultPack = protocolPackNew();
        pckWriteU32P(resultPack, backupCopyResultNoOp);
        pckWriteU64P(resultPack, 0);
        pckWriteU64P(resultPack, 0);
        pckWriteStrP(resultPack, NULL);
        pckWriteStrP(resultPack, NULL);
        pckWriteEndP(resultPack);

        protocolParallelJobResultSet(job, pckReadNewBuf(pckWriteBuf(resultPack)));

        // Create manifest with file
        Manifest *manifest = manifestNewInternal();
        manifestFileAdd(manifest, &(ManifestFile){.name = STRDEF("pg_data/test")});

        TEST_RESULT_UINT(
            backupJobResult(manifest, STRDEF("host"), STRDEF("log-test"), strLstNew(), job, 0, 0), 0, "log noop result");

        TEST_RESULT_LOG("P00 DETAIL: match file from prior backup host:log-test (0B, 100%)");
    }

    // Offline tests should only be used to test offline functionality and errors easily tested in offline mode
    // *****************************************************************************************************************************
    if (testBegin("cmdBackup() offline"))
    {
        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // Replace backup labels since the times are not deterministic
        hrnLogReplaceAdd("[0-9]{8}-[0-9]{6}F_[0-9]{8}-[0-9]{6}I", NULL, "INCR", true);
        hrnLogReplaceAdd("[0-9]{8}-[0-9]{6}F_[0-9]{8}-[0-9]{6}D", NULL, "DIFF", true);
        hrnLogReplaceAdd("[0-9]{8}-[0-9]{6}F", NULL, "FULL", true);

        // Create stanza
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        // Create pg_control
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_84, .systemId = 1000000000000000840}));

        cmdStanzaCreate();
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'test1' on repo1");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when pg appears to be running");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_POSTMASTERPID, "PID");

        TEST_ERROR(
            cmdBackup(), PgRunningError,
            "--no-online passed but postmaster.pid exists - looks like " PG_NAME " is running. Shut down " PG_NAME " and try"
                " again, or use --force.");

        TEST_RESULT_LOG("P00   WARN: no prior backup exists, incr backup has been changed to full");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("offline full backup");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        hrnCfgArgRawBool(argList, cfgOptCompress, false);
        hrnCfgArgRawBool(argList, cfgOptForce, true);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        HRN_STORAGE_PUT_Z(storagePgWrite(), "postgresql.conf", "CONFIGSTUFF");

        TEST_RESULT_VOID(cmdBackup(), "backup");

        TEST_RESULT_LOG_FMT(
            "P00   WARN: no prior backup exists, incr backup has been changed to full\n"
            "P00   WARN: --no-online passed and postmaster.pid exists but --force was passed so backup will continue though it"
                " looks like " PG_NAME " is running and the backup will probably not be consistent\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, 99%%) checksum %s\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (11B, 100%%) checksum"
                " e3db315c260e79211b7b52587123b7aa060f30ab\n"
            "P00   INFO: full backup size = 8KB\n"
            "P00   INFO: new backup label = [FULL-1]",
            TEST_64BIT() ?
                (TEST_BIG_ENDIAN() ? "749acedef8f8d5fe35fc20c0375657f876ccc38e" : "21e2ddc99cdf4cfca272eee4f38891146092e358") :
                "8bb70506d988a8698d9e8cf90736ada23634571b");

        // Make pg no longer appear to be running
        HRN_STORAGE_REMOVE(storagePgWrite(), PG_FILE_POSTMASTERPID, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when no files have changed");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        hrnCfgArgRawBool(argList, cfgOptCompress, true);
        hrnCfgArgRawBool(argList, cfgOptRepoHardlink, true);
        hrnCfgArgRawStrId(argList, cfgOptType, backupTypeDiff);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_ERROR(cmdBackup(), FileMissingError, "no files have changed since the last backup - this seems unlikely");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P00   WARN: diff backup cannot alter compress-type option to 'gz', reset to value in [FULL-1]\n"
            "P00   WARN: diff backup cannot alter hardlink option to 'true', reset to value in [FULL-1]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("offline incr backup to test unresumable backup");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        hrnCfgArgRawBool(argList, cfgOptCompress, false);
        hrnCfgArgRawBool(argList, cfgOptChecksumPage, true);
        hrnCfgArgRawStrId(argList, cfgOptType, backupTypeIncr);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, "VER");

        TEST_RESULT_VOID(cmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P00   WARN: incr backup cannot alter 'checksum-page' option to 'true', reset to 'false' from [FULL-1]\n"
            "P00   WARN: backup '[DIFF-1]' cannot be resumed: new backup type 'incr' does not match resumable backup type 'diff'\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, 100%) checksum c8663c2525f44b6d9c687fbceb4aafc63ed8b451\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-1]\n"
            "P00 DETAIL: reference pg_data/postgresql.conf to [FULL-1]\n"
            "P00   INFO: incr backup size = 3B\n"
            "P00   INFO: new backup label = [INCR-1]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("offline diff backup to test prior backup must be full");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        hrnCfgArgRawBool(argList, cfgOptCompress, false);
        hrnCfgArgRawStrId(argList, cfgOptType, backupTypeDiff);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        sleepMSec(MSEC_PER_SEC - (timeMSec() % MSEC_PER_SEC));
        HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, "VR2");

        TEST_RESULT_VOID(cmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, 100%) checksum 6f1894088c578e4f0b9888e8e8a997d93cbbc0c5\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-1]\n"
            "P00 DETAIL: reference pg_data/postgresql.conf to [FULL-1]\n"
            "P00   INFO: diff backup size = 3B\n"
            "P00   INFO: new backup label = [DIFF-2]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("only repo2 configured");

        // Create stanza on a second repo
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        cmdStanzaCreate();
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'test1' on repo2");

        // Set log level to warn
        harnessLogLevelSet(logLevelWarn);

        // With repo2 the only repo configured, ensure it is chosen by confirming diff is changed to full due to no prior backups
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 2, "1");
        hrnCfgArgRawStrId(argList, cfgOptType, backupTypeDiff);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_RESULT_VOID(cmdBackup(), "backup");
        TEST_RESULT_LOG("P00   WARN: no prior backup exists, diff backup has been changed to full");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo");

        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // Add repo1 to the configuration
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_RESULT_VOID(cmdBackup(), "backup");
        TEST_RESULT_LOG(
            "P00   INFO: repo option not specified, defaulting to repo1\n"
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P00   WARN: diff backup cannot alter compress-type option to 'gz', reset to value in [FULL-1]\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, 100%) checksum 6f1894088c578e4f0b9888e8e8a997d93cbbc0c5\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-1]\n"
            "P00 DETAIL: reference pg_data/postgresql.conf to [FULL-1]\n"
            "P00   INFO: diff backup size = 3B\n"
            "P00   INFO: new backup label = [DIFF-3]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo - specify repo");

        hrnCfgArgRawZ(argList, cfgOptRepo, "2");

        HRN_CFG_LOAD(cfgCmdBackup, argList);

        HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, "VER");

        unsigned int backupCount = strLstSize(storageListP(storageRepoIdx(1), strNewFmt(STORAGE_PATH_BACKUP "/test1")));

        TEST_RESULT_VOID(cmdBackup(), "backup");
        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-2], version = " PROJECT_VERSION "\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, 100%) checksum c8663c2525f44b6d9c687fbceb4aafc63ed8b451\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-2]\n"
            "P00 DETAIL: reference pg_data/postgresql.conf to [FULL-2]\n"
            "P00   INFO: diff backup size = 3B\n"
            "P00   INFO: new backup label = [DIFF-4]");
        TEST_RESULT_UINT(
            strLstSize(storageListP(storageRepoIdx(1), strNewFmt(STORAGE_PATH_BACKUP "/test1"))), backupCount + 1,
            "new backup repo2");

        // Cleanup
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);
        harnessLogLevelReset();
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdBackup() online"))
    {
        const String *pg1Path = STRDEF(TEST_PATH "/pg1");
        const String *repoPath = STRDEF(TEST_PATH "/repo");
        const String *pg2Path = STRDEF(TEST_PATH "/pg2");

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
            // Create stanza
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawBool(argList, cfgOptOnline, false);
            HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

            // Create pg_control
            HRN_STORAGE_PUT(
                storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
                hrnPgControlToBuffer((PgControl){.version = PG_VERSION_95, .systemId = 1000000000000000950}),
                .timeModified = backupTimeStart);

            cmdStanzaCreate();
            TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'test1' on repo1");

            // Load options
            argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawBool(argList, cfgOptStopAuto, true);
            hrnCfgArgRawBool(argList, cfgOptCompress, false);
            hrnCfgArgRawBool(argList, cfgOptArchiveCheck, false);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Add files
            HRN_STORAGE_PUT_Z(storagePgWrite(), "postgresql.conf", "CONFIGSTUFF", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, PG_VERSION_95_STR, .timeModified = backupTimeStart);
            HRN_STORAGE_PATH_CREATE(storagePgWrite(), strZ(pgWalPath(PG_VERSION_95)), .noParentCreate = true);

            // Create a backup manifest that looks like a halted backup manifest
            Manifest *manifestResume = manifestNewBuild(
                storagePg(), PG_VERSION_95, hrnPgCatalogVersion(PG_VERSION_95), true, false, NULL, NULL);
            ManifestData *manifestResumeData = (ManifestData *)manifestData(manifestResume);

            manifestResumeData->backupType = backupTypeFull;
            const String *resumeLabel = backupLabelCreate(backupTypeFull, NULL, backupTimeStart);
            manifestBackupLabelSet(manifestResume, resumeLabel);

            // Copy a file to be resumed that has not changed in the repo
            HRN_STORAGE_COPY(
                storagePg(), PG_FILE_PGVERSION, storageRepoWrite(),
                strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/PG_VERSION", strZ(resumeLabel))));

            strcpy(
                ((ManifestFile *)manifestFileFind(manifestResume, STRDEF("pg_data/PG_VERSION")))->checksumSha1,
                "06d06bb31b570b94d7b4325f511f853dbe771c21");

            // Save the resume manifest
            manifestSave(
                manifestResume,
                storageWriteIo(
                    storageNewWriteP(
                        storageRepoWrite(),
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(resumeLabel)))));

            // Run backup
            testBackupPqScriptP(PG_VERSION_95, backupTimeStart);
            TEST_RESULT_VOID(cmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: execute exclusive pg_start_backup(): backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105D944C000000000, lsn = 5d944c0/0\n"
                "P00   WARN: resumable backup 20191002-070640F of same type exists -- remove invalid files and resume\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: checksum resumed file " TEST_PATH "/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
                "P00   INFO: full backup size = [SIZE]\n"
                "P00   INFO: execute exclusive pg_stop_backup() and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105D944C000000000, lsn = 5d944c0/800000\n"
                "P00   INFO: new backup label = 20191002-070640F");

            TEST_RESULT_STR_Z(
                testBackupValidate(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ". {link, d=20191002-070640F}\n"
                "pg_data {path}\n"
                "pg_data/PG_VERSION {file, s=3}\n"
                "pg_data/global {path}\n"
                "pg_data/global/pg_control {file, s=8192}\n"
                "pg_data/pg_xlog {path}\n"
                "pg_data/postgresql.conf {file, s=11}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"06d06bb31b570b94d7b4325f511f853dbe771c21\",\"size\":3"
                    ",\"timestamp\":1570000000}\n"
                "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1570000000}\n"
                "pg_data/postgresql.conf={\"checksum\":\"e3db315c260e79211b7b52587123b7aa060f30ab\",\"size\":11"
                    ",\"timestamp\":1570000000}\n"
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_xlog={}\n",
                "compare file list");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online resumed compressed 9.5 full backup");

        // Backup start time
        backupTimeStart = BACKUP_EPOCH + 100000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawBool(argList, cfgOptStopAuto, true);
            hrnCfgArgRawBool(argList, cfgOptRepoHardlink, true);
            hrnCfgArgRawBool(argList, cfgOptArchiveCopy, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Create a backup manifest that looks like a halted backup manifest
            Manifest *manifestResume = manifestNewBuild(
                storagePg(), PG_VERSION_95, hrnPgCatalogVersion(PG_VERSION_95), true, false, NULL, NULL);
            ManifestData *manifestResumeData = (ManifestData *)manifestData(manifestResume);

            manifestResumeData->backupType = backupTypeFull;
            manifestResumeData->backupOptionCompressType = compressTypeGz;
            const String *resumeLabel = backupLabelCreate(backupTypeFull, NULL, backupTimeStart);
            manifestBackupLabelSet(manifestResume, resumeLabel);

            // File exists in cluster and repo but not in the resume manifest
            HRN_STORAGE_PUT_Z(storagePgWrite(), "not-in-resume", "TEST", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/not-in-resume.gz", strZ(resumeLabel))));

            // Remove checksum from file so it won't be resumed
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/global/pg_control.gz", strZ(resumeLabel))));

            ((ManifestFile *)manifestFileFind(manifestResume, STRDEF("pg_data/global/pg_control")))->checksumSha1[0] = 0;

            // Size does not match between cluster and resume manifest
            HRN_STORAGE_PUT_Z(storagePgWrite(), "size-mismatch", "TEST", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/size-mismatch.gz", strZ(resumeLabel))));
            manifestFileAdd(
                manifestResume, &(ManifestFile){
                    .name = STRDEF("pg_data/size-mismatch"), .checksumSha1 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
                    .size = 33});

            // Time does not match between cluster and resume manifest
            HRN_STORAGE_PUT_Z(storagePgWrite(), "time-mismatch", "TEST", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/time-mismatch.gz", strZ(resumeLabel))));
            manifestFileAdd(
                manifestResume, &(ManifestFile){
                    .name = STRDEF("pg_data/time-mismatch"), .checksumSha1 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", .size = 4,
                    .timestamp = backupTimeStart - 1});

            // Size is zero in cluster and resume manifest. ??? We'd like to remove this requirement after the migration.
            HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "zero-size", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_Z(
                storageRepoWrite(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/zero-size.gz", strZ(resumeLabel))),
                "ZERO-SIZE");
            manifestFileAdd(
                manifestResume, &(ManifestFile){.name = STRDEF("pg_data/zero-size"), .size = 0, .timestamp = backupTimeStart});

            // Path is not in manifest
            HRN_STORAGE_PATH_CREATE(
                storageRepoWrite(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/bogus_path", strZ(resumeLabel))));

            // File is not in manifest
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/global/bogus.gz", strZ(resumeLabel))));

            // File has incorrect compression type
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/global/bogus", strZ(resumeLabel))));

            // Save the resume manifest
            manifestSave(
                manifestResume,
                storageWriteIo(
                    storageNewWriteP(
                        storageRepoWrite(),
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(resumeLabel)))));

            // Disable storageFeaturePath so paths will not be created before files are copied
            ((Storage *)storageRepoWrite())->pub.interface.feature ^= 1 << storageFeaturePath;

            // Disable storageFeaturePathSync so paths will not be synced
            ((Storage *)storageRepoWrite())->pub.interface.feature ^= 1 << storageFeaturePathSync;

            // Run backup
            testBackupPqScriptP(PG_VERSION_95, backupTimeStart);
            TEST_RESULT_VOID(cmdBackup(), "backup");

            // Enable storage features
            ((Storage *)storageRepoWrite())->pub.interface.feature |= 1 << storageFeaturePath;
            ((Storage *)storageRepoWrite())->pub.interface.feature |= 1 << storageFeaturePathSync;

            TEST_RESULT_LOG(
                "P00   INFO: execute exclusive pg_start_backup(): backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105D95D3000000000, lsn = 5d95d30/0\n"
                "P00   WARN: resumable backup 20191003-105320F of same type exists -- remove invalid files and resume\n"
                "P00 DETAIL: remove path '" TEST_PATH "/repo/backup/test1/20191003-105320F/pg_data/bogus_path' from resumed"
                    " backup\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191003-105320F/pg_data/global/bogus' from resumed"
                    " backup (mismatched compression type)\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191003-105320F/pg_data/global/bogus.gz' from resumed"
                    " backup (missing in manifest)\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191003-105320F/pg_data/global/pg_control.gz' from"
                    " resumed backup (no checksum in resumed manifest)\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191003-105320F/pg_data/not-in-resume.gz' from resumed"
                    " backup (missing in resumed manifest)\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191003-105320F/pg_data/size-mismatch.gz' from resumed"
                    " backup (mismatched size)\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191003-105320F/pg_data/time-mismatch.gz' from resumed"
                    " backup (mismatched timestamp)\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191003-105320F/pg_data/zero-size.gz' from resumed"
                    " backup (zero size)\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/time-mismatch (4B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/size-mismatch (4B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/not-in-resume (4B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/zero-size (0B, [PCT])\n"
                "P00   INFO: full backup size = [SIZE]\n"
                "P00   INFO: execute exclusive pg_stop_backup() and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105D95D3000000000, lsn = 5d95d30/800000\n"
                "P00   INFO: check archive for segment(s) 0000000105D95D3000000000:0000000105D95D3000000000\n"
                "P00 DETAIL: copy segment 0000000105D95D3000000000 to backup\n"
                "P00   INFO: new backup label = 20191003-105320F");

            TEST_RESULT_STR_Z(
                testBackupValidate(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ". {link, d=20191003-105320F}\n"
                "pg_data {path}\n"
                "pg_data/PG_VERSION.gz {file, s=3}\n"
                "pg_data/global {path}\n"
                "pg_data/global/pg_control.gz {file, s=8192}\n"
                "pg_data/not-in-resume.gz {file, s=4}\n"
                "pg_data/pg_xlog {path}\n"
                "pg_data/pg_xlog/0000000105D95D3000000000.gz {file, s=16777216}\n"
                "pg_data/postgresql.conf.gz {file, s=11}\n"
                "pg_data/size-mismatch.gz {file, s=4}\n"
                "pg_data/time-mismatch.gz {file, s=4}\n"
                "pg_data/zero-size.gz {file, s=0}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"06d06bb31b570b94d7b4325f511f853dbe771c21\",\"size\":3"
                    ",\"timestamp\":1570000000}\n"
                "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1570000000}\n"
                "pg_data/not-in-resume={\"checksum\":\"984816fd329622876e14907634264e6f332e9fb3\",\"size\":4"
                    ",\"timestamp\":1570100000}\n"
                "pg_data/pg_xlog/0000000105D95D3000000000={\"size\":16777216,\"timestamp\":1570100002}\n"
                "pg_data/postgresql.conf={\"checksum\":\"e3db315c260e79211b7b52587123b7aa060f30ab\",\"size\":11"
                    ",\"timestamp\":1570000000}\n"
                "pg_data/size-mismatch={\"checksum\":\"984816fd329622876e14907634264e6f332e9fb3\",\"size\":4"
                    ",\"timestamp\":1570100000}\n"
                "pg_data/time-mismatch={\"checksum\":\"984816fd329622876e14907634264e6f332e9fb3\",\"size\":4"
                    ",\"timestamp\":1570100000}\n"
                "pg_data/zero-size={\"size\":0,\"timestamp\":1570100000}\n"
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_xlog={}\n",
                "compare file list");

            // Remove test files
            HRN_STORAGE_REMOVE(storagePgWrite(), "not-in-resume", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "size-mismatch", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "time-mismatch", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "zero-size", .errorOnMissing = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online resumed compressed 9.5 diff backup");

        backupTimeStart = BACKUP_EPOCH + 200000;

        {
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeDiff);
            hrnCfgArgRawBool(argList, cfgOptCompress, false);
            hrnCfgArgRawBool(argList, cfgOptStopAuto, true);
            hrnCfgArgRawBool(argList, cfgOptRepoHardlink, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Load the previous manifest and null out the checksum-page option to be sure it gets set to false in this backup
            const String *manifestPriorFile = STRDEF(STORAGE_REPO_BACKUP "/latest/" BACKUP_MANIFEST_FILE);
            Manifest *manifestPrior = manifestNewLoad(storageReadIo(storageNewReadP(storageRepo(), manifestPriorFile)));
            ((ManifestData *)manifestData(manifestPrior))->backupOptionChecksumPage = NULL;
            manifestSave(manifestPrior, storageWriteIo(storageNewWriteP(storageRepoWrite(), manifestPriorFile)));

            // Create a backup manifest that looks like a halted backup manifest
            Manifest *manifestResume = manifestNewBuild(
                storagePg(), PG_VERSION_95, hrnPgCatalogVersion(PG_VERSION_95), true, false, NULL, NULL);
            ManifestData *manifestResumeData = (ManifestData *)manifestData(manifestResume);

            manifestResumeData->backupType = backupTypeDiff;
            manifestResumeData->backupLabelPrior = manifestData(manifestPrior)->backupLabel;
            manifestResumeData->backupOptionCompressType = compressTypeGz;
            const String *resumeLabel = backupLabelCreate(
                backupTypeDiff, manifestData(manifestPrior)->backupLabel, backupTimeStart);
            manifestBackupLabelSet(manifestResume, resumeLabel);

            // Reference in manifest
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/PG_VERSION.gz", strZ(resumeLabel))));

            // Reference in resumed manifest
            HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "resume-ref", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/resume-ref.gz", strZ(resumeLabel))));
            manifestFileAdd(
                manifestResume, &(ManifestFile){.name = STRDEF("pg_data/resume-ref"), .size = 0, .reference = STRDEF("BOGUS")});

            // Time does not match between cluster and resume manifest (but resume because time is in future so delta enabled). Note
            // also that the repo file is intenionally corrupt to generate a warning about corruption in the repository.
            HRN_STORAGE_PUT_Z(storagePgWrite(), "time-mismatch2", "TEST", .timeModified = backupTimeStart + 100);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), strZ(strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/time-mismatch2.gz", strZ(resumeLabel))));
            manifestFileAdd(
                manifestResume, &(ManifestFile){
                    .name = STRDEF("pg_data/time-mismatch2"), .checksumSha1 = "984816fd329622876e14907634264e6f332e9fb3", .size = 4,
                    .timestamp = backupTimeStart});

            // Links are always removed on resume
            THROW_ON_SYS_ERROR(
                symlink(
                    "..",
                    strZ(storagePathP(storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/link", strZ(resumeLabel))))) == -1,
                FileOpenError, "unable to create symlink");

            // Special files should not be in the repo
            HRN_SYSTEM_FMT(
                "mkfifo -m 666 %s",
                strZ(storagePathP(storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/pipe", strZ(resumeLabel)))));

            // Save the resume manifest
            manifestSave(
                manifestResume,
                storageWriteIo(
                    storageNewWriteP(
                        storageRepoWrite(),
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(resumeLabel)))));

            // Run backup
            testBackupPqScriptP(PG_VERSION_95, backupTimeStart);
            TEST_RESULT_VOID(cmdBackup(), "backup");

            // Check log
            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191003-105320F, version = " PROJECT_VERSION "\n"
                "P00   WARN: diff backup cannot alter compress-type option to 'none', reset to value in 20191003-105320F\n"
                "P00   INFO: execute exclusive pg_start_backup(): backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105D9759000000000, lsn = 5d97590/0\n"
                "P00   WARN: file 'time-mismatch2' has timestamp in the future, enabling delta checksum\n"
                "P00   WARN: resumable backup 20191003-105320F_20191004-144000D of same type exists"
                    " -- remove invalid files and resume\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191003-105320F_20191004-144000D/pg_data/PG_VERSION.gz'"
                    " from resumed backup (reference in manifest)\n"
                "P00   WARN: remove special file '" TEST_PATH "/repo/backup/test1/20191003-105320F_20191004-144000D/pg_data/pipe'"
                    " from resumed backup\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191003-105320F_20191004-144000D/pg_data/resume-ref.gz'"
                    " from resumed backup (reference in resumed manifest)\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P00   WARN: resumed backup file pg_data/time-mismatch2 does not have expected checksum"
                    " 984816fd329622876e14907634264e6f332e9fb3. The file will be recopied and backup will continue but this may be"
                    " an issue unless the resumed backup path in the repository is known to be corrupted.\n"
                "            NOTE: this does not indicate a problem with the PostgreSQL page checksums.\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/time-mismatch2 (4B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/resume-ref (0B, [PCT])\n"
                "P00 DETAIL: hardlink pg_data/PG_VERSION to 20191003-105320F\n"
                "P00 DETAIL: hardlink pg_data/global/pg_control to 20191003-105320F\n"
                "P00 DETAIL: hardlink pg_data/postgresql.conf to 20191003-105320F\n"
                "P00   INFO: diff backup size = [SIZE]\n"
                "P00   INFO: execute exclusive pg_stop_backup() and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105D9759000000000, lsn = 5d97590/800000\n"
                    "P00   INFO: check archive for segment(s) 0000000105D9759000000000:0000000105D9759000000000\n"
                "P00   INFO: new backup label = 20191003-105320F_20191004-144000D");

            // Check repo directory
            TEST_RESULT_STR_Z(
                testBackupValidate(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ". {link, d=20191003-105320F_20191004-144000D}\n"
                "pg_data {path}\n"
                "pg_data/PG_VERSION.gz {file, s=3}\n"
                "pg_data/global {path}\n"
                "pg_data/global/pg_control.gz {file, s=8192}\n"
                "pg_data/pg_xlog {path}\n"
                "pg_data/postgresql.conf.gz {file, s=11}\n"
                "pg_data/resume-ref.gz {file, s=0}\n"
                "pg_data/time-mismatch2.gz {file, s=4}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"06d06bb31b570b94d7b4325f511f853dbe771c21\",\"reference\":\"20191003-105320F\""
                    ",\"size\":3,\"timestamp\":1570000000}\n"
                "pg_data/global/pg_control={\"reference\":\"20191003-105320F\",\"size\":8192,\"timestamp\":1570000000}\n"
                "pg_data/postgresql.conf={\"checksum\":\"e3db315c260e79211b7b52587123b7aa060f30ab\""
                    ",\"reference\":\"20191003-105320F\",\"size\":11,\"timestamp\":1570000000}\n"
                "pg_data/resume-ref={\"size\":0,\"timestamp\":1570200000}\n"
                "pg_data/time-mismatch2={\"checksum\":\"984816fd329622876e14907634264e6f332e9fb3\",\"size\":4"
                    ",\"timestamp\":1570200100}\n"
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_xlog={}\n",
                "compare file list");

            // Remove test files
            HRN_STORAGE_REMOVE(storagePgWrite(), "resume-ref", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "time-mismatch2", .errorOnMissing = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 9.6 backup-standby full backup");

        backupTimeStart = BACKUP_EPOCH + 1200000;

        {
            // Update pg_control
            HRN_STORAGE_PUT(
                storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
                hrnPgControlToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 1000000000000000960}),
                .timeModified = backupTimeStart);

            // Update version
            HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, PG_VERSION_96_STR, .timeModified = backupTimeStart);

            // Upgrade stanza
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawBool(argList, cfgOptOnline, false);
            HRN_CFG_LOAD(cfgCmdStanzaUpgrade, argList);

            cmdStanzaUpgrade();
            TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'test1' on repo1");

            // Load options
            argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pg1Path);
            hrnCfgArgKeyRaw(argList, cfgOptPgPath, 2, pg2Path);
            hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 2, "5433");
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawBool(argList, cfgOptCompress, false);
            hrnCfgArgRawBool(argList, cfgOptBackupStandby, true);
            hrnCfgArgRawBool(argList, cfgOptStartFast, true);
            hrnCfgArgRawBool(argList, cfgOptArchiveCopy, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Create file to copy from the standby. This file will be zero-length on the primary and non-zero-length on the standby
            // but no bytes will be copied.
            HRN_STORAGE_PUT_EMPTY(storagePgIdxWrite(0), PG_PATH_BASE "/1/1", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(1), PG_PATH_BASE "/1/1", "1234");

            // Create file to copy from the standby. This file will be smaller on the primary than the standby and have no common
            // data in the bytes that exist on primary and standby.  If the file is copied from the primary instead of the standby
            // the checksum will change but not the size.
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(0), PG_PATH_BASE "/1/2", "DA", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(1), PG_PATH_BASE "/1/2", "5678");

            // Create file to copy from the standby. This file will be larger on the primary than the standby and have no common
            // data in the bytes that exist on primary and standby.  If the file is copied from the primary instead of the standby
            // the checksum and size will change.
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(0), PG_PATH_BASE "/1/3", "TEST", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(1), PG_PATH_BASE "/1/3", "ABC");

            // Create a file on the primary that does not exist on the standby to test that the file is removed from the manifest
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(0), PG_PATH_BASE "/1/0", "DATA", .timeModified = backupTimeStart);

            // Set log level to warn because the following test uses multiple processes so the log order will not be deterministic
            harnessLogLevelSet(logLevelWarn);

            // Run backup but error on archive check
            testBackupPqScriptP(PG_VERSION_96, backupTimeStart, .noWal = true, .backupStandby = true);
            TEST_ERROR(
                cmdBackup(), ArchiveTimeoutError,
                "WAL segment 0000000105DA69C000000000 was not archived before the 100ms timeout\n"
                "HINT: check the archive_command to ensure that all options are correct (especially --stanza).\n"
                "HINT: check the PostgreSQL server log for errors.\n"
                "HINT: run the 'start' command if the stanza was previously stopped.");

            // Remove halted backup so there's no resume
            HRN_STORAGE_PATH_REMOVE(storageRepoWrite(), STORAGE_REPO_BACKUP "/20191016-042640F", .recurse = true);

            // Run backup
            testBackupPqScriptP(PG_VERSION_96, backupTimeStart, .backupStandby = true, .walCompressType = compressTypeGz);
            TEST_RESULT_VOID(cmdBackup(), "backup");

            // Set log level back to detail
            harnessLogLevelSet(logLevelDetail);

            TEST_RESULT_LOG(
                "P00   WARN: no prior backup exists, incr backup has been changed to full");

            TEST_RESULT_STR_Z(
                testBackupValidate(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ". {link, d=20191016-042640F}\n"
                "pg_data {path}\n"
                "pg_data/PG_VERSION {file, s=3}\n"
                "pg_data/backup_label {file, s=17}\n"
                "pg_data/base {path}\n"
                "pg_data/base/1 {path}\n"
                "pg_data/base/1/1 {file, s=0}\n"
                "pg_data/base/1/2 {file, s=2}\n"
                "pg_data/base/1/3 {file, s=3}\n"
                "pg_data/global {path}\n"
                "pg_data/global/pg_control {file, s=8192}\n"
                "pg_data/pg_xlog {path}\n"
                "pg_data/pg_xlog/0000000105DA69C000000000 {file, s=16777216}\n"
                "pg_data/postgresql.conf {file, s=11}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"f5b7e6d36dc0113f61b36c700817d42b96f7b037\",\"size\":3"
                    ",\"timestamp\":1571200000}\n"
                "pg_data/backup_label={\"checksum\":\"8e6f41ac87a7514be96260d65bacbffb11be77dc\",\"size\":17"
                    ",\"timestamp\":1571200002}\n"
                "pg_data/base/1/1={\"master\":false,\"size\":0,\"timestamp\":1571200000}\n"
                "pg_data/base/1/2={\"checksum\":\"54ceb91256e8190e474aa752a6e0650a2df5ba37\",\"master\":false,\"size\":2"
                    ",\"timestamp\":1571200000}\n"
                "pg_data/base/1/3={\"checksum\":\"3c01bdbb26f358bab27f267924aa2c9a03fcfdb8\",\"master\":false,\"size\":3"
                    ",\"timestamp\":1571200000}\n"
                "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1571200000}\n"
                "pg_data/pg_xlog/0000000105DA69C000000000={\"size\":16777216,\"timestamp\":1571200002}\n"
                "pg_data/postgresql.conf={\"checksum\":\"e3db315c260e79211b7b52587123b7aa060f30ab\",\"size\":11"
                    ",\"timestamp\":1570000000}\n"
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/base={}\n"
                "pg_data/base/1={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_xlog={}\n",
                "compare file list");

            // Remove test files
            HRN_STORAGE_PATH_REMOVE(storagePgIdxWrite(1), "", .recurse = true);
            HRN_STORAGE_PATH_REMOVE(storagePgWrite(), "base/1", .recurse = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 full backup with tablespaces and page checksums");

        backupTimeStart = BACKUP_EPOCH + 2200000;

        {
            // Update pg_control
            HRN_STORAGE_PUT(
                storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
                hrnPgControlToBuffer(
                    (PgControl){
                        .version = PG_VERSION_11, .systemId = 1000000000000001100, .pageChecksum = true,
                        .walSegmentSize = 1024 * 1024}),
                .timeModified = backupTimeStart);

            // Update version
            HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, PG_VERSION_11_STR, .timeModified = backupTimeStart);

            // Update wal path
            HRN_STORAGE_PATH_REMOVE(storagePgWrite(), strZ(pgWalPath(PG_VERSION_95)));
            HRN_STORAGE_PATH_CREATE(storagePgWrite(), strZ(pgWalPath(PG_VERSION_11)), .noParentCreate = true);

            // Upgrade stanza
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawBool(argList, cfgOptOnline, false);
            HRN_CFG_LOAD(cfgCmdStanzaUpgrade, argList);

            cmdStanzaUpgrade();
            TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'test1' on repo1");

            // Load options
            argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawBool(argList, cfgOptRepoHardlink, true);
            hrnCfgArgRawZ(argList, cfgOptManifestSaveThreshold, "1");
            hrnCfgArgRawBool(argList, cfgOptArchiveCopy, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Move pg1-path and put a link in its place. This tests that backup works when pg1-path is a symlink yet should be
            // completely invisible in the manifest and logging.
            HRN_SYSTEM_FMT("mv %s %s-data", strZ(pg1Path), strZ(pg1Path));
            HRN_SYSTEM_FMT("ln -s %s-data %s ", strZ(pg1Path), strZ(pg1Path));

            // Zeroed file which passes page checksums
            Buffer *relation = bufNew(PG_PAGE_SIZE_DEFAULT);
            memset(bufPtr(relation), 0, bufSize(relation));
            bufUsedSet(relation, bufSize(relation));

            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0};

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/1", relation, .timeModified = backupTimeStart);

            // Zeroed file which will fail on alignment
            relation = bufNew(PG_PAGE_SIZE_DEFAULT + 1);
            memset(bufPtr(relation), 0, bufSize(relation));
            bufUsedSet(relation, bufSize(relation));

            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0};

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/2", relation, .timeModified = backupTimeStart);

            // File with bad page checksums
            relation = bufNew(PG_PAGE_SIZE_DEFAULT * 4);
            memset(bufPtr(relation), 0, bufSize(relation));
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0xFF};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x01)) = (PageHeaderData){.pd_upper = 0x00};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x02)) = (PageHeaderData){.pd_upper = 0xFE};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x03)) = (PageHeaderData){.pd_upper = 0xEF};
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/3", relation, .timeModified = backupTimeStart);
            const char *rel1_3Sha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, relation)));

            // File with bad page checksum
            relation = bufNew(PG_PAGE_SIZE_DEFAULT * 3);
            memset(bufPtr(relation), 0, bufSize(relation));
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0x00};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x01)) = (PageHeaderData){.pd_upper = 0x08};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x02)) = (PageHeaderData){.pd_upper = 0x00};
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/4", relation, .timeModified = backupTimeStart);
            const char *rel1_4Sha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, relation)));

            // Add a tablespace
            HRN_STORAGE_PATH_CREATE(storagePgWrite(), PG_PATH_PGTBLSPC);
            THROW_ON_SYS_ERROR(
                symlink("../../pg1-tblspc/32768", strZ(storagePathP(storagePg(), STRDEF(PG_PATH_PGTBLSPC "/32768")))) == -1,
                FileOpenError, "unable to create symlink");

            HRN_STORAGE_PUT_EMPTY(
                storageTest,
                strZ(strNewFmt("pg1-tblspc/32768/%s/1/5", strZ(pgTablespaceId(PG_VERSION_11, hrnPgCatalogVersion(PG_VERSION_11))))),
                .timeModified = backupTimeStart);

            // Disable storageFeatureSymLink so tablespace (and latest) symlinks will not be created
            ((Storage *)storageRepoWrite())->pub.interface.feature ^= 1 << storageFeatureSymLink;

            // Disable storageFeatureHardLink so hardlinks will not be created
            ((Storage *)storageRepoWrite())->pub.interface.feature ^= 1 << storageFeatureHardLink;

            // Run backup
            testBackupPqScriptP(PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 3);
            TEST_RESULT_VOID(cmdBackup(), "backup");

            // Reset storage features
            ((Storage *)storageRepoWrite())->pub.interface.feature |= 1 << storageFeatureSymLink;
            ((Storage *)storageRepoWrite())->pub.interface.feature |= 1 << storageFeatureHardLink;

            TEST_RESULT_LOG(
                "P00   INFO: execute non-exclusive pg_start_backup(): backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DB5DE000000000, lsn = 5db5de0/0\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/3 (32KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: invalid page checksums found in file " TEST_PATH "/pg1/base/1/3 at pages 0, 2-3\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/4 (24KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: invalid page checksum found in file " TEST_PATH "/pg1/base/1/4 at page 1\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/2 (8KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: page misalignment in file " TEST_PATH "/pg1/base/1/2: file size 8193 is not divisible by page size"
                    " 8192\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/1 (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (2B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/pg_tblspc/32768/PG_11_201809051/1/5 (0B, [PCT])\n"
                "P00   INFO: full backup size = [SIZE]\n"
                "P00   INFO: execute non-exclusive pg_stop_backup() and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DB5DE000000002, lsn = 5db5de0/280000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from pg_stop_backup()\n"
                "P00   INFO: check archive for segment(s) 0000000105DB5DE000000000:0000000105DB5DE000000002\n"
                "P00 DETAIL: copy segment 0000000105DB5DE000000000 to backup\n"
                "P00 DETAIL: copy segment 0000000105DB5DE000000001 to backup\n"
                "P00 DETAIL: copy segment 0000000105DB5DE000000002 to backup\n"
                "P00   INFO: new backup label = 20191027-181320F");

            TEST_RESULT_STR(
                testBackupValidate(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/20191027-181320F")),
                strNewFmt(
                    "pg_data {path}\n"
                    "pg_data/PG_VERSION.gz {file, s=2}\n"
                    "pg_data/backup_label.gz {file, s=17}\n"
                    "pg_data/base {path}\n"
                    "pg_data/base/1 {path}\n"
                    "pg_data/base/1/1.gz {file, s=8192}\n"
                    "pg_data/base/1/2.gz {file, s=8193}\n"
                    "pg_data/base/1/3.gz {file, s=32768}\n"
                    "pg_data/base/1/4.gz {file, s=24576}\n"
                    "pg_data/global {path}\n"
                    "pg_data/global/pg_control.gz {file, s=8192}\n"
                    "pg_data/pg_tblspc {path}\n"
                    "pg_data/pg_wal {path}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000000.gz {file, s=1048576}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000001.gz {file, s=1048576}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000002.gz {file, s=1048576}\n"
                    "pg_data/postgresql.conf.gz {file, s=11}\n"
                    "pg_tblspc {path}\n"
                    "pg_tblspc/32768 {path}\n"
                    "pg_tblspc/32768/PG_11_201809051 {path}\n"
                    "pg_tblspc/32768/PG_11_201809051/1 {path}\n"
                    "pg_tblspc/32768/PG_11_201809051/1/5.gz {file, s=0}\n"
                    "--------\n"
                    "[backup:target]\n"
                    "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                    "pg_tblspc/32768={\"path\":\"../../pg1-tblspc/32768\",\"tablespace-id\":\"32768\""
                        ",\"tablespace-name\":\"tblspc32768\",\"type\":\"link\"}\n"
                    "\n"
                    "[target:file]\n"
                    "pg_data/PG_VERSION={\"checksum\":\"17ba0791499db908433b80f37c5fbc89b870084b\",\"size\":2"
                        ",\"timestamp\":1572200000}\n"
                    "pg_data/backup_label={\"checksum\":\"8e6f41ac87a7514be96260d65bacbffb11be77dc\",\"size\":17"
                        ",\"timestamp\":1572200002}\n"
                    "pg_data/base/1/1={\"checksum\":\"0631457264ff7f8d5fb1edc2c0211992a67c73e6\",\"checksum-page\":true"
                        ",\"master\":false,\"size\":8192,\"timestamp\":1572200000}\n"
                    "pg_data/base/1/2={\"checksum\":\"8beb58e08394fe665fb04a17b4003faa3802760b\",\"checksum-page\":false"
                        ",\"master\":false,\"size\":8193,\"timestamp\":1572200000}\n"
                    "pg_data/base/1/3={\"checksum\":\"%s\",\"checksum-page\":false,\"checksum-page-error\":[0,[2,3]]"
                        ",\"master\":false,\"size\":32768,\"timestamp\":1572200000}\n"
                    "pg_data/base/1/4={\"checksum\":\"%s\",\"checksum-page\":false,\"checksum-page-error\":[1],\"master\":false"
                        ",\"size\":24576,\"timestamp\":1572200000}\n"
                    "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1572200000}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000000={\"size\":1048576,\"timestamp\":1572200002}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000001={\"size\":1048576,\"timestamp\":1572200002}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000002={\"size\":1048576,\"timestamp\":1572200002}\n"
                    "pg_data/postgresql.conf={\"checksum\":\"e3db315c260e79211b7b52587123b7aa060f30ab\",\"size\":11"
                        ",\"timestamp\":1570000000}\n"
                    "pg_tblspc/32768/PG_11_201809051/1/5={\"checksum-page\":true,\"master\":false,\"size\":0"
                        ",\"timestamp\":1572200000}\n"
                    "\n"
                    "[target:link]\n"
                    "pg_data/pg_tblspc/32768={\"destination\":\"../../pg1-tblspc/32768\"}\n"
                    "\n"
                    "[target:path]\n"
                    "pg_data={}\n"
                    "pg_data/base={}\n"
                    "pg_data/base/1={}\n"
                    "pg_data/global={}\n"
                    "pg_data/pg_tblspc={}\n"
                    "pg_data/pg_wal={}\n"
                    "pg_tblspc={}\n"
                    "pg_tblspc/32768={}\n"
                    "pg_tblspc/32768/PG_11_201809051={}\n"
                    "pg_tblspc/32768/PG_11_201809051/1={}\n",
                    rel1_3Sha1, rel1_4Sha1),
                "compare file list");

            // Remove test files
            HRN_STORAGE_REMOVE(storagePgWrite(), "base/1/2", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "base/1/3", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "base/1/4", .errorOnMissing = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when pg_control not present");

        backupTimeStart = BACKUP_EPOCH + 2300000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeIncr);
            hrnCfgArgRawBool(argList, cfgOptRepoHardlink, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Run backup
            testBackupPqScriptP(PG_VERSION_11, backupTimeStart, .errorAfterStart = true);
            TEST_ERROR(
                cmdBackup(), FileMissingError,
                "pg_control must be present in all online backups\n"
                "HINT: is something wrong with the clock or filesystem timestamps?");

            // Check log
            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191027-181320F, version = " PROJECT_VERSION "\n"
                "P00   INFO: execute non-exclusive pg_start_backup(): backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DB764000000000, lsn = 5db7640/0");

            // Remove partial backup so it won't be resumed (since it errored before any checksums were written)
            HRN_STORAGE_PATH_REMOVE(storageRepoWrite(), STORAGE_REPO_BACKUP "/20191027-181320F_20191028-220000I", .recurse = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 incr backup with tablespaces");

        backupTimeStart = BACKUP_EPOCH + 2400000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, "/repo-bogus");
            hrnCfgArgKeyRaw(argList, cfgOptRepoPath, 2, repoPath);
            hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 2, "1");
            hrnCfgArgKeyRawBool(argList, cfgOptRepoHardlink, 2, true);
            hrnCfgArgRawZ(argList, cfgOptRepo, "2");
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeIncr);
            hrnCfgArgRawBool(argList, cfgOptDelta, true);
            hrnCfgArgRawBool(argList, cfgOptRepoHardlink, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Update pg_control timestamp
            HRN_STORAGE_TIME(storagePg(), "global/pg_control", backupTimeStart);

            // Run backup.  Make sure that the timeline selected converts to hexdecimal that can't be interpreted as decimal.
            testBackupPqScriptP(PG_VERSION_11, backupTimeStart, .timeline = 0x2C);
            TEST_RESULT_VOID(cmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191027-181320F, version = " PROJECT_VERSION "\n"
                "P00   INFO: execute non-exclusive pg_start_backup(): backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000002C05DB8EB000000000, lsn = 5db8eb0/0\n"
                "P00   WARN: a timeline switch has occurred since the 20191027-181320F backup, enabling delta checksum\n"
                "            HINT: this is normal after restoring from backup or promoting a standby.\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/base/1/1 (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/PG_VERSION (2B, [PCT]) checksum [SHA1]\n"
                "P00 DETAIL: hardlink pg_data/PG_VERSION to 20191027-181320F\n"
                "P00 DETAIL: hardlink pg_data/base/1/1 to 20191027-181320F\n"
                "P00 DETAIL: hardlink pg_data/global/pg_control to 20191027-181320F\n"
                "P00 DETAIL: hardlink pg_data/postgresql.conf to 20191027-181320F\n"
                "P00 DETAIL: hardlink pg_tblspc/32768/PG_11_201809051/1/5 to 20191027-181320F\n"
                "P00   INFO: incr backup size = [SIZE]\n"
                "P00   INFO: execute non-exclusive pg_stop_backup() and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000002C05DB8EB000000000, lsn = 5db8eb0/80000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from pg_stop_backup()\n"
                "P00   INFO: check archive for segment(s) 0000002C05DB8EB000000000:0000002C05DB8EB000000000\n"
                "P00   INFO: new backup label = 20191027-181320F_20191030-014640I");

            TEST_RESULT_STR_Z(
                testBackupValidate(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ". {link, d=20191027-181320F_20191030-014640I}\n"
                "pg_data {path}\n"
                "pg_data/PG_VERSION.gz {file, s=2}\n"
                "pg_data/backup_label.gz {file, s=17}\n"
                "pg_data/base {path}\n"
                "pg_data/base/1 {path}\n"
                "pg_data/base/1/1.gz {file, s=8192}\n"
                "pg_data/global {path}\n"
                "pg_data/global/pg_control.gz {file, s=8192}\n"
                "pg_data/pg_tblspc {path}\n"
                "pg_data/pg_tblspc/32768 {link, d=../../pg_tblspc/32768}\n"
                "pg_data/pg_wal {path}\n"
                "pg_data/postgresql.conf.gz {file, s=11}\n"
                "pg_tblspc {path}\n"
                "pg_tblspc/32768 {path}\n"
                "pg_tblspc/32768/PG_11_201809051 {path}\n"
                "pg_tblspc/32768/PG_11_201809051/1 {path}\n"
                "pg_tblspc/32768/PG_11_201809051/1/5.gz {file, s=0}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "pg_tblspc/32768={\"path\":\"../../pg1-tblspc/32768\",\"tablespace-id\":\"32768\""
                    ",\"tablespace-name\":\"tblspc32768\",\"type\":\"link\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"17ba0791499db908433b80f37c5fbc89b870084b\",\"reference\":\"20191027-181320F\""
                    ",\"size\":2,\"timestamp\":1572200000}\n"
                "pg_data/backup_label={\"checksum\":\"8e6f41ac87a7514be96260d65bacbffb11be77dc\",\"size\":17"
                    ",\"timestamp\":1572400002}\n"
                "pg_data/base/1/1={\"checksum\":\"0631457264ff7f8d5fb1edc2c0211992a67c73e6\",\"checksum-page\":true"
                    ",\"master\":false,\"reference\":\"20191027-181320F\",\"size\":8192,\"timestamp\":1572200000}\n"
                "pg_data/global/pg_control={\"reference\":\"20191027-181320F\",\"size\":8192,\"timestamp\":1572400000}\n"
                "pg_data/postgresql.conf={\"checksum\":\"e3db315c260e79211b7b52587123b7aa060f30ab\""
                    ",\"reference\":\"20191027-181320F\",\"size\":11,\"timestamp\":1570000000}\n"
                "pg_tblspc/32768/PG_11_201809051/1/5={\"checksum-page\":true,\"master\":false,\"reference\":\"20191027-181320F\""
                    ",\"size\":0,\"timestamp\":1572200000}\n"
                "\n"
                "[target:link]\n"
                "pg_data/pg_tblspc/32768={\"destination\":\"../../pg1-tblspc/32768\"}\n"
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/base={}\n"
                "pg_data/base/1={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_tblspc={}\n"
                "pg_data/pg_wal={}\n"
                "pg_tblspc={}\n"
                "pg_tblspc/32768={}\n"
                "pg_tblspc/32768/PG_11_201809051={}\n"
                "pg_tblspc/32768/PG_11_201809051/1={}\n",
                "compare file list");
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
