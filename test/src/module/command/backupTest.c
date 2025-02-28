/***********************************************************************************************************************************
Test Backup Command
***********************************************************************************************************************************/
#include "command/stanza/create.h"
#include "command/stanza/upgrade.h"
#include "common/crypto/hash.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "postgres/interface/static.vendor.h"
#include "storage/helper.h"
#include "storage/posix/storage.h"

#include "common/harnessBackup.h"
#include "common/harnessBlockIncr.h"
#include "common/harnessConfig.h"
#include "common/harnessManifest.h"
#include "common/harnessPack.h"
#include "common/harnessPostgres.h"
#include "common/harnessPq.h"
#include "common/harnessProtocol.h"
#include "common/harnessStorage.h"
#include "common/harnessTime.h"

/***********************************************************************************************************************************
Get a list of all files in the backup and a redacted version of the manifest that can be tested against a static string
***********************************************************************************************************************************/
static String *
testBackupValidateFile(
    const Storage *const storage, const String *const path, Manifest *const manifest, const ManifestData *const manifestData,
    const String *const fileName, uint64_t fileSize, ManifestFilePack **const filePack, const CipherType cipherType,
    const String *const cipherPass)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STORAGE, storage);
        FUNCTION_HARNESS_PARAM(STRING, path);
        FUNCTION_HARNESS_PARAM(MANIFEST, manifest);
        FUNCTION_HARNESS_PARAM_P(VOID, manifestData);
        FUNCTION_HARNESS_PARAM(STRING, fileName);
        FUNCTION_HARNESS_PARAM(UINT64, fileSize);
        FUNCTION_HARNESS_PARAM_P(VOID, filePack);
        FUNCTION_HARNESS_PARAM(STRING_ID, cipherType);
        FUNCTION_HARNESS_PARAM(STRING, cipherPass);
    FUNCTION_HARNESS_END();

    String *const result = strNew();
    ManifestFile file = manifestFileUnpack(manifest, *filePack);

    // Output name and size
    // -------------------------------------------------------------------------------------------------------------
    if (file.bundleId != 0)
        strCatFmt(result, "%s/%s {", strZ(fileName), strZ(file.name));
    else
        strCatFmt(result, "%s {", strZ(fileName));

    strCatFmt(result, "s=%" PRIu64, file.size);

    if (file.sizeOriginal != file.size)
        strCatFmt(result, ", so=%" PRIu64, file.sizeOriginal);

    // Validate repo checksum
    // -------------------------------------------------------------------------------------------------------------
    if (file.checksumRepoSha1 != NULL)
    {
        StorageRead *read = storageNewReadP(
            storage, strNewFmt("%s/%s", strZ(path), strZ(fileName)), .offset = file.bundleOffset,
            .limit = VARUINT64(file.sizeRepo));
        const Buffer *const checksum = cryptoHashOne(hashTypeSha1, storageGetP(read));

        if (!bufEq(checksum, BUF(file.checksumRepoSha1, HASH_TYPE_SHA1_SIZE)))
            THROW_FMT(AssertError, "'%s' repo checksum does match manifest", strZ(file.name));
    }

    // Calculate checksum/size and decompress if needed
    // -------------------------------------------------------------------------------------------------------------
    uint64_t size = 0;
    const Buffer *checksum = NULL;

    // If block incremental
    if (file.blockIncrMapSize != 0)
    {
        // Read block map
        StorageRead *read = storageNewReadP(
            storage, strNewFmt("%s/%s", strZ(path), strZ(fileName)),
            .offset = file.bundleOffset + file.sizeRepo - file.blockIncrMapSize,
            .limit = VARUINT64(file.blockIncrMapSize));

        if (cipherType != cipherTypeNone)
        {
            ioFilterGroupAdd(
                ioReadFilterGroup(storageReadIo(read)),
                cipherBlockNewP(cipherModeDecrypt, cipherType, BUFSTR(cipherPass), .raw = true));
        }

        ioReadOpen(storageReadIo(read));

        const BlockMap *const blockMap = blockMapNewRead(
            storageReadIo(read), file.blockIncrSize, file.blockIncrChecksumSize);

        // Build map log
        String *const mapLog = strNew();
        const BlockMapItem *blockMapItemLast = NULL;

        for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(blockMap); blockMapIdx++)
        {
            const BlockMapItem *const blockMapItem = blockMapGet(blockMap, blockMapIdx);
            const bool superBlockChange =
                blockMapItemLast == NULL || blockMapItemLast->reference != blockMapItem->reference ||
                blockMapItemLast->offset != blockMapItem->offset;

            if (superBlockChange && blockMapIdx != 0)
                strCatChr(mapLog, '}');

            if (!strEmpty(mapLog))
                strCatChr(mapLog, ',');

            if (superBlockChange)
                strCatFmt(mapLog, "%u:{", blockMapItem->reference);

            strCatFmt(mapLog, "%" PRIu64, blockMapItem->block);

            blockMapItemLast = blockMapItem;
        }

        // Check blocks
        Buffer *fileBuffer = bufNew((size_t)file.size);
        bufUsedSet(fileBuffer, bufSize(fileBuffer));

        BlockDelta *const blockDelta = blockDeltaNew(
            blockMap, file.blockIncrSize, file.blockIncrChecksumSize, NULL, cipherType, cipherPass,
            manifestData->backupOptionCompressType);

        for (unsigned int readIdx = 0; readIdx < blockDeltaReadSize(blockDelta); readIdx++)
        {
            const BlockDeltaRead *const read = blockDeltaReadGet(blockDelta, readIdx);
            const String *const blockName = backupFileRepoPathP(
                strLstGet(manifestReferenceList(manifest), read->reference), .manifestName = file.name,
                .bundleId = read->bundleId, .blockIncr = true);

            IoRead *blockRead = storageReadIo(
                storageNewReadP(
                    storage, blockName, .offset = read->offset, .limit = VARUINT64(read->size)));
            ioReadOpen(blockRead);

            const BlockDeltaWrite *deltaWrite = blockDeltaNext(blockDelta, read, blockRead);

            while (deltaWrite != NULL)
            {
                // Update size and file
                size += bufUsed(deltaWrite->block);
                memcpy(
                    bufPtr(fileBuffer) + deltaWrite->offset, bufPtr(deltaWrite->block),
                    bufUsed(deltaWrite->block));

                deltaWrite = blockDeltaNext(blockDelta, read, blockRead);
            }
        }

        strCatFmt(result, ", m=%s}", strZ(mapLog));

        checksum = cryptoHashOne(hashTypeSha1, fileBuffer);
    }
    // Else normal file
    else
    {
        StorageRead *read = storageNewReadP(
            storage, strNewFmt("%s/%s", strZ(path), strZ(fileName)), .offset = file.bundleOffset,
            .limit = VARUINT64(file.sizeRepo));
        const bool raw = file.bundleId != 0 && manifest->pub.data.bundleRaw;

        if (cipherType != cipherTypeNone)
        {
            ioFilterGroupAdd(
                ioReadFilterGroup(storageReadIo(read)),
                cipherBlockNewP(cipherModeDecrypt, cipherType, BUFSTR(cipherPass), .raw = raw));
        }

        if (manifestData->backupOptionCompressType != compressTypeNone)
        {
            ioFilterGroupAdd(
                ioReadFilterGroup(storageReadIo(read)),
                decompressFilterP(manifestData->backupOptionCompressType, .raw = raw));
        }

        ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), cryptoHashNew(hashTypeSha1));

        size = bufUsed(storageGetP(read));
        checksum = pckReadBinP(
            ioFilterGroupResultP(ioReadFilterGroup(storageReadIo(read)), CRYPTO_HASH_FILTER_TYPE));
    }

    // Validate checksum
    if (!bufEq(checksum, BUF(file.checksumSha1, HASH_TYPE_SHA1_SIZE)))
        THROW_FMT(AssertError, "'%s' checksum does match manifest", strZ(file.name));

    // Test size and repo-size
    // -------------------------------------------------------------------------------------------------------------
    if (size != file.size)
        THROW_FMT(AssertError, "'%s' size does match manifest", strZ(file.name));

    // Repo size can only be compared to file size when not bundled
    if (file.bundleId == 0 && fileSize != file.sizeRepo)
        THROW_FMT(AssertError, "'%s' repo size does match manifest", strZ(file.name));

    // Timestamp
    // -------------------------------------------------------------------------------------------------------------
    if (file.timestamp != manifestData->backupTimestampStart)
    {
        strCatFmt(
            result, ", ts=%s%" PRId64, file.timestamp > manifestData->backupTimestampStart ? "+" : "",
            (int64_t)file.timestamp - (int64_t)manifestData->backupTimestampStart);
    }

    // Page checksum
    // -------------------------------------------------------------------------------------------------------------
    if (file.checksumPage)
    {
        strCatZ(result, ", ckp=");

        if (file.checksumPageError)
        {
            if (file.checksumPageErrorList != NULL)
                strCat(result, file.checksumPageErrorList);
            else
                strCatZ(result, "t");
        }
        else
            strCatZ(result, "t");
    }

    // pg_control and WAL headers have different checksums depending on cpu architecture so remove the checksum from
    // the test output.
    // -------------------------------------------------------------------------------------------------------------
    if (strEqZ(file.name, MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL) ||
        strBeginsWith(
            file.name, strNewFmt(MANIFEST_TARGET_PGDATA "/%s/", strZ(pgWalPath(manifestData->pgVersion)))))
    {
        file.checksumSha1 = NULL;
    }

    strCatZ(result, "}\n");

    // Update changes to manifest file
    manifestFilePackUpdate(manifest, filePack, &file);

    FUNCTION_HARNESS_RETURN(STRING, result);
}

static String *
testBackupValidateList(
    const Storage *const storage, const String *const path, Manifest *const manifest, const ManifestData *const manifestData,
    StringList *const manifestFileList, const CipherType cipherType, const String *const cipherPass, String *const result)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STORAGE, storage);
        FUNCTION_HARNESS_PARAM(STRING, path);
        FUNCTION_HARNESS_PARAM(MANIFEST, manifest);
        FUNCTION_HARNESS_PARAM_P(VOID, manifestData);
        FUNCTION_HARNESS_PARAM(STRING_LIST, manifestFileList);
        FUNCTION_HARNESS_PARAM(STRING_ID, cipherType);
        FUNCTION_HARNESS_PARAM(STRING, cipherPass);
        FUNCTION_HARNESS_PARAM(STRING, result);
    FUNCTION_HARNESS_END();

    // Output root path if it is a link so we can verify the destination
    const StorageInfo dotInfo = storageInfoP(storage, path);

    if (dotInfo.type == storageTypeLink)
        strCatFmt(result, ".> {d=%s}\n", strZ(dotInfo.linkDestination));

    // Output path contents
    StorageIterator *const storageItr = storageNewItrP(storage, path, .recurse = true, .sortOrder = sortOrderAsc);

    while (storageItrMore(storageItr))
    {
        const StorageInfo info = storageItrNext(storageItr);

        // Don't include backup.manifest or copy. We'll test that they are present elsewhere
        if (info.type == storageTypeFile &&
            (strEqZ(info.name, BACKUP_MANIFEST_FILE) || strEqZ(info.name, BACKUP_MANIFEST_FILE INFO_COPY_EXT)))
        {
            continue;
        }

        switch (info.type)
        {
            case storageTypeFile:
            {
                // Test mode, user, group. These values are not in the manifest but we know what they should be based on the default
                // mode and current user/group.
                // -----------------------------------------------------------------------------------------------------------------
                if (info.mode != 0640)
                    THROW_FMT(AssertError, "'%s' mode is not 0640", strZ(info.name));

                if (!strEq(info.user, TEST_USER_STR))
                    THROW_FMT(AssertError, "'%s' user should be '" TEST_USER "'", strZ(info.name));

                if (!strEq(info.group, TEST_GROUP_STR))
                    THROW_FMT(AssertError, "'%s' group should be '" TEST_GROUP "'", strZ(info.name));

                // Build file list (needed because bundles can contain multiple files)
                // -----------------------------------------------------------------------------------------------------------------
                List *const fileList = lstNewP(sizeof(ManifestFilePack **));
                bool bundle = strBeginsWithZ(info.name, "bundle/");

                if (bundle)
                {
                    const uint64_t bundleId = cvtZToUInt64(strZ(info.name) + sizeof("bundle"));

                    for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
                    {
                        ManifestFilePack **const filePack = lstGet(manifest->pub.fileList, fileIdx);
                        ManifestFile file = manifestFileUnpack(manifest, *filePack);

                        // File bundle is part of this backup
                        if (file.bundleId == bundleId && file.reference == NULL)
                            lstAdd(fileList, &filePack);
                    }
                }
                else
                {
                    const String *manifestName = info.name;

                    // Remove block incremental extension
                    if (strEndsWithZ(info.name, BACKUP_BLOCK_INCR_EXT))
                    {
                        manifestName = strSubN(info.name, 0, strSize(info.name) - (sizeof(BACKUP_BLOCK_INCR_EXT) - 1));
                    }
                    // Else remove compression extension
                    else if (manifestData->backupOptionCompressType != compressTypeNone)
                    {
                        manifestName = strSubN(
                            info.name, 0, strSize(info.name) - strSize(compressExtStr(manifestData->backupOptionCompressType)));
                    }

                    ManifestFilePack **const filePack = manifestFilePackFindInternal(manifest, manifestName);
                    lstAdd(fileList, &filePack);
                }

                // Check files
                // -----------------------------------------------------------------------------------------------------------------
                for (unsigned int fileIdx = 0; fileIdx < lstSize(fileList); fileIdx++)
                {
                    // Remove this file from manifest file list to track what has been updated
                    ManifestFilePack **const filePack = *(ManifestFilePack ***)lstGet(fileList, fileIdx);
                    const unsigned int manifestFileIdx = strLstFindIdxP(
                        manifestFileList, (String *const)*filePack, .required = true);
                    strLstRemoveIdx(manifestFileList, manifestFileIdx);

                    strCat(
                        result,
                        testBackupValidateFile(
                            storage, path, manifest, manifestData, info.name, info.size, filePack, cipherType, cipherPass));
                }

                break;
            }

            case storageTypeLink:
                strCatFmt(result, "%s> {d=%s}\n", strZ(info.name), strZ(info.linkDestination));
                break;

            case storageTypePath:
            {
                // Add path to output only when it is empty -- otherwise it is implied by the files below it
                if (strLstEmpty(storageListP(storage, storagePathP(storage, strNewFmt("%s/%s", strZ(path), strZ(info.name))))))
                    strCatFmt(result, "%s/\n", strZ(info.name));

                // Check against the manifest
                // -----------------------------------------------------------------------------------------------------------------
                if (!strEq(info.name, STRDEF("bundle")))
                    manifestPathFind(manifest, info.name);

                // Test mode, user, group. These values are not in the manifest but we know what they should be based on the default
                // mode and current user/group.
                if (info.mode != 0750)
                    THROW_FMT(AssertError, "'%s' mode is not 00750", strZ(info.name));

                if (!strEq(info.user, TEST_USER_STR))
                    THROW_FMT(AssertError, "'%s' user should be '" TEST_USER "'", strZ(info.name));

                if (!strEq(info.group, TEST_GROUP_STR))
                    THROW_FMT(AssertError, "'%s' group should be '" TEST_GROUP "'", strZ(info.name));

                break;
            }

            case storageTypeSpecial:
                THROW_FMT(AssertError, "unexpected special file '%s'", strZ(info.name));
        }
    }

    FUNCTION_HARNESS_RETURN(STRING, result);
}

typedef struct TestBackupValidateParam
{
    VAR_PARAM_HEADER;
    CipherType cipherType;                                          // Cipher type
    const char *cipherPass;                                         // Cipher pass
} TestBackupValidateParam;

#define testBackupValidateP(storage, path, ...)                                                                                    \
    testBackupValidate(storage, path, (TestBackupValidateParam){VAR_PARAM_INIT, __VA_ARGS__})

static String *
testBackupValidate(const Storage *const storage, const String *const path, const TestBackupValidateParam param)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STORAGE, storage);
        FUNCTION_HARNESS_PARAM(STRING, path);
        FUNCTION_HARNESS_PARAM(UINT64, param.cipherType);
        FUNCTION_HARNESS_PARAM(STRINGZ, param.cipherPass);
    FUNCTION_HARNESS_END();

    ASSERT(storage != NULL);
    ASSERT(path != NULL);

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build a list of files in the backup path and verify against the manifest
        // -------------------------------------------------------------------------------------------------------------------------
        const InfoBackup *const infoBackup = infoBackupLoadFile(
            storageRepo(), INFO_BACKUP_PATH_FILE_STR, param.cipherType == 0 ? cipherTypeNone : param.cipherType,
            param.cipherPass == NULL ? NULL : STR(param.cipherPass));
        Manifest *manifest = manifestLoadFile(
            storage, strNewFmt("%s/" BACKUP_MANIFEST_FILE, strZ(path)), param.cipherType == 0 ? cipherTypeNone : param.cipherType,
            param.cipherPass == NULL ? NULL : infoBackupCipherPass(infoBackup));

        // Build list of files in the manifest
        StringList *const manifestFileList = strLstNew();

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
            strLstAdd(manifestFileList, manifestFileUnpack(manifest, manifestFilePackGet(manifest, fileIdx)).name);

        // Validate files on disk against the manifest
        const CipherType cipherType = param.cipherType == 0 ? cipherTypeNone : param.cipherType;
        const String *const cipherPass = param.cipherPass == NULL ? NULL : manifestCipherSubPass(manifest);

        testBackupValidateList(storage, path, manifest, manifestData(manifest), manifestFileList, cipherType, cipherPass, result);

        // Check remaining files in the manifest -- these should all be references
        for (unsigned int manifestFileIdx = 0; manifestFileIdx < strLstSize(manifestFileList); manifestFileIdx++)
        {
            ManifestFilePack **const filePack = manifestFilePackFindInternal(
                manifest, strLstGet(manifestFileList, manifestFileIdx));
            const ManifestFile file = manifestFileUnpack(manifest, *filePack);

            // No need to check zero-length files in bundled backups
            if (manifestData(manifest)->bundle && file.size == 0)
                continue;

            // Error if reference is NULL
            if (file.reference == NULL)
                THROW_FMT(AssertError, "manifest file '%s' not in backup but does not have a reference", strZ(file.name));

            strCat(
                result,
                testBackupValidateFile(
                    storage, strPath(path), manifest, manifestData(manifest),
                    strSub(
                        backupFileRepoPathP(
                            file.reference,
                            .manifestName = file.name, .bundleId = file.bundleId,
                            .compressType = manifestData(manifest)->backupOptionCompressType,
                            .blockIncr = file.blockIncrMapSize != 0),
                        sizeof(STORAGE_REPO_BACKUP)),
                    file.sizeRepo, filePack, cipherType, cipherPass));
        }

        // Make sure both backup.manifest files exist since we skipped them in the callback above
        if (!storageExistsP(storage, strNewFmt("%s/" BACKUP_MANIFEST_FILE, strZ(path))))
            THROW(AssertError, BACKUP_MANIFEST_FILE " is missing");

        if (!storageExistsP(storage, strNewFmt("%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(path))))
            THROW(AssertError, BACKUP_MANIFEST_FILE INFO_COPY_EXT " is missing");

        // Update manifest to make the output a bit simpler
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            ManifestFilePack **const filePack = lstGet(manifest->pub.fileList, fileIdx);
            ManifestFile file = manifestFileUnpack(manifest, *filePack);

            // If compressed or block incremental then set the repo-size to size so it will not be in test output. Even the same
            // compression algorithm can give slightly different results based on the version so repo-size is not deterministic for
            // compression. Block incremental maps increase repo-size in a non-obvious way.
            if (manifestData(manifest)->backupOptionCompressType != compressTypeNone || file.blockIncrMapSize != 0)
                file.sizeRepo = file.size;

            // Bundle id/offset are too noisy so remove them. They are verified against size/checksum and listed with the files.
            file.bundleId = 0;
            file.bundleOffset = 0;

            // Remove repo checksum since it has been validated
            file.checksumRepoSha1 = NULL;

            // Update changes to manifest file
            manifestFilePackUpdate(manifest, filePack, &file);
        }

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

                if (strEqZ(section, INFO_SECTION_BACKREST) ||
                    strEqZ(section, INFO_SECTION_CIPHER) ||
                    strEqZ(section, MANIFEST_SECTION_BACKUP) ||
                    strEqZ(section, MANIFEST_SECTION_BACKUP_DB) ||
                    strEqZ(section, MANIFEST_SECTION_BACKUP_OPTION) ||
                    strEqZ(section, MANIFEST_SECTION_DB) ||
                    strEqZ(section, MANIFEST_SECTION_TARGET_FILE) ||
                    strEqZ(section, MANIFEST_SECTION_TARGET_FILE_DEFAULT) ||
                    strEqZ(section, MANIFEST_SECTION_TARGET_LINK) ||
                    strEqZ(section, MANIFEST_SECTION_TARGET_LINK_DEFAULT) ||
                    strEqZ(section, MANIFEST_SECTION_TARGET_PATH) ||
                    strEqZ(section, MANIFEST_SECTION_TARGET_PATH_DEFAULT))
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
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Install local command handler shim
    static const ProtocolServerHandler testLocalHandlerList[] = {PROTOCOL_SERVER_HANDLER_BACKUP_LIST};
    hrnProtocolLocalShimInstall(testLocalHandlerList, LENGTH_OF(testLocalHandlerList));

    // The tests expect the timezone to be UTC
    hrnTzSet("UTC");

    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("backupRegExp()"))
    {
        const String *full = STRDEF("20181119-152138F");
        const String *incr = STRDEF("20181119-152138F_20181119-152152I");
        const String *diff = STRDEF("20181119-152138F_20181119-152152D");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("regular expression - error");

        TEST_ERROR(
            backupRegExpP(0),
            AssertError, "assertion 'param.full || param.differential || param.incremental' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("regular expression - match full");

        String *filter = backupRegExpP(.full = true);
        TEST_RESULT_STR_Z(filter, "^[0-9]{8}\\-[0-9]{6}F$", "full backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), false, "does not exactly match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), false, "does not exactly match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "exactly matches full");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("regular expression - match full, incremental");

        filter = backupRegExpP(.full = true, .incremental = true);

        TEST_RESULT_STR_Z(
            filter, "^[0-9]{8}\\-[0-9]{6}F(\\_[0-9]{8}\\-[0-9]{6}I){0,1}$", "full and optional incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), false, "does not match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "match full");
        TEST_RESULT_BOOL(
            regExpMatchOne(
                filter, STRDEF("12341234-123123F_12341234-123123IG")), false, "does not match with trailing character");
        TEST_RESULT_BOOL(
            regExpMatchOne(
                filter, STRDEF("A12341234-123123F_12341234-123123I")), false, "does not match with leading character");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("regular expression - match full, differential");

        filter = backupRegExpP(.full = true, .differential = true);

        TEST_RESULT_STR_Z(
            filter, "^[0-9]{8}\\-[0-9]{6}F(\\_[0-9]{8}\\-[0-9]{6}D){0,1}$", "full and optional diff backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), false, "does not match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "match full");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("regular expression - match full, incremental, differential");

        filter = backupRegExpP(.full = true, .incremental = true, .differential = true);

        TEST_RESULT_STR_Z(
            filter, "^[0-9]{8}\\-[0-9]{6}F(\\_[0-9]{8}\\-[0-9]{6}(D|I)){0,1}$",
            "full, optional diff and incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "match full");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("regular expression - match incremental, differential without end anchor");

        filter = backupRegExpP(.incremental = true, .differential = true, .noAnchorEnd = true);

        TEST_RESULT_STR_Z(filter, "^[0-9]{8}\\-[0-9]{6}F\\_[0-9]{8}\\-[0-9]{6}(D|I)", "diff and incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), false, "does not match full");
        TEST_RESULT_BOOL(
            regExpMatchOne(
                filter, STRDEF("A12341234-123123F_12341234-123123I")), false, "does not match with leading character");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("regular expression - match incremental");

        filter = backupRegExpP(.incremental = true);

        TEST_RESULT_STR_Z(filter, "^[0-9]{8}\\-[0-9]{6}F\\_[0-9]{8}\\-[0-9]{6}I$", "incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), false, "does not match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), false, "does not match full");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("regular expression - match differential");

        filter = backupRegExpP(.differential = true);

        TEST_RESULT_STR_Z(filter, "^[0-9]{8}\\-[0-9]{6}F\\_[0-9]{8}\\-[0-9]{6}D$", "diff backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), false, "does not match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), false, "does not match full");
    }

    // *****************************************************************************************************************************
    if (testBegin("PageChecksum"))
    {
        #define PG_SEGMENT_PAGE_DEFAULT                             (PG_SEGMENT_SIZE_DEFAULT / pgPageSize8)

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("segment page default");

        TEST_RESULT_UINT(PG_SEGMENT_PAGE_DEFAULT, 131072, "check pages per segment");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("two misaligned buffers in a row");

        Buffer *buffer = bufNew(513);
        bufUsedSet(buffer, bufSize(buffer));
        memset(bufPtr(buffer), 0, bufSize(buffer));

        *(PageHeaderData *)(bufPtr(buffer) + (pgPageSize8 * 0x00)) = (PageHeaderData){.pd_upper = 0};

        Buffer *bufferOut = bufNew(513);
        IoWrite *write = ioBufferWriteNew(bufferOut);
        ioFilterGroupAdd(
            ioWriteFilterGroup(write),
            pageChecksumNewPack(
                ioFilterParamList(pageChecksumNew(0, PG_SEGMENT_PAGE_DEFAULT, pgPageSize8, true, STRDEF(BOGUS_STR)))));
        ioWriteOpen(write);
        ioWrite(write, buffer);
        TEST_ERROR(ioWrite(write, buffer), AssertError, "should not be possible to see two misaligned pages in a row");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retry a page with an invalid checksum");

        // Write to file with valid checksums
        buffer = bufNew(pgPageSize8 * 4);
        memset(bufPtr(buffer), 0, bufSize(buffer));
        bufUsedSet(buffer, bufSize(buffer));

        *(PageHeaderData *)(bufPtr(buffer) + (pgPageSize8 * 0x00)) = (PageHeaderData){.pd_upper = 0x00};
        *(PageHeaderData *)(bufPtr(buffer) + (pgPageSize8 * 0x01)) = (PageHeaderData){.pd_upper = 0xFF};
        ((PageHeaderData *)(bufPtr(buffer) + (pgPageSize8 * 0x01)))->pd_checksum = pgPageChecksum(
            bufPtr(buffer) + (pgPageSize8 * 0x01), 1, pgPageSize8);
        *(PageHeaderData *)(bufPtr(buffer) + (pgPageSize8 * 0x02)) = (PageHeaderData){.pd_upper = 0x00};
        *(PageHeaderData *)(bufPtr(buffer) + (pgPageSize8 * 0x03)) = (PageHeaderData){.pd_upper = 0xFE};
        ((PageHeaderData *)(bufPtr(buffer) + (pgPageSize8 * 0x03)))->pd_checksum = pgPageChecksum(
            bufPtr(buffer) + (pgPageSize8 * 0x03), 3, pgPageSize8);

        HRN_STORAGE_PUT(storageTest, "relation", buffer);

        // Now break the checksum to force a retry
        ((PageHeaderData *)(bufPtr(buffer) + (pgPageSize8 * 0x01)))->pd_checksum = 0;
        ((PageHeaderData *)(bufPtr(buffer) + (pgPageSize8 * 0x03)))->pd_checksum = 0;

        write = ioBufferWriteNew(bufferOut);
        ioFilterGroupAdd(
            ioWriteFilterGroup(write),
            pageChecksumNew(0, PG_SEGMENT_PAGE_DEFAULT, pgPageSize8, true, storagePathP(storageTest, STRDEF("relation"))));
        ioWriteOpen(write);
        ioWrite(write, buffer);
        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            hrnPackToStr(ioFilterGroupResultPackP(ioWriteFilterGroup(write), PAGE_CHECKSUM_FILTER_TYPE)),
            "2:bool:true, 3:bool:true", "valid on retry");
    }

    // *****************************************************************************************************************************
    if (testBegin("segmentNumber()"))
    {
        TEST_RESULT_UINT(segmentNumber(STRDEF("999")), 0, "No segment number");
        TEST_RESULT_UINT(segmentNumber(STRDEF("999.123")), 123, "Segment number");
    }

    // *****************************************************************************************************************************
    if (testBegin("BlockMap"))
    {
        TEST_TITLE("build equal block map");

        BlockMap *blockMap = NULL;
        TEST_ASSIGN(blockMap, blockMapNew(), "new");

        BlockMapItem blockMapItem =
        {
            .reference = 128,
            .superBlockSize = 1,
            .bundleId = 0,
            .offset = 0,
            .size = 3,
            .checksum = {0xee, 0xee, 0x01, 0xff, 0xff},
        };

        TEST_RESULT_UINT(blockMapAdd(blockMap, &blockMapItem)->reference, 128, "add");
        TEST_RESULT_UINT(blockMapGet(blockMap, 0)->reference, 128, "get");

        blockMapItem = (BlockMapItem)
        {
            .reference = 128,
            .superBlockSize = 1,
            .bundleId = 0,
            .offset = 3,
            .size = 5,
            .checksum = {0xee, 0xee, 0x02, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        blockMapItem = (BlockMapItem)
        {
            .reference = 0,
            .superBlockSize = 1,
            .bundleId = 1,
            .offset = 1,
            .size = 5,
            .checksum = {0xee, 0xee, 0x03, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        blockMapItem = (BlockMapItem)
        {
            .reference = 128,
            .superBlockSize = 1,
            .bundleId = 0,
            .offset = 8,
            .size = 99,
            .checksum = {0xee, 0xee, 0x04, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        blockMapItem = (BlockMapItem)
        {
            .reference = 0,
            .superBlockSize = 1,
            .bundleId = 1,
            .offset = 7,
            .size = 99,
            .checksum = {0xee, 0xee, 0x05, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        // Keep this last
        blockMapItem = (BlockMapItem)
        {
            .reference = 4,
            .superBlockSize = 1,
            .bundleId = 0,
            .offset = 0,
            .size = 8,
            .checksum = {0xee, 0xee, 0x88, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write equal block map");

        Buffer *buffer = bufNew(256);
        IoWrite *write = ioBufferWriteNewOpen(buffer);
        TEST_RESULT_VOID(blockMapWrite(blockMap, write, 1, 5), "save");
        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, buffer),
            "00"                                        // Version 0

            "8008"                                      // reference 128
            "18"                                        // size 3
            "eeee01ffff"                                // checksum
            "21"                                        // size
            "eeee02ffff"                                // checksum

            "06"                                        // reference 0
            "01"                                        // bundle 1
            "01"                                        // offset 1
            "01"                                        // size 1
            "eeee03ffff"                                // checksum

            "8008"                                      // reference 128
            "e10b"                                      // size 99
            "eeee04ffff"                                // checksum

            "04"                                        // reference 0
            "01"                                        // offset 7
            "01"                                        // size 99
            "eeee05ffff"                                // checksum

            "21"                                        // reference 0
            "a90b"                                      // size 8
            "eeee88ffff",                               // checksum
            "compare");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read equal block map");

        Buffer *bufferCompare = bufNew(256);
        write = ioBufferWriteNewOpen(bufferCompare);
        TEST_RESULT_VOID(blockMapWrite(blockMapNewRead(ioBufferReadNewOpen(buffer), 1, 5), write, 1, 5), "read and save");
        ioWriteClose(write);

        TEST_RESULT_STR(strNewEncode(encodingHex, bufferCompare), strNewEncode(encodingHex, buffer), "compare");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("equal block delta");

        TEST_RESULT_STR_Z(
            hrnBlockDeltaRender(blockMapNewRead(ioBufferReadNewOpen(buffer), 1, 5), 1, 5),
            "read {reference: 128, bundleId: 0, offset: 0, size: 107}\n"
            "  super block {max: 1, size: 3}\n"
            "    block {no: 0, offset: 0}\n"
            "  super block {max: 1, size: 5}\n"
            "    block {no: 0, offset: 1}\n"
            "  super block {max: 1, size: 99}\n"
            "    block {no: 0, offset: 3}\n"
            "read {reference: 4, bundleId: 0, offset: 0, size: 8}\n"
            "  super block {max: 1, size: 8}\n"
            "    block {no: 0, offset: 5}\n"
            "read {reference: 0, bundleId: 1, offset: 1, size: 5}\n"
            "  super block {max: 1, size: 5}\n"
            "    block {no: 0, offset: 2}\n"
            "read {reference: 0, bundleId: 1, offset: 7, size: 99}\n"
            "  super block {max: 1, size: 99}\n"
            "    block {no: 0, offset: 4}\n",
            "check delta");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("build unequal block map");

        TEST_ASSIGN(blockMap, blockMapNew(), "new");

        blockMapItem = (BlockMapItem)
        {
            .reference = 0,
            .superBlockSize = 6,
            .offset = 0,
            .size = 4,
            .block = 0,
            .checksum = {0xee, 0xee, 0x01, 0, 0, 0, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        blockMapItem = (BlockMapItem)
        {
            .reference = 0,
            .superBlockSize = 6,
            .offset = 0,
            .size = 4,
            .block = 1,
            .checksum = {0xee, 0xee, 0x02, 0, 0, 0, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        blockMapItem = (BlockMapItem)
        {
            .reference = 0,
            .superBlockSize = 6,
            .offset = 4,
            .size = 5,
            .block = 0,
            .checksum = {0xee, 0xee, 0x03, 0, 0, 0, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        blockMapItem = (BlockMapItem)
        {
            .reference = 1,
            .superBlockSize = 3,
            .offset = 0,
            .size = 99,
            .block = 0,
            .checksum = {0xee, 0xee, 0x04, 0, 0, 0, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        blockMapItem = (BlockMapItem)
        {
            .reference = 0,
            .superBlockSize = 6,
            .offset = 4,
            .size = 5,
            .block = 3,
            .checksum = {0xee, 0xee, 0x05, 0, 0, 0, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        blockMapItem = (BlockMapItem)
        {
            .reference = 2,
            .superBlockSize = 2,
            .offset = 0,
            .size = 1,
            .block = 0,
            .checksum = {0xee, 0xee, 0x06, 0, 0, 0, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        blockMapItem = (BlockMapItem)
        {
            .reference = 0,
            .superBlockSize = 6,
            .offset = 4,
            .size = 5,
            .block = 5,
            .checksum = {0xee, 0xee, 0x07, 0, 0, 0, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        blockMapItem = (BlockMapItem)
        {
            .reference = 0,
            .superBlockSize = 2,
            .offset = 9,
            .size = 6,
            .block = 0,
            .checksum = {0xee, 0xee, 0x08, 0, 0, 0, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        blockMapItem = (BlockMapItem)
        {
            .reference = 1,
            .superBlockSize = 3,
            .offset = 99,
            .size = 1,
            .block = 1,
            .checksum = {0xee, 0xee, 0x09, 0, 0, 0, 0xff, 0xff},
        };

        TEST_RESULT_VOID(blockMapAdd(blockMap, &blockMapItem), "add");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write unequal block map");

        buffer = bufNew(256);
        write = ioBufferWriteNewOpen(buffer);
        TEST_RESULT_VOID(blockMapWrite(blockMap, write, 3, 8), "save");
        ioWriteClose(write);

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, buffer),
            "00"                                        // Version 0

            "00"                                        // reference 0
            "22"                                        // size 4
            "04"                                        // super block size 6
            "eeee01000000ffff"                          // checksum
            "eeee02000000ffff"                          // checksum

            "15"                                        // size 5
            "00"                                        // block total 1
            "eeee03000000ffff"                          // checksum

            "08"                                        // reference 1
            "e10b"                                      // size 99
            "eeee04000000ffff"                          // checksum

            "06"                                        // reference 0
            "01"                                        // block total 1
            "02"                                        // block 3
            "eeee05000000ffff"                          // checksum

            "10"                                        // reference 2
            "3b01"                                      // size 1
            "02"                                        // super block size 2
            "eeee06000000ffff"                          // checksum

            "02"                                        // reference 0
            "01"                                        // block total 1
            "01"                                        // block 5
            "eeee07000000ffff"                          // checksum

            "13"                                        // size 6
            "0102"                                      // size 2
            "eeee08000000ffff"                          // checksum

            "09"                                        // reference 1
            "4d"                                        // size 1
            "01"                                        // block total 1
            "01"                                        // block 1
            "eeee09000000ffff",                         // checksum
            "compare");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read unequal block map");

        bufferCompare = bufNew(256);
        write = ioBufferWriteNewOpen(bufferCompare);
        TEST_RESULT_VOID(blockMapWrite(blockMapNewRead(ioBufferReadNewOpen(buffer), 3, 8), write, 3, 8), "read and save");
        ioWriteClose(write);

        TEST_RESULT_STR(strNewEncode(encodingHex, bufferCompare), strNewEncode(encodingHex, buffer), "compare");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("unequal block delta");

        TEST_RESULT_STR_Z(
            hrnBlockDeltaRender(blockMapNewRead(ioBufferReadNewOpen(buffer), 3, 8), 3, 8),
            "read {reference: 2, bundleId: 0, offset: 0, size: 1}\n"
            "  super block {max: 2, size: 1}\n"
            "    block {no: 0, offset: 15}\n"
            "read {reference: 1, bundleId: 0, offset: 0, size: 100}\n"
            "  super block {max: 3, size: 99}\n"
            "    block {no: 0, offset: 9}\n"
            "  super block {max: 3, size: 1}\n"
            "    block {no: 1, offset: 24}\n"
            "read {reference: 0, bundleId: 0, offset: 0, size: 15}\n"
            "  super block {max: 6, size: 4}\n"
            "    block {no: 0, offset: 0}\n"
            "    block {no: 1, offset: 3}\n"
            "  super block {max: 6, size: 5}\n"
            "    block {no: 0, offset: 6}\n"
            "    block {no: 3, offset: 12}\n"
            "    block {no: 5, offset: 18}\n"
            "  super block {max: 2, size: 6}\n"
            "    block {no: 0, offset: 21}\n",
            "check delta");
    }

    // *****************************************************************************************************************************
    if (testBegin("BlockIncr"))
    {
        TEST_TITLE("block incremental config map");

        TEST_ERROR(
            backupBlockIncrMapSize(cfgOptRepoBlockSizeMap, 0, STRDEF("0")), OptionInvalidValueError,
            "'0' is not valid for 'repo1-block-size-map' option");
        TEST_ERROR(
            backupBlockIncrMapSize(cfgOptRepoBlockSizeMap, 0, STRDEF("Z")), OptionInvalidValueError,
            "'Z' is not valid for 'repo1-block-size-map' option");
        TEST_ERROR(
            backupBlockIncrMapSize(cfgOptRepoBlockSizeMap, 0, STRDEF("5GiB")), OptionInvalidValueError,
            "'5GiB' is not valid for 'repo1-block-size-map' option");
        TEST_ERROR(
            backupBlockIncrMapChecksumSize(cfgOptRepoBlockChecksumSizeMap, 0, VARSTRDEF("Z")), OptionInvalidValueError,
            "'Z' is not valid for 'repo1-block-checksum-size-map' option");
        TEST_ERROR(
            backupBlockIncrMapChecksumSize(cfgOptRepoBlockChecksumSizeMap, 0, VARSTRDEF("5")), OptionInvalidValueError,
            "'5' is not valid for 'repo1-block-checksum-size-map' option");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full backup with zero block");

        ioBufferSizeSet(2);

        const Buffer *source = BUFSTRZ("");
        Buffer *destination = bufNew(256);
        IoWrite *write = ioBufferWriteNew(destination);

        TEST_RESULT_VOID(
            ioFilterGroupAdd(ioWriteFilterGroup(write), blockIncrNew(3, 3, 6, 0, 0, 0, NULL, NULL, NULL)), "block incr");
        TEST_RESULT_VOID(ioWriteOpen(write), "open");
        TEST_RESULT_VOID(ioWrite(write, source), "write");
        TEST_RESULT_VOID(ioWriteClose(write), "close");

        TEST_RESULT_UINT(pckReadU64P(ioFilterGroupResultP(ioWriteFilterGroup(write), BLOCK_INCR_FILTER_TYPE)), 0, "compare");
        TEST_RESULT_STR_Z(strNewEncode(encodingHex, destination), "", "compare");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full backup with partial block");

        source = BUFSTRZ("12");
        destination = bufNew(256);
        write = ioBufferWriteNew(destination);

        TEST_RESULT_VOID(
            ioFilterGroupAdd(ioWriteFilterGroup(write), blockIncrNew(3, 3, 8, 0, 0, 0, NULL, NULL, NULL)), "block incr");
        TEST_RESULT_VOID(ioWriteOpen(write), "open");
        TEST_RESULT_VOID(ioWrite(write, source), "write");
        TEST_RESULT_VOID(ioWriteClose(write), "close");

        uint64_t mapSize;
        TEST_ASSIGN(mapSize, pckReadU64P(ioFilterGroupResultP(ioWriteFilterGroup(write), BLOCK_INCR_FILTER_TYPE)), "map size");
        TEST_RESULT_UINT(mapSize, 13, "map size");

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, BUF(bufPtr(destination), bufUsed(destination) - (size_t)mapSize)),
            "3132",                                     // block 0
            "block list");

        const Buffer *map = BUF(bufPtr(destination) + (bufUsed(destination) - (size_t)mapSize), (size_t)mapSize);

        TEST_RESULT_STR_Z(
            hrnBlockDeltaRender(blockMapNewRead(ioBufferReadNewOpen(map), 3, 8), 3, 8),
            "read {reference: 0, bundleId: 0, offset: 0, size: 2}\n"
            "  super block {max: 2, size: 2}\n"
            "    block {no: 0, offset: 0}\n",
            "check delta");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full backup");

        source = BUFSTRZ("ABCXYZ123");
        destination = bufNew(256);
        write = ioBufferWriteNew(destination);

        TEST_RESULT_VOID(
            ioFilterGroupAdd(ioWriteFilterGroup(write), ioBufferNew()), "buffer to force internal buffer size");
        TEST_RESULT_VOID(
            ioFilterGroupAdd(
                ioWriteFilterGroup(write),
                blockIncrNewPack(ioFilterParamList(blockIncrNew(2, 3, 8, 2, 4, 5, NULL, NULL, NULL)))),
            "block incr");
        TEST_RESULT_VOID(ioWriteOpen(write), "open");
        TEST_RESULT_VOID(ioWrite(write, source), "write");
        TEST_RESULT_VOID(ioWriteClose(write), "close");

        TEST_ASSIGN(mapSize, pckReadU64P(ioFilterGroupResultP(ioWriteFilterGroup(write), BLOCK_INCR_FILTER_TYPE)), "map size");
        TEST_RESULT_UINT(mapSize, 31, "map size");

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, BUF(bufPtr(destination), bufUsed(destination) - (size_t)mapSize)),
            "414243"                                      // block 0
            "58595a"                                      // block 1
            "313233",                                     // block 2
            "block list");

        map = BUF(bufPtr(destination) + (bufUsed(destination) - (size_t)mapSize), (size_t)mapSize);

        TEST_RESULT_STR_Z(
            hrnBlockDeltaRender(blockMapNewRead(ioBufferReadNewOpen(map), 3, 8), 3, 8),
            "read {reference: 2, bundleId: 4, offset: 5, size: 9}\n"
            "  super block {max: 3, size: 3}\n"
            "    block {no: 0, offset: 0}\n"
            "  super block {max: 3, size: 3}\n"
            "    block {no: 0, offset: 3}\n"
            "  super block {max: 3, size: 3}\n"
            "    block {no: 0, offset: 6}\n",
            "check delta");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("diff/incr backup");

        ioBufferSizeSet(3);

        source = BUFSTRZ("ACCXYZ123@");
        destination = bufNew(256);
        write = ioBufferWriteNew(destination);

        TEST_RESULT_VOID(
            ioFilterGroupAdd(ioWriteFilterGroup(write), ioBufferNew()), "buffer to force internal buffer size");
        TEST_RESULT_VOID(
            ioFilterGroupAdd(
                ioWriteFilterGroup(write), blockIncrNewPack(ioFilterParamList(blockIncrNew(3, 3, 8, 3, 0, 0, map, NULL, NULL)))),
            "block incr");
        TEST_RESULT_VOID(ioWriteOpen(write), "open");
        TEST_RESULT_VOID(ioWrite(write, source), "write");
        TEST_RESULT_VOID(ioWriteClose(write), "close");

        TEST_ASSIGN(mapSize, pckReadU64P(ioFilterGroupResultP(ioWriteFilterGroup(write), BLOCK_INCR_FILTER_TYPE)), "map size");
        TEST_RESULT_UINT(mapSize, 44, "map size");

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, BUF(bufPtr(destination), bufUsed(destination) - (size_t)mapSize)),
            "414343"                                    // block 0
            "40",                                       // block 3
            "block list");

        map = BUF(bufPtr(destination) + (bufUsed(destination) - (size_t)mapSize), (size_t)mapSize);

        TEST_RESULT_STR_Z(
            hrnBlockDeltaRender(blockMapNewRead(ioBufferReadNewOpen(map), 3, 8), 3, 8),
            "read {reference: 3, bundleId: 0, offset: 0, size: 4}\n"
            "  super block {max: 3, size: 3}\n"
            "    block {no: 0, offset: 0}\n"
            "  super block {max: 1, size: 1}\n"
            "    block {no: 0, offset: 9}\n"
            "read {reference: 2, bundleId: 4, offset: 8, size: 6}\n"
            "  super block {max: 3, size: 3}\n"
            "    block {no: 0, offset: 3}\n"
            "  super block {max: 3, size: 3}\n"
            "    block {no: 0, offset: 6}\n",
            "check delta");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("diff/incr backup with identical data");

        ioBufferSizeSet(3);

        source = BUFSTRZ("ACCXYZ123@");
        destination = bufNew(256);
        write = ioBufferWriteNew(destination);

        TEST_RESULT_VOID(
            ioFilterGroupAdd(ioWriteFilterGroup(write), ioBufferNew()), "buffer to force internal buffer size");
        TEST_RESULT_VOID(
            ioFilterGroupAdd(
                ioWriteFilterGroup(write), blockIncrNewPack(ioFilterParamList(blockIncrNew(3, 3, 8, 3, 0, 0, map, NULL, NULL)))),
            "block incr");
        TEST_RESULT_VOID(ioWriteOpen(write), "open");
        TEST_RESULT_VOID(ioWrite(write, source), "write");
        TEST_RESULT_VOID(ioWriteClose(write), "close");

        TEST_ASSIGN(mapSize, pckReadU64P(ioFilterGroupResultP(ioWriteFilterGroup(write), BLOCK_INCR_FILTER_TYPE)), "map size");
        TEST_RESULT_UINT(mapSize, 0, "map size is zero");
        TEST_RESULT_UINT(bufUsed(destination), 0, "repo size is zero");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("full backup with larger super block");

        ioBufferSizeSet(2);

        source = BUFSTRZ("ABCXYZ123");
        destination = bufNew(256);
        write = ioBufferWriteNew(destination);

        TEST_RESULT_VOID(
            ioFilterGroupAdd(ioWriteFilterGroup(write), ioBufferNew()), "buffer to force internal buffer size");
        TEST_RESULT_VOID(
            ioFilterGroupAdd(
                ioWriteFilterGroup(write), blockIncrNewPack(ioFilterParamList(blockIncrNew(6, 3, 8, 2, 4, 5, NULL, NULL, NULL)))),
            "block incr");
        TEST_RESULT_VOID(ioWriteOpen(write), "open");
        TEST_RESULT_VOID(ioWrite(write, source), "write");
        TEST_RESULT_VOID(ioWriteClose(write), "close");

        TEST_ASSIGN(mapSize, pckReadU64P(ioFilterGroupResultP(ioWriteFilterGroup(write), BLOCK_INCR_FILTER_TYPE)), "map size");
        TEST_RESULT_UINT(mapSize, 32, "map size");

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, BUF(bufPtr(destination), bufUsed(destination) - (size_t)mapSize)),
            "414243"                                    // super block 0 / block 0
            "58595a"                                    // super block 0 / block 1
            "313233",                                   // block 2
            "block list");

        map = BUF(bufPtr(destination) + (bufUsed(destination) - (size_t)mapSize), (size_t)mapSize);

        TEST_RESULT_STR_Z(
            hrnBlockDeltaRender(blockMapNewRead(ioBufferReadNewOpen(map), 3, 8), 3, 8),
            "read {reference: 2, bundleId: 4, offset: 5, size: 9}\n"
            "  super block {max: 6, size: 6}\n"
            "    block {no: 0, offset: 0}\n"
            "    block {no: 1, offset: 3}\n"
            "  super block {max: 3, size: 3}\n"
            "    block {no: 0, offset: 6}\n",
            "check delta");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("new filter from pack");

        TEST_RESULT_VOID(
            blockIncrNewPack(
                ioFilterParamList(
                    blockIncrNew(
                        3, 3, 8, 2, 4, 5, NULL, compressFilterP(compressTypeGz, 1, .raw = true),
                        cipherBlockNewP(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRDEF(TEST_CIPHER_PASS), .raw = true)))),
            "block incr pack");
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
        const String *const olderBackupLabel = backupLabelFormat(backupTypeFull, NULL, timestamp - 3);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("assign label when no history");

        HRN_STORAGE_PATH_CREATE(storageRepoWrite(), STORAGE_REPO_BACKUP "/backup.history/2019");

        TEST_RESULT_STR(backupLabelCreate(backupTypeFull, NULL, timestamp), backupLabel, "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("assign label when history is older");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(),
            zNewFmt(
                STORAGE_REPO_BACKUP "/backup.history/2019/%s.manifest.gz",
                strZ(backupLabelFormat(backupTypeFull, NULL, timestamp - 4))));

        TEST_RESULT_STR(backupLabelCreate(backupTypeFull, NULL, timestamp), backupLabel, "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("assign label when backup is older");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(olderBackupLabel)));

        TEST_RESULT_STR(backupLabelCreate(backupTypeFull, NULL, timestamp), backupLabel, "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("differential and incremental backups read history related to full backup they are created on (full only)");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(),
            zNewFmt(
                STORAGE_REPO_BACKUP "/backup.history/2019/%s.manifest.gz",
                strZ(backupLabelFormat(backupTypeFull, NULL, timestamp - 1))));

        TEST_RESULT_STR(
            backupLabelCreate(backupTypeDiff, olderBackupLabel, timestamp),
            backupLabelFormat(backupTypeDiff, olderBackupLabel, timestamp),
            "create label");

        TEST_RESULT_STR(
            backupLabelCreate(backupTypeIncr, olderBackupLabel, timestamp),
            backupLabelFormat(backupTypeIncr, olderBackupLabel, timestamp),
            "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("differential and incremental backups read history related to full backup they are created on (diff exists)");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(),
            zNewFmt(
                STORAGE_REPO_BACKUP "/backup.history/2019/%s.manifest.gz",
                strZ(backupLabelFormat(backupTypeDiff, olderBackupLabel, timestamp - 2))));

        TEST_RESULT_STR(
            backupLabelCreate(backupTypeDiff, olderBackupLabel, timestamp),
            backupLabelFormat(backupTypeDiff, olderBackupLabel, timestamp),
            "create label");

        TEST_RESULT_STR(
            backupLabelCreate(backupTypeIncr, olderBackupLabel, timestamp),
            backupLabelFormat(backupTypeIncr, olderBackupLabel, timestamp),
            "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("differential and incremental backups read history related to full backup they are created on (diff and incr)");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(),
            zNewFmt(
                STORAGE_REPO_BACKUP "/backup.history/2019/%s.manifest.gz",
                strZ(backupLabelFormat(backupTypeIncr, olderBackupLabel, timestamp - 1))));

        TEST_RESULT_STR(
            backupLabelCreate(backupTypeDiff, olderBackupLabel, timestamp),
            backupLabelFormat(backupTypeDiff, olderBackupLabel, timestamp),
            "create label");

        TEST_RESULT_STR(
            backupLabelCreate(backupTypeIncr, olderBackupLabel, timestamp),
            backupLabelFormat(backupTypeIncr, olderBackupLabel, timestamp),
            "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("differential and incremental backups read history related to full backup they are created on (skew)");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(),
            zNewFmt(
                STORAGE_REPO_BACKUP "/backup.history/2019/%s.manifest.gz",
                strZ(backupLabelFormat(backupTypeIncr, olderBackupLabel, timestamp + 3))));

        TEST_ERROR(
            backupLabelCreate(backupTypeDiff, olderBackupLabel, timestamp), ClockError,
            "new backup label '20191203-193409F_20191203-193413D' is not later "
            "than latest backup label '20191203-193409F_20191203-193415I'\n"
            "HINT: has the timezone changed?\n"
            "HINT: is there clock skew?");

        TEST_ERROR(
            backupLabelCreate(backupTypeIncr, olderBackupLabel, timestamp), ClockError,
            "new backup label '20191203-193409F_20191203-193413I' is not later "
            "than latest backup label '20191203-193409F_20191203-193415I'\n"
            "HINT: has the timezone changed?\n"
            "HINT: is there clock skew?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("advance time when backup is same");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(backupLabelFormat(backupTypeFull, NULL, timestamp))));

        TEST_RESULT_STR_Z(backupLabelCreate(backupTypeFull, NULL, timestamp), "20191203-193413F", "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when new label is in the past even with advanced time");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(backupLabelFormat(backupTypeFull, NULL, timestamp + 1))));

        TEST_ERROR(
            backupLabelCreate(backupTypeFull, NULL, timestamp), ClockError,
            "new backup label '20191203-193413F' is not later than latest backup label '20191203-193413F'\n"
            "HINT: has the timezone changed?\n"
            "HINT: is there clock skew?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when new label is in the past even with advanced time (from history)");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(),
            zNewFmt(
                STORAGE_REPO_BACKUP "/backup.history/2019/%s.manifest.gz",
                strZ(backupLabelFormat(backupTypeFull, NULL, timestamp + 3600))));

        TEST_ERROR(
            backupLabelCreate(backupTypeFull, NULL, timestamp), ClockError,
            "new backup label '20191203-193413F' is not later than latest backup label '20191203-203412F'\n"
            "HINT: has the timezone changed?\n"
            "HINT: is there clock skew?");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupInit()"))
    {
        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("warn and reset when backup from standby used in offline mode");

        // Create pg_control
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_96);

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptBackupStandby, true);
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_RESULT_VOID(
            backupInit(infoBackupNew(PG_VERSION_96, HRN_PG_SYSTEMID_96, hrnPgCatalogVersion(PG_VERSION_96), NULL)), "backup init");

        TEST_RESULT_LOG(
            "P00   WARN: option backup-standby is enabled but backup is offline - backups will be performed from the primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when pg_control does not match stanza");

        // Create pg_control
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_10);

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_ERROR(
            backupInit(infoBackupNew(PG_VERSION_11, HRN_PG_SYSTEMID_11, hrnPgCatalogVersion(PG_VERSION_11), NULL)),
            BackupMismatchError,
            "PostgreSQL version 10, system-id " HRN_PG_SYSTEMID_10_Z " do not match stanza version 11, system-id"
            " " HRN_PG_SYSTEMID_11_Z "\n"
            "HINT: is this the correct stanza?");
        TEST_ERROR(
            backupInit(infoBackupNew(PG_VERSION_10, HRN_PG_SYSTEMID_11, hrnPgCatalogVersion(PG_VERSION_10), NULL)),
            BackupMismatchError,
            "PostgreSQL version 10, system-id " HRN_PG_SYSTEMID_10_Z " do not match stanza version 10, system-id"
            " " HRN_PG_SYSTEMID_11_Z "\n"
            "HINT: is this the correct stanza?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("reset checksum-page when the cluster does not have checksums enabled");

        // Create pg_control
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_95);

        // Create stanza
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        cmdStanzaCreate();
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'test1' on repo1");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptChecksumPage, true);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        HRN_PQ_SCRIPT_SET(
            // Connect to primary
            HRN_PQ_SCRIPT_OPEN_GE_93(1, "dbname='postgres' port=5432", PG_VERSION_95, TEST_PATH "/pg1", false, NULL, NULL));

        TEST_RESULT_VOID(
            dbFree(
                backupInit(infoBackupNew(PG_VERSION_95, HRN_PG_SYSTEMID_95, hrnPgCatalogVersion(PG_VERSION_95), NULL))->dbPrimary),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "check checksum-page");

        TEST_RESULT_LOG(
            "P00   WARN: checksum-page option set to true but checksums are not enabled on the cluster, resetting to false");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ok if cluster checksums are enabled and checksum-page is any value");

        // Create pg_control with page checksums
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_95, .pageChecksumVersion = 1);

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptChecksumPage, false);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        HRN_PQ_SCRIPT_SET(
            // Connect to primary
            HRN_PQ_SCRIPT_OPEN_GE_93(1, "dbname='postgres' port=5432", PG_VERSION_95, TEST_PATH "/pg1", false, NULL, NULL));

        TEST_RESULT_VOID(
            dbFree(
                backupInit(infoBackupNew(PG_VERSION_95, HRN_PG_SYSTEMID_95, hrnPgCatalogVersion(PG_VERSION_95), NULL))->dbPrimary),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "check checksum-page");

        // Create pg_control without page checksums
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_95);

        HRN_PQ_SCRIPT_SET(
            // Connect to primary
            HRN_PQ_SCRIPT_OPEN_GE_93(1, "dbname='postgres' port=5432", PG_VERSION_95, TEST_PATH "/pg1", false, NULL, NULL));

        TEST_RESULT_VOID(
            dbFree(
                backupInit(infoBackupNew(PG_VERSION_95, HRN_PG_SYSTEMID_95, hrnPgCatalogVersion(PG_VERSION_95), NULL))->dbPrimary),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "check checksum-page");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupTime()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sleep retries and stall error");

        // Create pg_control
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_95);

        // Create stanza
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        cmdStanzaCreate();
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'test1' on repo1");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        HRN_PQ_SCRIPT_SET(
            // Connect to primary
            HRN_PQ_SCRIPT_OPEN_GE_93(1, "dbname='postgres' port=5432", PG_VERSION_95, TEST_PATH "/pg1", false, NULL, NULL),

            // Advance the time slowly to force retries
            HRN_PQ_SCRIPT_TIME_QUERY(1, 1575392588998),
            HRN_PQ_SCRIPT_TIME_QUERY(1, 1575392588999),
            HRN_PQ_SCRIPT_TIME_QUERY(1, 1575392589001),

            // Stall time to force an error
            HRN_PQ_SCRIPT_TIME_QUERY(1, 1575392589998),
            HRN_PQ_SCRIPT_TIME_QUERY(1, 1575392589997),
            HRN_PQ_SCRIPT_TIME_QUERY(1, 1575392589998),
            HRN_PQ_SCRIPT_TIME_QUERY(1, 1575392589999));

        BackupData *backupData = backupInit(
            infoBackupNew(PG_VERSION_95, HRN_PG_SYSTEMID_95, hrnPgCatalogVersion(PG_VERSION_95), NULL));

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

        TEST_RESULT_LOG("P00   INFO: backup '20191003-105320F' cannot be resumed: resume is disabled");

        TEST_STORAGE_LIST_EMPTY(storageRepo(), STORAGE_REPO_BACKUP, .comment = "check backup path removed");

        cfgOptionSet(cfgOptResume, cfgSourceParam, BOOL_TRUE_VAR);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cannot resume when error on manifest load");

        Manifest *manifest = NULL;

        OBJ_NEW_BASE_BEGIN(Manifest, .childQty = MEM_CONTEXT_QTY_MAX)
        {
            manifest = manifestNewInternal();
            manifest->pub.data.backupType = backupTypeFull;
            manifest->pub.data.backrestVersion = STRDEF("BOGUS");
        }
        OBJ_NEW_END();

        HRN_STORAGE_PUT_Z(storageRepoWrite(), STORAGE_REPO_BACKUP "/20191003-105320F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, "X");

        TEST_RESULT_PTR(backupResumeFind(manifest, NULL), NULL, "find resumable backup");

        TEST_RESULT_LOG(
            "P00   WARN: backup '20191003-105320F' cannot be resumed: unable to read"
            " <REPO:BACKUP>/20191003-105320F/backup.manifest.copy");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cannot resume when pgBackRest version has changed");

        Manifest *manifestResume = NULL;

        OBJ_NEW_BASE_BEGIN(Manifest, .childQty = MEM_CONTEXT_QTY_MAX)
        {
            manifestResume = manifestNewInternal();
            manifestResume->pub.info = infoNew(NULL);
            manifestResume->pub.data.backupType = backupTypeFull;
            manifestResume->pub.data.backupLabel = strNewZ("20191003-105320F");
            manifestResume->pub.data.pgVersion = PG_VERSION_12;
        }
        OBJ_NEW_END();

        HRN_MANIFEST_TARGET_ADD(manifestResume, .name = MANIFEST_TARGET_PGDATA, .path = "/pg");
        HRN_MANIFEST_PATH_ADD(manifestResume, .name = MANIFEST_TARGET_PGDATA);
        HRN_MANIFEST_FILE_ADD(manifestResume, .name = "pg_data/" PG_FILE_PGVERSION);

        manifestSave(
            manifestResume,
            storageWriteIo(
                storageNewWriteP(
                    storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/20191003-105320F/" BACKUP_MANIFEST_FILE INFO_COPY_EXT))));

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

        ProtocolParallelJob *job = protocolParallelJobNew(VARSTRDEF("key"), strIdFromZ("x"), NULL);
        protocolParallelJobErrorSet(job, errorTypeCode(&AssertError), STRDEF("error message"));

        unsigned int currentPercentComplete = 0;

        TEST_ERROR(
            backupJobResult(
                (Manifest *)1, NULL, storageTest, strLstNew(), job, false, pgPageSize8, 0, NULL, &currentPercentComplete),
            AssertError, "error message");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("report host/100% progress on noop result");

        // Create job that skips file
        job = protocolParallelJobNew(VARSTRDEF("pg_data/test"), strIdFromZ("x"), NULL);

        PackWrite *const resultPack = protocolPackNew();
        pckWriteStrP(resultPack, STRDEF("pg_data/test"));
        pckWriteU32P(resultPack, backupCopyResultNoOp);
        // No more fields need to be written since noop will ignore them anyway
        pckWriteEndP(resultPack);

        protocolParallelJobResultSet(job, pckReadNew(pckWriteResult(resultPack)));

        // Create manifest with file
        Manifest *manifest = NULL;

        OBJ_NEW_BASE_BEGIN(Manifest, .childQty = MEM_CONTEXT_QTY_MAX)
        {
            manifest = manifestNewInternal();
            HRN_MANIFEST_FILE_ADD(manifest, .name = "pg_data/test");
        }
        OBJ_NEW_END();

        uint64_t sizeProgress = 0;
        currentPercentComplete = 4567;

        lockInit(TEST_PATH_STR, cfgOptionStr(cfgOptExecId));
        TEST_RESULT_VOID(cmdLockAcquireP(), "acquire backup lock");

        TEST_RESULT_VOID(
            backupJobResult(
                manifest, STRDEF("host"), storageTest, strLstNew(), job, false, pgPageSize8, 0, &sizeProgress,
                &currentPercentComplete),
            "log noop result");
        TEST_RESULT_VOID(cmdLockReleaseP(), "release backup lock");

        TEST_RESULT_LOG("P00 DETAIL: match file from prior backup host:" TEST_PATH "/test (0B, 100.00%)");
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
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_95);

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

        HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_POSTMTRPID, "PID");

        TEST_ERROR(
            hrnCmdBackup(), PgRunningError,
            "--no-online passed but " PG_FILE_POSTMTRPID " exists - looks like " PG_NAME " is running. Shut down " PG_NAME " and"
            " try again, or use --force.");

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

        TEST_RESULT_VOID(hrnCmdBackup(), "backup");

        TEST_RESULT_LOG_FMT(
            "P00   WARN: no prior backup exists, incr backup has been changed to full\n"
            "P00   WARN: --no-online passed and " PG_FILE_POSTMTRPID " exists but --force was passed so backup will continue though"
            " it looks like " PG_NAME " is running and the backup will probably not be consistent\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, 99.86%%) checksum %s\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (11B, 100.00%%) checksum"
            " e3db315c260e79211b7b52587123b7aa060f30ab\n"
            "P00   INFO: new backup label = [FULL-1]\n"
            "P00   INFO: full backup size = 8KB, file total = 2",
            TEST_64BIT() ?
                (TEST_BIG_ENDIAN() ? "ead3f998dc6dbc4b444f89cd449dcb81801a21ed" : "6f7fb3cd71dbef602850d05332cdd1c8e4a64121") :
                "a90290adaf2e8a36d53b0c15d277e23e1b321fdb");

        // Make pg no longer appear to be running
        HRN_STORAGE_REMOVE(storagePgWrite(), PG_FILE_POSTMTRPID, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when no files have changed");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg1");
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        hrnCfgArgRawBool(argList, cfgOptCompress, true);
        hrnCfgArgRawStrId(argList, cfgOptType, backupTypeDiff);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_ERROR(hrnCmdBackup(), FileMissingError, "no files have changed since the last backup - this seems unlikely");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P00   WARN: diff backup cannot alter compress-type option to 'gz', reset to value in [FULL-1]");

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

        TEST_RESULT_VOID(hrnCmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P00   WARN: incr backup cannot alter 'checksum-page' option to 'true', reset to 'false' from [FULL-1]\n"
            "P00   INFO: backup '[DIFF-1]' cannot be resumed: resume only valid for full backup\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, 100.00%) checksum c8663c2525f44b6d9c687fbceb4aafc63ed8b451\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-1]\n"
            "P00 DETAIL: reference pg_data/postgresql.conf to [FULL-1]\n"
            "P00   INFO: new backup label = [INCR-1]\n"
            "P00   INFO: incr backup size = 3B, file total = 3");

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

        hrnSleepRemainder();
        HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, "VR2");

        TEST_RESULT_VOID(hrnCmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, 100.00%) checksum 6f1894088c578e4f0b9888e8e8a997d93cbbc0c5\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-1]\n"
            "P00 DETAIL: reference pg_data/postgresql.conf to [FULL-1]\n"
            "P00   INFO: new backup label = [DIFF-2]\n"
            "P00   INFO: diff backup size = 3B, file total = 3");

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

        TEST_RESULT_VOID(hrnCmdBackup(), "backup");

        TEST_RESULT_LOG("P00   WARN: no prior backup exists, diff backup has been changed to full");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo");

        // Set log level to detail
        harnessLogLevelSet(logLevelDetail);

        // Add repo1 to the configuration
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_RESULT_VOID(hrnCmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   INFO: repo option not specified, defaulting to repo1\n"
            "P00   INFO: last backup label = [FULL-1], version = " PROJECT_VERSION "\n"
            "P00   WARN: diff backup cannot alter compress-type option to 'gz', reset to value in [FULL-1]\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, 100.00%) checksum 6f1894088c578e4f0b9888e8e8a997d93cbbc0c5\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-1]\n"
            "P00 DETAIL: reference pg_data/postgresql.conf to [FULL-1]\n"
            "P00   INFO: new backup label = [DIFF-3]\n"
            "P00   INFO: diff backup size = 3B, file total = 3");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo - specify repo");

        hrnCfgArgRawZ(argList, cfgOptRepo, "2");

        HRN_CFG_LOAD(cfgCmdBackup, argList);

        HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, "VER");

        // Modify file postgresql.conf during the backup to demonstrate that percent complete will be updated correctly
        HRN_STORAGE_PUT_Z(storagePgWrite(), "postgresql.conf", "CONFIGSTUFF2");
        HRN_BACKUP_SCRIPT_SET(
            {.op = hrnBackupScriptOpUpdate, .file = storagePathP(storagePg(), STRDEF("postgresql.conf")),
             .content = BUFSTRDEF("CONFIG")});

        unsigned int backupCount = strLstSize(storageListP(storageRepoIdx(1), strNewFmt(STORAGE_PATH_BACKUP "/test1")));

        TEST_RESULT_VOID(hrnCmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-2], version = " PROJECT_VERSION "\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (12B->6B, 80.00%) checksum"
            " 2fb60054b43a25d7a958d3d19bdb1aa7809577a8\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, 100.00%) checksum c8663c2525f44b6d9c687fbceb4aafc63ed8b451\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-2]\n"
            "P00   INFO: new backup label = [DIFF-4]\n"
            "P00   INFO: diff backup size = 9B, file total = 3");
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
        hrnLogReplaceAdd(", [0-9]{1,3}.[0-9]{1,2}%\\)", "[0-9].+%", "PCT", false);
        hrnLogReplaceAdd(" backup size = [0-9.]+[A-Z]+", "[^ ]+$", "SIZE", false);

        // Replace checksums since they can differ between architectures (e.g. 32/64 bit)
        hrnLogReplaceAdd("\\) checksum [a-f0-9]{40}", "[a-f0-9]{40}$", "SHA1", false);

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
            HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_95);

            cmdStanzaCreate();
            TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'test1' on repo1");

            // Load options
            argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawBool(argList, cfgOptBackupFullIncr, true);
            hrnCfgArgRawBool(argList, cfgOptStopAuto, true);
            hrnCfgArgRawBool(argList, cfgOptCompress, false);
            hrnCfgArgRawBool(argList, cfgOptArchiveCheck, false);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Add files
            HRN_STORAGE_PUT_Z(storagePgWrite(), "postgresql.conf", "CONFIGSTUFF", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, PG_VERSION_95_Z, .timeModified = backupTimeStart);
            HRN_STORAGE_PATH_CREATE(storagePgWrite(), strZ(pgWalPath(PG_VERSION_95)), .noParentCreate = true);

            // Create a backup manifest that looks like a halted backup manifest
            Manifest *manifestResume = manifestNewBuild(
                storagePg(), PG_VERSION_95, hrnPgCatalogVersion(PG_VERSION_95), 0, true, false, false, false, NULL, NULL, NULL);

            manifestResume->pub.data.backupType = backupTypeFull;
            const String *resumeLabel = backupLabelCreate(backupTypeFull, NULL, backupTimeStart);
            manifestBackupLabelSet(manifestResume, resumeLabel);

            // Copy a file to be resumed that has not changed in the repo
            HRN_STORAGE_COPY(
                storagePg(), PG_FILE_PGVERSION, storageRepoWrite(),
                zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/PG_VERSION", strZ(resumeLabel)));

            ManifestFilePack **const filePack = manifestFilePackFindInternal(manifestResume, STRDEF("pg_data/PG_VERSION"));
            ManifestFile file = manifestFileUnpack(manifestResume, *filePack);

            file.checksumSha1 = bufPtr(bufNewDecode(encodingHex, STRDEF("06d06bb31b570b94d7b4325f511f853dbe771c21")));
            file.checksumRepoSha1 = bufPtr(bufNewDecode(encodingHex, STRDEF("06d06bb31b570b94d7b4325f511f853dbe771c21")));

            manifestFilePackUpdate(manifestResume, filePack, &file);

            // Save the resume manifest
            manifestSave(
                manifestResume,
                storageWriteIo(
                    storageNewWriteP(
                        storageRepoWrite(),
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(resumeLabel)))));

            // Run backup
            hrnBackupPqScriptP(PG_VERSION_95, backupTimeStart, .noArchiveCheck = true, .noWal = true, .fullIncrNoOp = true);

            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: execute exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105D944C000000000, lsn = 5d944c0/0\n"
                "P00   WARN: resumable backup 20191002-070640F of same type exists -- invalid files will be removed then the backup"
                " will resume\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: checksum resumed file " TEST_PATH "/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105D944C000000000, lsn = 5d944c0/800000\n"
                "P00   INFO: new backup label = 20191002-070640F\n"
                "P00   INFO: full backup size = [SIZE], file total = 3");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191002-070640F}\n"
                "pg_data/PG_VERSION {s=3}\n"
                "pg_data/global/pg_control {s=8192}\n"
                "pg_data/pg_xlog/\n"
                "pg_data/postgresql.conf {s=11}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
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
            hrnCfgArgRawBool(argList, cfgOptArchiveCopy, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Create a backup manifest that looks like a halted backup manifest
            Manifest *manifestResume = manifestNewBuild(
                storagePg(), PG_VERSION_95, hrnPgCatalogVersion(PG_VERSION_95), 0, true, false, false, false, NULL, NULL, NULL);

            manifestResume->pub.data.backupType = backupTypeFull;
            manifestResume->pub.data.backupOptionCompressType = compressTypeGz;
            const String *resumeLabel = backupLabelCreate(backupTypeFull, NULL, backupTimeStart);
            manifestBackupLabelSet(manifestResume, resumeLabel);

            // File exists in cluster and repo but not in the resume manifest
            HRN_STORAGE_PUT_Z(storagePgWrite(), "not-in-resume", "TEST", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/not-in-resume.gz", strZ(resumeLabel)));

            // Remove checksum from file so it won't be resumed
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/global/pg_control.gz", strZ(resumeLabel)));

            ManifestFilePack **const filePack = manifestFilePackFindInternal(manifestResume, STRDEF("pg_data/global/pg_control"));
            ManifestFile file = manifestFileUnpack(manifestResume, *filePack);

            file.checksumSha1 = NULL;

            manifestFilePackUpdate(manifestResume, filePack, &file);

            // Size does not match between cluster and resume manifest
            HRN_STORAGE_PUT_Z(storagePgWrite(), "size-mismatch", "TEST", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/size-mismatch.gz", strZ(resumeLabel)));
            HRN_MANIFEST_FILE_ADD(
                manifestResume, .name = "pg_data/size-mismatch", .size = 33,
                .checksumSha1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                .checksumRepoSha1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

            // Time does not match between cluster and resume manifest
            HRN_STORAGE_PUT_Z(storagePgWrite(), "time-mismatch", "TEST", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/time-mismatch.gz", strZ(resumeLabel)));
            HRN_MANIFEST_FILE_ADD(
                manifestResume, .name = "pg_data/time-mismatch", .size = 4, .timestamp = backupTimeStart - 1,
                .checksumSha1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                .checksumRepoSha1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

            // Size is zero in cluster and resume manifest. ??? We'd like to remove this requirement after the migration.
            HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "zero-size", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_Z(
                storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/zero-size.gz", strZ(resumeLabel)), "ZERO-SIZE");
            HRN_MANIFEST_FILE_ADD(manifestResume, .name = "pg_data/zero-size", .size = 0, .timestamp = backupTimeStart);

            // Path is not in manifest
            HRN_STORAGE_PATH_CREATE(storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/bogus_path", strZ(resumeLabel)));

            // File is not in manifest
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/global/bogus.gz", strZ(resumeLabel)));

            // File has incorrect compression type
            HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/global/bogus", strZ(resumeLabel)));

            // Save the resume manifest
            manifestSave(
                manifestResume,
                storageWriteIo(
                    storageNewWriteP(
                        storageRepoWrite(),
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(resumeLabel)))));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
            // Disable storageFeaturePath so paths will not be created before files are copied
            ((Storage *)storageRepoWrite())->pub.interface.feature ^= 1 << storageFeaturePath;

            // Disable storageFeaturePathSync so paths will not be synced
            ((Storage *)storageRepoWrite())->pub.interface.feature ^= 1 << storageFeaturePathSync;

            // Run backup
            hrnBackupPqScriptP(PG_VERSION_95, backupTimeStart);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            // Enable storage features
            ((Storage *)storageRepoWrite())->pub.interface.feature |= 1 << storageFeaturePath;
            ((Storage *)storageRepoWrite())->pub.interface.feature |= 1 << storageFeaturePathSync;
#pragma GCC diagnostic pop

            TEST_RESULT_LOG(
                "P00   INFO: execute exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105D95D3000000000, lsn = 5d95d30/0\n"
                "P00   INFO: check archive for prior segment 0000000105D95D2F000000FF\n"
                "P00   WARN: resumable backup 20191003-105320F of same type exists -- invalid files will be removed then the backup"
                " will resume\n"
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
                "P00   INFO: execute exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105D95D3000000000, lsn = 5d95d30/800000\n"
                "P00   INFO: check archive for segment(s) 0000000105D95D3000000000:0000000105D95D3000000000\n"
                "P00 DETAIL: copy segment 0000000105D95D3000000000 to backup\n"
                "P00   INFO: new backup label = 20191003-105320F\n"
                "P00   INFO: full backup size = [SIZE], file total = 8");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191003-105320F}\n"
                "pg_data/PG_VERSION.gz {s=3, ts=-100000}\n"
                "pg_data/global/pg_control.gz {s=8192}\n"
                "pg_data/not-in-resume.gz {s=4}\n"
                "pg_data/pg_xlog/0000000105D95D3000000000.gz {s=16777216, ts=+2}\n"
                "pg_data/postgresql.conf.gz {s=11, ts=-100000}\n"
                "pg_data/size-mismatch.gz {s=4}\n"
                "pg_data/time-mismatch.gz {s=4}\n"
                "pg_data/zero-size.gz {s=0}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");

            // Remove test files
            HRN_STORAGE_REMOVE(storagePgWrite(), "not-in-resume", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "size-mismatch", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "time-mismatch", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "zero-size", .errorOnMissing = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online resumed compressed 9.5 full delta backup");

        backupTimeStart = BACKUP_EPOCH + 200000;

        {
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawBool(argList, cfgOptDelta, true);
            hrnCfgArgRawBool(argList, cfgOptStopAuto, true);
            hrnCfgArgRawBool(argList, cfgOptRepoHardlink, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Create a backup manifest that looks like a halted backup manifest
            Manifest *manifestResume = manifestNewBuild(
                storagePg(), PG_VERSION_95, hrnPgCatalogVersion(PG_VERSION_95), 0, true, false, false, false, NULL, NULL, NULL);

            manifestResume->pub.data.backupOptionCompressType = compressTypeGz;
            const String *resumeLabel = backupLabelCreate(backupTypeFull, NULL, backupTimeStart - 100000);
            manifestBackupLabelSet(manifestResume, resumeLabel);
            strLstAddZ(manifestResume->pub.referenceList, "BOGUS");

            // Time does not match between cluster and resume manifest (but resume because time is in future so delta enabled). Note
            // also that the repo file is intentionally corrupt to generate a warning about corruption in the repository.
            HRN_STORAGE_PUT_Z(storagePgWrite(), "time-mismatch2", "TEST", .timeModified = backupTimeStart + 100);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/time-mismatch2.gz", strZ(resumeLabel)));
            HRN_MANIFEST_FILE_ADD(
                manifestResume, .name = "pg_data/time-mismatch2", .size = 4, .timestamp = backupTimeStart,
                .checksumSha1 = "984816fd329622876e14907634264e6f332e9fb3");

            // File does not match what is in manifest
            HRN_STORAGE_PUT_Z(storagePgWrite(), "content-mismatch", "TEST", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/content-mismatch.gz", strZ(resumeLabel)));
            HRN_MANIFEST_FILE_ADD(
                manifestResume, .name = "pg_data/content-mismatch", .size = 4, .timestamp = backupTimeStart,
                .checksumSha1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                .checksumRepoSha1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

            // Repo size mismatch
            HRN_STORAGE_PUT_Z(storagePgWrite(), "repo-size-mismatch", "TEST", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/repo-size-mismatch.gz", strZ(resumeLabel)));
            HRN_MANIFEST_FILE_ADD(
                manifestResume, .name = "pg_data/repo-size-mismatch", .size = 4, .sizeRepo = 4, .timestamp = backupTimeStart,
                .checksumSha1 = "984816fd329622876e14907634264e6f332e9fb3",
                .checksumRepoSha1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

            // File that will resumed but removed from pg during the backup (and then the repo)
            HRN_STORAGE_PUT_Z(storagePgWrite(), "removed-during", "TEST", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_EMPTY(
                storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/removed-during.gz", strZ(resumeLabel)));
            HRN_MANIFEST_FILE_ADD(
                manifestResume, .name = "pg_data/removed-during", .size = 4, .timestamp = backupTimeStart,
                .checksumSha1 = "984816fd329622876e14907634264e6f332e9fb3");

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

            // Save the resume manifest with full type
            manifestResume->pub.data.backupType = backupTypeFull;

            manifestSave(
                manifestResume,
                storageWriteIo(
                    storageNewWriteP(
                        storageRepoWrite(),
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(resumeLabel)))));

            // Run backup
            HRN_BACKUP_SCRIPT_SET(
                {.op = hrnBackupScriptOpRemove, .file = storagePathP(storagePg(), STRDEF("removed-during"))});
            hrnBackupPqScriptP(PG_VERSION_95, backupTimeStart);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            // Check log
            TEST_RESULT_LOG(
                "P00   INFO: execute exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105D9759000000000, lsn = 5d97590/0\n"
                "P00   INFO: check archive for prior segment 0000000105D9758F000000FF\n"
                "P00   WARN: resumable backup 20191003-105321F of same type exists -- invalid files will be removed then the backup"
                " will resume\n"
                "P00   WARN: remove special file '" TEST_PATH "/repo/backup/test1/20191003-105321F/pg_data/pipe' from resumed"
                " backup\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/time-mismatch2 (4B, [PCT]) checksum [SHA1]\n"
                "P00   WARN: resumed backup file pg_data/time-mismatch2 did not have expected checksum"
                " 984816fd329622876e14907634264e6f332e9fb3. The file was recopied and backup will continue but this may be an issue"
                " unless the resumed backup path in the repository is known to be corrupted.\n"
                "            NOTE: this does not indicate a problem with the PostgreSQL page checksums.\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/repo-size-mismatch (4B, [PCT]) checksum [SHA1]\n"
                "P00   WARN: resumed backup file pg_data/repo-size-mismatch did not have expected checksum"
                " 984816fd329622876e14907634264e6f332e9fb3. The file was recopied and backup will continue but this may be an issue"
                " unless the resumed backup path in the repository is known to be corrupted.\n"
                "            NOTE: this does not indicate a problem with the PostgreSQL page checksums.\n"
                "P01 DETAIL: skip file removed by database " TEST_PATH "/pg1/removed-during\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/content-mismatch (4B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105D9759000000000, lsn = 5d97590/800000\n"
                "P00   INFO: check archive for segment(s) 0000000105D9759000000000:0000000105D9759000000000\n"
                "P00   INFO: new backup label = 20191003-105321F\n"
                "P00   INFO: full backup size = [SIZE], file total = 6");

            // Check repo directory
            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191003-105321F}\n"
                "pg_data/PG_VERSION.gz {s=3, ts=-200000}\n"
                "pg_data/content-mismatch.gz {s=4}\n"
                "pg_data/global/pg_control.gz {s=8192}\n"
                "pg_data/pg_xlog/\n"
                "pg_data/postgresql.conf.gz {s=11, ts=-200000}\n"
                "pg_data/repo-size-mismatch.gz {s=4}\n"
                "pg_data/time-mismatch2.gz {s=4, ts=+100}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");

            // Remove test files
            HRN_STORAGE_REMOVE(storagePgWrite(), "time-mismatch2", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "content-mismatch", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "repo-size-mismatch", .errorOnMissing = true);

            // Remove main manifest to make this backup look resumable
            HRN_STORAGE_REMOVE(storageRepoWrite(), "backup/test1/20191003-105321F/backup.manifest");

            // Save the resume manifest with diff type
            manifestResume->pub.data.backupType = backupTypeDiff;

            manifestSave(
                manifestResume,
                storageWriteIo(
                    storageNewWriteP(
                        storageRepoWrite(),
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(resumeLabel)))));

            // Backup errors on backup type
            hrnBackupPqScriptP(PG_VERSION_95, backupTimeStart);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            // Check log
            TEST_RESULT_LOG(
                "P00   WARN: backup '20191003-105321F' missing manifest removed from backup.info\n"
                "P00   INFO: execute exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105D9759000000000, lsn = 5d97590/0\n"
                "P00   INFO: check archive for prior segment 0000000105D9758F000000FF\n"
                "P00   WARN: backup '20191003-105321F' cannot be resumed: new backup type 'full' does not match resumable backup"
                " type 'diff'\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105D9759000000000, lsn = 5d97590/800000\n"
                "P00   INFO: check archive for segment(s) 0000000105D9759000000000:0000000105D9759000000000\n"
                "P00   INFO: new backup label = 20191004-144000F\n"
                "P00   INFO: full backup size = [SIZE], file total = 3");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 9.6 backup-standby full backup");

        backupTimeStart = BACKUP_EPOCH + 1200000;

        {
            // Update pg_control
            HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_96);

            // Update version
            HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, PG_VERSION_96_Z, .timeModified = backupTimeStart);

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
            hrnCfgArgRawBool(argList, cfgOptBackupFullIncr, true);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawBool(argList, cfgOptCompress, false);
            hrnCfgArgRawBool(argList, cfgOptBackupStandby, true);
            hrnCfgArgRawBool(argList, cfgOptStartFast, true);
            hrnCfgArgRawBool(argList, cfgOptArchiveCopy, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Add pg_control to standby
            HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_96);

            // Create file to copy from the standby. This file will be zero-length on the primary and non-zero-length on the standby
            // but no bytes will be copied.
            HRN_STORAGE_PUT_EMPTY(storagePgIdxWrite(0), PG_PATH_BASE "/1/1", .timeModified = backupTimeStart - 7200);
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(1), PG_PATH_BASE "/1/1", "1234");

            // Create file to copy from the standby. This file will be smaller on the primary than the standby and have no common
            // data in the bytes that exist on primary and standby. If the file is copied from the primary instead of the standby
            // the checksum will change but not the size.
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(0), PG_PATH_BASE "/1/2", "DA", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(1), PG_PATH_BASE "/1/2", "5678");

            // Create file to copy from the standby. This file will be larger on the primary than the standby and have no common
            // data in the bytes that exist on primary and standby. If the file is copied from the primary instead of the standby
            // the checksum and size will change.
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(0), PG_PATH_BASE "/1/3", "TEST", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(1), PG_PATH_BASE "/1/3", "ABC");

            // Create a file on the primary that does not exist on the standby to test that the file is removed from the manifest
            HRN_STORAGE_PUT_Z(storagePgIdxWrite(0), PG_PATH_BASE "/1/0", "DATA", .timeModified = backupTimeStart);

            // Set log level to warn because the following test uses multiple processes so the log order will not be deterministic
            harnessLogLevelSet(logLevelWarn);

            // Run backup but error on first archive check
            hrnBackupPqScriptP(
                PG_VERSION_96, backupTimeStart, .noPriorWal = true, .backupStandby = true, .walCompressType = compressTypeGz,
                .startFast = true, .fullIncr = true);
            TEST_ERROR(
                hrnCmdBackup(), ArchiveTimeoutError,
                "WAL segment 0000000105DA69BF000000FF was not archived before the 100ms timeout\n"
                "HINT: check the archive_command to ensure that all options are correct (especially --stanza).\n"
                "HINT: check the PostgreSQL server log for errors.\n"
                "HINT: run the 'start' command if the stanza was previously stopped.");

            TEST_RESULT_LOG(
                "P00   WARN: no prior backup exists, incr backup has been changed to full");

            // Remove halted backup so there's no resume
            HRN_STORAGE_PATH_REMOVE(storageRepoWrite(), STORAGE_REPO_BACKUP "/20191016-042640F", .recurse = true);

            // Run backup but error on archive check
            hrnBackupPqScriptP(
                PG_VERSION_96, backupTimeStart, .noWal = true, .backupStandby = true, .walCompressType = compressTypeGz,
                .startFast = true);
            TEST_ERROR(
                hrnCmdBackup(), ArchiveTimeoutError,
                "WAL segment 0000000105DA69C000000000 was not archived before the 100ms timeout\n"
                "HINT: check the archive_command to ensure that all options are correct (especially --stanza).\n"
                "HINT: check the PostgreSQL server log for errors.\n"
                "HINT: run the 'start' command if the stanza was previously stopped.");

            // Remove halted backup so there's no resume
            HRN_STORAGE_PATH_REMOVE(storageRepoWrite(), STORAGE_REPO_BACKUP "/20191016-042640F", .recurse = true);

            // Set archive.info/copy to an older timestamp so we can be sure it was updated as part of backup
            time_t archiveInfoOldTimestamp = 967746268;
            HRN_STORAGE_TIME(storageRepo(), INFO_ARCHIVE_PATH_FILE, archiveInfoOldTimestamp);
            HRN_STORAGE_TIME(storageRepo(), INFO_ARCHIVE_PATH_FILE_COPY, archiveInfoOldTimestamp);

            // Get a copy of archive.info
            const String *archiveInfoContent = strNewBuf(storageGetP(storageNewReadP(storageRepo(), INFO_ARCHIVE_PATH_FILE_STR)));

            // Run backup
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            hrnBackupPqScriptP(
                PG_VERSION_96, backupTimeStart, .backupStandby = true, .walCompressType = compressTypeGz, .startFast = true,
                .fullIncr = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            // Check archive.info/copy timestamp was updated but contents were not
            TEST_RESULT_INT_NE(
                storageInfoP(storageRepo(), INFO_ARCHIVE_PATH_FILE_STR).timeModified, archiveInfoOldTimestamp, "time updated");
            TEST_STORAGE_GET(storageRepo(), INFO_ARCHIVE_PATH_FILE, strZ(archiveInfoContent));
            TEST_RESULT_INT_NE(
                storageInfoP(storageRepo(), INFO_ARCHIVE_PATH_FILE_COPY_STR).timeModified, archiveInfoOldTimestamp, "time updated");
            TEST_STORAGE_GET(storageRepo(), INFO_ARCHIVE_PATH_FILE_COPY, strZ(archiveInfoContent));

            // Set log level back to detail
            harnessLogLevelSet(logLevelDetail);

            TEST_RESULT_LOG(
                "P00   WARN: no prior backup exists, incr backup has been changed to full");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191016-042640F}\n"
                "pg_data/PG_VERSION {s=3}\n"
                "pg_data/backup_label {s=17, ts=+2}\n"
                "pg_data/base/1/1 {s=0, ts=-7200}\n"
                "pg_data/base/1/2 {s=2}\n"
                "pg_data/base/1/3 {s=3, so=4}\n"
                "pg_data/global/pg_control {s=8192}\n"
                "pg_data/pg_xlog/0000000105DA69C000000000 {s=16777216, ts=+2}\n"
                "pg_data/postgresql.conf {s=11, ts=-1200000}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");

            // Remove test files
            HRN_STORAGE_PATH_REMOVE(storagePgIdxWrite(1), NULL, .recurse = true);
            HRN_STORAGE_PATH_REMOVE(storagePgWrite(), "base/1", .recurse = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 9.6 backup-standby full backup with block incremental");

        backupTimeStart = BACKUP_EPOCH + 1600000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pg1Path);
            hrnCfgArgKeyRaw(argList, cfgOptPgPath, 2, pg2Path);
            hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 2, "5433");
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawBool(argList, cfgOptRepoBlock, true);
            hrnCfgArgRawBool(argList, cfgOptCompress, false);
            hrnCfgArgRawBool(argList, cfgOptBackupStandby, true);
            hrnCfgArgRawBool(argList, cfgOptStartFast, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Add file that is large enough for block incremental but larger on the primary than the standby. The standby size will
            // be increased before the next backup.
            Buffer *relation = bufNew(pgPageSize8 * 5);
            memset(bufPtr(relation), 0, bufSize(relation));
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgIdxWrite(0), PG_PATH_BASE "/1/3", relation, .timeModified = backupTimeStart);

            bufUsedSet(relation, bufSize(relation) - pgPageSize8 * 2);
            HRN_STORAGE_PUT(storagePgIdxWrite(1), PG_PATH_BASE "/1/3", relation);

            // Add file that is large enough for block incremental but larger on the primary than the standby. This tests that file
            // size is set correctly when a file is referenced to a prior backup but the original size is different.
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgIdxWrite(0), PG_PATH_BASE "/1/4", relation, .timeModified = backupTimeStart);

            bufUsedSet(relation, bufSize(relation) - pgPageSize8);
            HRN_STORAGE_PUT(storagePgIdxWrite(1), PG_PATH_BASE "/1/4", relation);

            // Set log level to warn because the following test uses multiple processes so the log order will not be deterministic
            harnessLogLevelSet(logLevelWarn);

            // Run backup
            hrnBackupPqScriptP(PG_VERSION_96, backupTimeStart, .backupStandby = true, .startFast = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            // Set log level back to detail
            harnessLogLevelSet(logLevelDetail);

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191020-193320F}\n"
                "bundle/1/pg_data/PG_VERSION {s=3, ts=-400000}\n"
                "bundle/1/pg_data/global/pg_control {s=8192}\n"
                "bundle/1/pg_data/postgresql.conf {s=11, ts=-1600000}\n"
                "bundle/2/pg_data/base/1/3 {s=24576, so=40960, m=0:{0,1,2}}\n"
                "bundle/2/pg_data/base/1/4 {s=32768, so=40960, m=0:{0,1,2,3}}\n"
                "pg_data/backup_label {s=17, ts=+2}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 9.6 backup-standby incr backup with block incremental");

        backupTimeStart = BACKUP_EPOCH + 1700000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pg1Path);
            hrnCfgArgKeyRaw(argList, cfgOptPgPath, 2, pg2Path);
            hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 2, "5433");
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeIncr);
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawBool(argList, cfgOptRepoBlock, true);
            hrnCfgArgRawBool(argList, cfgOptCompress, false);
            hrnCfgArgRawZ(argList, cfgOptBackupStandby, "prefer");
            hrnCfgArgRawBool(argList, cfgOptStartFast, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Increase size of file on standby. This demonstrates that copy is using the larger file from the primary as the basis
            // for how far to read.
            Buffer *relation = bufNew(pgPageSize8 * 4);
            memset(bufPtr(relation), 0, bufSize(relation));
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgIdxWrite(1), PG_PATH_BASE "/1/3", relation);

            // Set log level to warn because the following test uses multiple processes so the log order will not be deterministic
            harnessLogLevelSet(logLevelWarn);

            // Run backup
            hrnBackupPqScriptP(PG_VERSION_96, backupTimeStart, .backupStandby = true, .startFast = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            // Set log level back to detail
            harnessLogLevelSet(logLevelDetail);

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191020-193320F_20191021-232000I}\n"
                "bundle/1/pg_data/global/pg_control {s=8192}\n"
                "bundle/2/pg_data/base/1/3 {s=32768, so=40960, m=0:{0,1,2},1:{0}, ts=-100000}\n"
                "pg_data/backup_label {s=17, ts=+2}\n"
                "20191020-193320F/bundle/1/pg_data/PG_VERSION {s=3, ts=-500000}\n"
                "20191020-193320F/bundle/2/pg_data/base/1/4 {s=32768, so=40960, m=0:{0,1,2,3}, ts=-100000}\n"
                "20191020-193320F/bundle/1/pg_data/postgresql.conf {s=11, ts=-1700000}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 9.6 backup-standby (prefer) incr backup with block incremental");

        backupTimeStart = BACKUP_EPOCH + 1800000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgKeyRaw(argList, cfgOptPgPath, 1, pg1Path);
            hrnCfgArgKeyRaw(argList, cfgOptPgPath, 2, pg2Path);
            hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 2, "5433");
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeIncr);
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawBool(argList, cfgOptRepoBlock, true);
            hrnCfgArgRawBool(argList, cfgOptCompress, false);
            hrnCfgArgRawZ(argList, cfgOptBackupStandby, "prefer");
            hrnCfgArgRawBool(argList, cfgOptStartFast, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Run backup
            hrnBackupPqScriptP(
                PG_VERSION_96, backupTimeStart, .backupStandby = true, .backupStandbyError = true, .startFast = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   WARN: unable to check pg2: [DbConnectError] unable to connect to 'dbname='postgres' port=5433': error\n"
                "P00   WARN: unable to find a standby to perform the backup, using primary instead\n"
                "P00   INFO: last backup label = 20191020-193320F_20191021-232000I, version = " PROJECT_VERSION "\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the requested immediate checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DAFC3000000000, lsn = 5dafc30/0\n"
                "P00   INFO: check archive for prior segment 0000000105DAFC2F000000FF\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/4 (bundle 1/0, 40KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/3 (bundle 1/8234, 40KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/16476, 8KB, [PCT]) checksum [SHA1]\n"
                "P00 DETAIL: reference pg_data/PG_VERSION to 20191020-193320F\n"
                "P00 DETAIL: reference pg_data/postgresql.conf to 20191020-193320F\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DAFC3000000000, lsn = 5dafc30/800000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DAFC3000000000:0000000105DAFC3000000000\n"
                "P00   INFO: new backup label = 20191020-193320F_20191023-030640I\n"
                "P00   INFO: incr backup size = [SIZE], file total = 6");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191020-193320F_20191023-030640I}\n"
                "bundle/1/pg_data/base/1/3 {s=40960, m=0:{0,1,2},1:{0},2:{0}, ts=-200000}\n"
                "bundle/1/pg_data/base/1/4 {s=40960, m=0:{0,1,2,3},2:{0}, ts=-200000}\n"
                "bundle/1/pg_data/global/pg_control {s=8192}\n"
                "pg_data/backup_label {s=17, ts=+2}\n"
                "20191020-193320F/bundle/1/pg_data/PG_VERSION {s=3, ts=-600000}\n"
                "20191020-193320F/bundle/1/pg_data/postgresql.conf {s=11, ts=-1800000}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 full backup with tablespaces and page checksums");

        backupTimeStart = BACKUP_EPOCH + 2200000;

        {
            // Update pg_control
            HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_11, .pageChecksumVersion = 1, .walSegmentSize = 1024 * 1024);

            // Update version
            HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, PG_VERSION_11_Z, .timeModified = backupTimeStart);

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
            Buffer *relation = bufNew(pgPageSize8 * 2);
            memset(bufPtr(relation), 0, bufSize(relation));
            bufUsedSet(relation, bufSize(relation));

            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x00)) = (PageHeaderData){.pd_upper = 0};

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/1", relation, .timeModified = backupTimeStart);

            // File which will fail on alignment
            relation = bufNew(pgPageSize8 + 512);
            memset(bufPtr(relation), 0, bufSize(relation));
            bufUsedSet(relation, bufSize(relation));

            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x00)) = (PageHeaderData){.pd_upper = 0xFE};
            ((PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x00)))->pd_checksum = pgPageChecksum(
                bufPtr(relation) + (pgPageSize8 * 0x00), 0, pgPageSize8);
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x01)) = (PageHeaderData){.pd_upper = 0xFF};

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/2", relation, .timeModified = backupTimeStart);

            // File with bad page checksums
            relation = bufNew(pgPageSize8 * 5);
            memset(bufPtr(relation), 0, bufSize(relation));
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x00)) = (PageHeaderData){.pd_upper = 0xFF};
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x01)) = (PageHeaderData){.pd_upper = 0x00};
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x02)) = (PageHeaderData){.pd_upper = 0xFE};
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x03)) = (PageHeaderData){.pd_upper = 0xEF};
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x04)) = (PageHeaderData){.pd_upper = 0x00};
            (bufPtr(relation) + (pgPageSize8 * 0x04))[pgPageSize8 - 1] = 0xFF;
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/3", relation, .timeModified = backupTimeStart);

            // File with bad page checksum
            relation = bufNew(pgPageSize8 * 3);
            memset(bufPtr(relation), 0, bufSize(relation));
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x00)) = (PageHeaderData){.pd_upper = 0x00};
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x01)) = (PageHeaderData){.pd_upper = 0x08};
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x02)) = (PageHeaderData){.pd_upper = 0xFF};
            ((PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x02)))->pd_checksum = pgPageChecksum(
                bufPtr(relation) + (pgPageSize8 * 0x02), 2, pgPageSize8);
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/4", relation, .timeModified = backupTimeStart);

            // Add a tablespace
            HRN_STORAGE_PATH_CREATE(storagePgWrite(), PG_PATH_PGTBLSPC);
            THROW_ON_SYS_ERROR(
                symlink("../../pg1-tblspc/32768", strZ(storagePathP(storagePg(), STRDEF(PG_PATH_PGTBLSPC "/32768")))) == -1,
                FileOpenError, "unable to create symlink");

            HRN_STORAGE_PUT_EMPTY(
                storageTest,
                zNewFmt("pg1-tblspc/32768/%s/1/5", strZ(pgTablespaceId(PG_VERSION_11, hrnPgCatalogVersion(PG_VERSION_11)))),
                .timeModified = backupTimeStart);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
            // Disable storageFeatureSymLink so tablespace (and latest) symlinks will not be created
            ((Storage *)storageRepoWrite())->pub.interface.feature ^= 1 << storageFeatureSymLink;

            // Disable storageFeatureHardLink so hardlinks will not be created
            ((Storage *)storageRepoWrite())->pub.interface.feature ^= 1 << storageFeatureHardLink;

            // Run backup
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 3, .walSwitch = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            // Reset storage features
            ((Storage *)storageRepoWrite())->pub.interface.feature |= 1 << storageFeatureSymLink;
            ((Storage *)storageRepoWrite())->pub.interface.feature |= 1 << storageFeatureHardLink;
#pragma GCC diagnostic pop

            TEST_RESULT_LOG(
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DB5DE000000000, lsn = 5db5de0/0\n"
                "P00   INFO: check archive for segment 0000000105DB5DE000000000\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/3 (40KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: invalid page checksums found in file " TEST_PATH "/pg1/base/1/3 at pages 0, 2-4\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/4 (24KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: invalid page checksum found in file " TEST_PATH "/pg1/base/1/4 at page 1\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/1 (16KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/2 (8.5KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: page misalignment in file " TEST_PATH "/pg1/base/1/2: file size 8704 is not divisible by page size"
                " 8192\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (2B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/pg_tblspc/32768/PG_11_201809051/1/5 (0B, [PCT])\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DB5DE000000002, lsn = 5db5de0/280000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00 DETAIL: wrote 'tablespace_map' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DB5DE000000000:0000000105DB5DE000000002\n"
                "P00 DETAIL: copy segment 0000000105DB5DE000000000 to backup\n"
                "P00 DETAIL: copy segment 0000000105DB5DE000000001 to backup\n"
                "P00 DETAIL: copy segment 0000000105DB5DE000000002 to backup\n"
                "P00   INFO: new backup label = 20191027-181320F\n"
                "P00   INFO: full backup size = [SIZE], file total = 13");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/20191027-181320F")),
                "pg_data/PG_VERSION.gz {s=2}\n"
                "pg_data/backup_label.gz {s=17, ts=+2}\n"
                "pg_data/base/1/1.gz {s=16384, ckp=t}\n"
                "pg_data/base/1/2.gz {s=8704, ckp=t}\n"
                "pg_data/base/1/3.gz {s=40960, ckp=[0,[2,4]]}\n"
                "pg_data/base/1/4.gz {s=24576, ckp=[1]}\n"
                "pg_data/global/pg_control.gz {s=8192}\n"
                "pg_data/pg_tblspc/\n"
                "pg_data/pg_wal/0000000105DB5DE000000000.gz {s=1048576, ts=+2}\n"
                "pg_data/pg_wal/0000000105DB5DE000000001.gz {s=1048576, ts=+2}\n"
                "pg_data/pg_wal/0000000105DB5DE000000002.gz {s=1048576, ts=+2}\n"
                "pg_data/postgresql.conf.gz {s=11, ts=-2200000}\n"
                "pg_data/tablespace_map.gz {s=19, ts=+2}\n"
                "pg_tblspc/32768/PG_11_201809051/1/5.gz {s=0, ckp=t}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "pg_tblspc/32768={\"path\":\"../../pg1-tblspc/32768\",\"tablespace-id\":\"32768\""
                ",\"tablespace-name\":\"tblspc32768\",\"type\":\"link\"}\n",
                "compare file list");

            // Remove test files
            HRN_STORAGE_REMOVE(storagePgWrite(), "base/1/2", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "base/1/3", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "base/1/4", .errorOnMissing = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when pg_control not present");

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

            // Preserve prior timestamp on pg_control
            hrnBackupPqScriptP(PG_VERSION_11, BACKUP_EPOCH + 2300000, .errorAfterStart = true);
            HRN_PG_CONTROL_TIME(storagePg(), backupTimeStart);

            // Run backup
            TEST_ERROR(
                hrnCmdBackup(), FileMissingError,
                "pg_control must be present in all online backups\n"
                "HINT: is something wrong with the clock or filesystem timestamps?");

            // Check log
            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191027-181320F, version = " PROJECT_VERSION "\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DB764000000000, lsn = 5db7640/0\n"
                "P00   INFO: check archive for prior segment 0000000105DB763F00000FFF");

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
            hrnCfgArgRawBool(argList, cfgOptPageHeaderCheck, false);
            hrnCfgArgRawBool(argList, cfgOptRepoHardlink, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // File with bad page checksum and header errors that will be ignored
            Buffer *relation = bufNew(pgPageSize8 * 4);
            memset(bufPtr(relation), 0, bufSize(relation));
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x00)) = (PageHeaderData){.pd_upper = 0xFF};
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x01)) = (PageHeaderData){.pd_upper = 0x00};
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x02)) = (PageHeaderData){.pd_upper = 0x00};
            (bufPtr(relation) + (pgPageSize8 * 0x02))[pgPageSize8 - 1] = 0xFF;
            ((PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x02)))->pd_checksum = pgPageChecksum(
                bufPtr(relation) + (pgPageSize8 * 0x02), 2, pgPageSize8);
            *(PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x03)) = (PageHeaderData){.pd_upper = 0x00};
            (bufPtr(relation) + (pgPageSize8 * 0x03))[pgPageSize8 - 1] = 0xEE;
            ((PageHeaderData *)(bufPtr(relation) + (pgPageSize8 * 0x03)))->pd_checksum = 1;
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/3", relation, .timeModified = backupTimeStart);

            // File will be truncated during backup to show that actual file size is copied no matter what original size is. This
            // will also cause an alignment error.
            Buffer *relationAfter = bufNew(pgPageSize8 + 15);
            memset(bufPtr(relationAfter), 0, bufSize(relationAfter));
            bufUsedSet(relationAfter, bufSize(relationAfter));

            // Run backup. Make sure that the timeline selected converts to hexadecimal that can't be interpreted as decimal.
            HRN_BACKUP_SCRIPT_SET(
                {.op = hrnBackupScriptOpUpdate, .file = storagePathP(storagePg(), STRDEF(PG_PATH_BASE "/1/1")),
                 .time = backupTimeStart, .content = relationAfter});
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .timeline = 0x2C, .walTotal = 2, .walSwitch = true, .fullIncrNoOp = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191027-181320F, version = " PROJECT_VERSION "\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000002C05DB8EB000000000, lsn = 5db8eb0/0\n"
                "P00   INFO: check archive for segment 0000002C05DB8EB000000000\n"
                "P00   WARN: a timeline switch has occurred since the 20191027-181320F backup, enabling delta checksum\n"
                "            HINT: this is normal after restoring from backup or promoting a standby.\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/3 (32KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: invalid page checksums found in file " TEST_PATH "/pg1/base/1/3 at pages 0, 3\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/1 (16KB->8KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: page misalignment in file " TEST_PATH "/pg1/base/1/1: file size 8207 is not divisible by page size"
                " 8192\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/PG_VERSION (2B, [PCT]) checksum [SHA1]\n"
                "P00 DETAIL: hardlink pg_data/PG_VERSION to 20191027-181320F\n"
                "P00 DETAIL: hardlink pg_data/postgresql.conf to 20191027-181320F\n"
                "P00 DETAIL: hardlink pg_tblspc/32768/PG_11_201809051/1/5 to 20191027-181320F\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000002C05DB8EB000000001, lsn = 5db8eb0/180000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00 DETAIL: wrote 'tablespace_map' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000002C05DB8EB000000000:0000002C05DB8EB000000001\n"
                "P00   INFO: new backup label = 20191027-181320F_20191030-014640I\n"
                "P00   INFO: incr backup size = [SIZE], file total = 8");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191027-181320F_20191030-014640I}\n"
                "pg_data/PG_VERSION.gz {s=2, ts=-200000}\n"
                "pg_data/backup_label.gz {s=17, ts=+2}\n"
                "pg_data/base/1/1.gz {s=8207, so=16384, ts=-200000, ckp=t}\n"
                "pg_data/base/1/3.gz {s=32768, ckp=[0,3]}\n"
                "pg_data/global/pg_control.gz {s=8192}\n"
                "pg_data/pg_tblspc/32768> {d=../../pg_tblspc/32768}\n"
                "pg_data/pg_wal/\n"
                "pg_data/postgresql.conf.gz {s=11, ts=-2400000}\n"
                "pg_data/tablespace_map.gz {s=19, ts=+2}\n"
                "pg_tblspc/32768/PG_11_201809051/1/5.gz {s=0, ts=-200000, ckp=t}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "pg_tblspc/32768={\"path\":\"../../pg1-tblspc/32768\",\"tablespace-id\":\"32768\""
                ",\"tablespace-name\":\"tblspc32768\",\"type\":\"link\"}\n",
                "compare file list");

            // Remove test files
            HRN_STORAGE_REMOVE(storagePgWrite(), "base/1/3", .errorOnMissing = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 full backup with tablespaces, bundles, annotations and unexpected pg_control/pg_catalog version");

        backupTimeStart = BACKUP_EPOCH + 2400000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawZ(argList, cfgOptManifestSaveThreshold, "1");
            hrnCfgArgRawBool(argList, cfgOptArchiveCopy, true);
            hrnCfgArgRawZ(argList, cfgOptBufferSize, "16K");
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawBool(argList, cfgOptResume, false);
            hrnCfgArgRawZ(argList, cfgOptAnnotation, "extra key=this is an annotation");
            hrnCfgArgRawZ(argList, cfgOptAnnotation, "source=this is another annotation");
            hrnCfgArgRawZ(argList, cfgOptPgVersionForce, "11");
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Create pg_control with unexpected catalog and control version
            HRN_PG_CONTROL_OVERRIDE_VERSION_PUT(
                storagePgWrite(), PG_VERSION_11, 1501, .catalogVersion = 202211110, .pageChecksumVersion = 1,
                .walSegmentSize = 1024 * 1024);

            // Set to a smaller values than the defaults allow
            cfgOptionSet(cfgOptRepoBundleSize, cfgSourceParam, VARINT64(pgPageSize8));
            cfgOptionSet(cfgOptRepoBundleLimit, cfgSourceParam, VARINT64(pgPageSize8));

            // Zero-length file to be stored
            HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "zero", .timeModified = backupTimeStart);

            // Zeroed file which passes page checksums
            Buffer *relation = bufNew(pgPageSize8 * 3);
            memset(bufPtr(relation), 0, bufSize(relation));
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/2", relation, .timeModified = backupTimeStart);

            // Old files
            HRN_STORAGE_PUT_Z(storagePgWrite(), "postgresql.auto.conf", "CONFIGSTUFF2", .timeModified = 1500000000);
            HRN_STORAGE_PUT_Z(storagePgWrite(), "stuff.conf", "CONFIGSTUFF3", .timeModified = 1500000000);

            // File that will get skipped while bundling smaller files and end up a bundle by itself
            Buffer *bigish = bufNew(pgPageSize8 - 1);
            memset(bufPtr(bigish), 0, bufSize(bigish));
            bufUsedSet(bigish, bufSize(bigish));

            HRN_STORAGE_PUT(storagePgWrite(), "bigish.dat", bigish, .timeModified = 1500000001);

            // Run backup
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 2, .pgVersionForce = STRDEF("11"),
                .walSwitch = true, .fullIncrNoOp = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DB8EB000000000, lsn = 5db8eb0/0\n"
                "P00   INFO: check archive for segment 0000000105DB8EB000000000\n"
                "P00 DETAIL: store zero-length file " TEST_PATH "/pg1/zero\n"
                "P00 DETAIL: store zero-length file " TEST_PATH "/pg1/pg_tblspc/32768/PG_11_201809051/1/5\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/2 (24KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/1 (8KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: page misalignment in file " TEST_PATH "/pg1/base/1/1: file size 8207 is not divisible by page size"
                " 8192\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/stuff.conf (bundle 1/0, 12B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.auto.conf (bundle 1/32, 12B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (bundle 1/64, 11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (bundle 1/95, 2B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/bigish.dat (bundle 2/0, 8.0KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 3/0, 8KB, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DB8EB000000001, lsn = 5db8eb0/180000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00 DETAIL: wrote 'tablespace_map' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DB8EB000000000:0000000105DB8EB000000001\n"
                "P00 DETAIL: copy segment 0000000105DB8EB000000000 to backup\n"
                "P00 DETAIL: copy segment 0000000105DB8EB000000001 to backup\n"
                "P00   INFO: new backup label = 20191030-014640F\n"
                "P00   INFO: full backup size = [SIZE], file total = 14");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191030-014640F}\n"
                "bundle/1/pg_data/PG_VERSION {s=2, ts=-200000}\n"
                "bundle/1/pg_data/postgresql.auto.conf {s=12, ts=-72400000}\n"
                "bundle/1/pg_data/postgresql.conf {s=11, ts=-2400000}\n"
                "bundle/1/pg_data/stuff.conf {s=12, ts=-72400000}\n"
                "bundle/2/pg_data/bigish.dat {s=8191, ts=-72399999}\n"
                "bundle/3/pg_data/global/pg_control {s=8192}\n"
                "pg_data/backup_label.gz {s=17, ts=+2}\n"
                "pg_data/base/1/1.gz {s=8207, ckp=t}\n"
                "pg_data/base/1/2.gz {s=24576, ckp=t}\n"
                "pg_data/pg_wal/0000000105DB8EB000000000.gz {s=1048576, ts=+2}\n"
                "pg_data/pg_wal/0000000105DB8EB000000001.gz {s=1048576, ts=+2}\n"
                "pg_data/tablespace_map.gz {s=19, ts=+2}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "pg_tblspc/32768={\"path\":\"../../pg1-tblspc/32768\",\"tablespace-id\":\"32768\""
                ",\"tablespace-name\":\"tblspc32768\",\"type\":\"link\"}\n"
                "\n"
                "[metadata]\n"
                "annotation={\"extra key\":\"this is an annotation\",\"source\":\"this is another annotation\"}\n",
                "compare file list");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 diff backup with bundles");

        backupTimeStart = BACKUP_EPOCH + 2600000;

        {
            // Remove old pg data
            HRN_STORAGE_PATH_REMOVE(storageTest, "pg1-data", .recurse = true);
            HRN_STORAGE_PATH_REMOVE(storageTest, "pg1-tblspc", .recurse = true);
            HRN_STORAGE_REMOVE(storageTest, "pg1");

            // Update pg_control
            HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_11, .walSegmentSize = 2 * 1024 * 1024);

            // Update version
            HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, PG_VERSION_11_Z, .timeModified = backupTimeStart);

            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeDiff);
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawBool(argList, cfgOptDelta, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Zero-length file to be stored
            HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "zero", .timeModified = backupTimeStart);

            // Run backup
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 2, .walSwitch = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191030-014640F, version = " PROJECT_VERSION "\n"
                "P00   WARN: diff backup cannot alter 'checksum-page' option to 'false', reset to 'true' from 20191030-014640F\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DBBF8000000000, lsn = 5dbbf80/0\n"
                "P00   INFO: check archive for segment 0000000105DBBF8000000000\n"
                "P00 DETAIL: store zero-length file " TEST_PATH "/pg1/zero\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/0, 8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/PG_VERSION (2B, [PCT]) checksum [SHA1]\n"
                "P00 DETAIL: reference pg_data/PG_VERSION to 20191030-014640F\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DBBF8000000001, lsn = 5dbbf80/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DBBF8000000000:0000000105DBBF8000000001\n"
                "P00   INFO: new backup label = 20191030-014640F_20191101-092000D\n"
                "P00   INFO: diff backup size = [SIZE], file total = 4");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191030-014640F_20191101-092000D}\n"
                "bundle/1/pg_data/global/pg_control {s=8192}\n"
                "pg_data/backup_label.gz {s=17, ts=+2}\n"
                "20191030-014640F/bundle/1/pg_data/PG_VERSION {s=2}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 full backup with block incr");

        backupTimeStart = BACKUP_EPOCH + 2800000;

        // Block sizes for testing
        #define BLOCK_MIN_SIZE                                      8192
        #define BLOCK_MIN_FILE_SIZE                                 16384
        #define BLOCK_MID_SIZE                                      16384
        #define BLOCK_MID_FILE_SIZE                                 131072
        #define BLOCK_MAX_SIZE                                      65536
        #define BLOCK_MAX_FILE_SIZE                                 1507328

        {
            // Remove old pg data
            HRN_STORAGE_PATH_REMOVE(storageTest, "pg1", .recurse = true);

            // Update pg_control
            HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_11, .pageChecksumVersion = 0, .walSegmentSize = 2 * 1024 * 1024);

            // Update version
            HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, PG_VERSION_11_Z, .timeModified = backupTimeStart);

            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawBool(argList, cfgOptBackupFullIncr, true);
            hrnCfgArgRawZ(argList, cfgOptCompressType, "none");
            hrnCfgArgRawBool(argList, cfgOptResume, false);
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBundleLimit, "8kB");
            hrnCfgArgRawBool(argList, cfgOptRepoBlock, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MAX_FILE_SIZE) "b=" STRINGIFY(BLOCK_MAX_SIZE) "b");
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MIN_FILE_SIZE) "=" STRINGIFY(BLOCK_MIN_SIZE));
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MID_FILE_SIZE) "=" STRINGIFY(BLOCK_MID_SIZE));
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // File that uses block incr and will grow (also updated before final pass)
            Buffer *fileBlockIncrGrow = bufNew(BLOCK_MIN_SIZE * 3);
            memset(bufPtr(fileBlockIncrGrow), 55, bufSize(fileBlockIncrGrow));
            bufUsedSet(fileBlockIncrGrow, bufSize(fileBlockIncrGrow));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-grow", fileBlockIncrGrow, .timeModified = backupTimeStart - 7200);

            memset(bufPtr(fileBlockIncrGrow), 0, bufSize(fileBlockIncrGrow));

            // File that uses block incr and will not be resumed
            Buffer *file = bufNew(BLOCK_MIN_SIZE * 3);
            memset(bufPtr(file), 0, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-no-resume", file, .timeModified = backupTimeStart);

            // Error when pg_control is missing after backup start
            HRN_BACKUP_SCRIPT_SET(
                {.op = hrnBackupScriptOpRemove, .exec = 2, .file = storagePathP(storagePg(), STRDEF("global/pg_control"))});
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 2, .walSwitch = true,
                .errorAfterCopyStart = true);
            TEST_ERROR(
                hrnCmdBackup(), FileMissingError,
                "raised from local-1 shim protocol: unable to open missing file '" TEST_PATH "/pg1/global/pg_control' for read");

            TEST_RESULT_LOG(
                "P00   INFO: full/incr backup preliminary copy of files last modified before 2019-11-03 16:51:20\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-grow (24KB, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DBF06000000000, lsn = 5dbf060/0\n"
                "P00   INFO: check archive for segment 0000000105DBF06000000000\n"
                "P00   INFO: full/incr backup cleanup\n"
                "P00   INFO: full/incr backup final copy\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-no-resume (24KB, [PCT]) checksum [SHA1]");

            HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_11, .pageChecksumVersion = 0, .walSegmentSize = 2 * 1024 * 1024);

            // File removed before final copy
            file = bufNew(BLOCK_MIN_SIZE + 1);
            memset(bufPtr(file), 71, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "rm-before-final-cp", file, .timeModified = backupTimeStart - 120);

            // Bundled file removed before final copy
            file = bufNew(BLOCK_MIN_SIZE);
            memset(bufPtr(file), 22, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "rm-bnd-before-final-cp", file, .timeModified = backupTimeStart - 120);

            // File time will change before the final copy and cause a delta
            Buffer *fileTimeChange = bufNew(BLOCK_MIN_SIZE + 1);
            memset(bufPtr(fileTimeChange), 0, bufSize(fileTimeChange));
            bufUsedSet(fileTimeChange, bufSize(fileTimeChange));

            HRN_STORAGE_PUT(storagePgWrite(), "time-change", fileTimeChange, .timeModified = backupTimeStart - 120);

            // File removed after prelim copy and before final manifest build
            file = bufNew(BLOCK_MIN_SIZE + 2);
            memset(bufPtr(file), 71, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "rm-after-prelim-cp", file, .timeModified = backupTimeStart - 120);

            // File just over the full/incr time limit
            file = bufNew(BLOCK_MIN_SIZE + 3);
            memset(bufPtr(file), 33, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "below-fi-limit", file, .timeModified = backupTimeStart - 119);

            // Zero-length file that will not be copied due to bundling
            HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "empty", .timeModified = backupTimeStart - 119);

            // Remove percentage log replacement to check progress reporting for full/incr
            hrnLogReplaceRemove(", [0-9]{1,3}.[0-9]{1,2}%\\)");

            // Run backup
            HRN_BACKUP_SCRIPT_SET(
                {.op = hrnBackupScriptOpUpdate, .after = true, .file = storagePathP(storagePg(), STRDEF("block-incr-grow")),
                 .content = fileBlockIncrGrow, .time = backupTimeStart},
                {.op = hrnBackupScriptOpUpdate, .after = true, .file = storagePathP(storagePg(), STRDEF("time-change")),
                 .content = fileTimeChange, .time = backupTimeStart - 121},
                {.op = hrnBackupScriptOpRemove, .after = true, .file = storagePathP(storagePg(), STRDEF("rm-after-prelim-cp"))},
                {.op = hrnBackupScriptOpRemove, .exec = 2, .file = storagePathP(storagePg(), STRDEF("rm-bnd-before-final-cp"))},
                {.op = hrnBackupScriptOpRemove, .exec = 2, .file = storagePathP(storagePg(), STRDEF("rm-before-final-cp"))});
            hrnBackupPqScriptP(PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 2, .walSwitch = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: full/incr backup preliminary copy of files last modified before 2019-11-03 16:51:20\n"
                "P00   INFO: backup '20191103-165320F' cannot be resumed: partially deleted by prior resume or invalid\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-grow (24KB, 24.99%) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/rm-after-prelim-cp (8KB, 33.33%) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/time-change (8KB, 41.66%) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/rm-before-final-cp (8KB, 49.99%) checksum [SHA1]\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DBF06000000000, lsn = 5dbf060/0\n"
                "P00   INFO: check archive for segment 0000000105DBF06000000000\n"
                "P00   INFO: full/incr backup cleanup\n"
                "P00   WARN: file 'time-change' has timestamp earlier than prior backup (prior 1572799880, current 1572799879),"
                " enabling delta checksum\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191103-165320F/pg_data/rm-after-prelim-cp' from backup"
                " (missing in manifest)\n"
                "P00   INFO: full/incr backup final copy\n"
                "P00 DETAIL: store zero-length file " TEST_PATH "/pg1/empty\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-no-resume (24KB, 52.93%) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-grow (24KB, 70.58%) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/below-fi-limit (8KB, 76.46%) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/time-change (8KB, 82.35%) checksum [SHA1]\n"
                "P01 DETAIL: skip file removed by database " TEST_PATH "/pg1/rm-before-final-cp\n"
                "P01 DETAIL: skip file removed by database " TEST_PATH "/pg1/rm-bnd-before-final-cp\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/0, 8KB, 99.99%) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (bundle 1/8192, 2B, 100.00%) checksum [SHA1]\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DBF06000000001, lsn = 5dbf060/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DBF06000000000:0000000105DBF06000000001\n"
                "P00   INFO: new backup label = 20191103-165320F\n"
                "P00   INFO: full backup size = [SIZE], file total = 8");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191103-165320F}\n"
                "bundle/1/pg_data/PG_VERSION {s=2}\n"
                "bundle/1/pg_data/global/pg_control {s=8192}\n"
                "pg_data/backup_label {s=17, ts=+2}\n"
                "pg_data/below-fi-limit {s=8195, ts=-119}\n"
                "pg_data/block-incr-grow.pgbi {s=24576, m=0:{0,1,2}}\n"
                "pg_data/block-incr-no-resume.pgbi {s=24576, m=0:{0,1,2}}\n"
                "pg_data/time-change {s=8193, ts=-121}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");

            HRN_STORAGE_REMOVE(storagePgWrite(), "rm-before-final-cp");
            HRN_STORAGE_REMOVE(storagePgWrite(), "time-change");
            HRN_STORAGE_REMOVE(storagePgWrite(), "below-fi-limit");
            HRN_STORAGE_REMOVE(storagePgWrite(), "empty");

            // Replace progress reporting to reduce log churn
            hrnLogReplaceAdd(", [0-9]{1,3}.[0-9]{1,2}%\\)", "[0-9].+%", "PCT", false);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 full backup with block incr resume");

        backupTimeStart = BACKUP_EPOCH + 2900000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawZ(argList, cfgOptCompressType, "none");
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBundleLimit, "23kB");
            hrnCfgArgRawBool(argList, cfgOptRepoBlock, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MAX_FILE_SIZE) "b=" STRINGIFY(BLOCK_MAX_SIZE) "b");
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MIN_FILE_SIZE) "=" STRINGIFY(BLOCK_MIN_SIZE));
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MID_FILE_SIZE) "=" STRINGIFY(BLOCK_MID_SIZE));
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Make this backup look resumable
            HRN_STORAGE_REMOVE(storageTest, "repo/backup/test1/20191103-165320F/backup.manifest");

            // Corrupt file that uses block incr and will not be resumed
            Buffer *file = bufNew(BLOCK_MIN_SIZE * 3);
            memset(bufPtr(file), 99, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storageRepoWrite(), "backup/test1/20191103-165320F/pg_data/block-incr-no-resume.pgbi", file);

            // File that shrinks below the limit where it would get block incremental if it were new
            file = bufNew(BLOCK_MIN_FILE_SIZE + 1);
            memset(bufPtr(file), 55, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-shrink", file, .timeModified = backupTimeStart);

            // File that shrinks to the size of a single block
            file = bufNew(BLOCK_MIN_FILE_SIZE);
            memset(bufPtr(file), 44, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-shrink-block", file, .timeModified = backupTimeStart);

            // File that shrinks below the size of a single block
            file = bufNew(BLOCK_MIN_FILE_SIZE);
            memset(bufPtr(file), 43, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-shrink-below", file, .timeModified = backupTimeStart);

            // Block incremental file that remains the same between backups
            file = bufNew(BLOCK_MIN_FILE_SIZE);
            memset(bufPtr(file), 33, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-same", file, .timeModified = backupTimeStart);

            // Normal file that remains the same between backups
            HRN_STORAGE_PUT_Z(storagePgWrite(), "normal-same", "SAME", .timeModified = backupTimeStart);

            // File that grows above the limit
            file = bufNew(BLOCK_MIN_FILE_SIZE - 1);
            memset(bufPtr(file), 77, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "grow-to-block-incr", file, .timeModified = backupTimeStart);

            // Normal file that remains the same between backups
            HRN_STORAGE_PUT_Z(storagePgWrite(), "normal-same", "SAME", .timeModified = backupTimeStart);

            // Run backup
            hrnBackupPqScriptP(PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 2, .walSwitch = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   WARN: backup '20191103-165320F' missing manifest removed from backup.info\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DC08C000000000, lsn = 5dc08c0/0\n"
                "P00   INFO: check archive for segment 0000000105DC08C000000000\n"
                "P00   WARN: resumable backup 20191103-165320F of same type exists -- invalid files will be removed then the backup"
                " will resume\n"
                "P00 DETAIL: remove path '" TEST_PATH "/repo/backup/test1/20191103-165320F/bundle' from resumed backup\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191103-165320F/pg_data/backup_label' from resumed"
                " backup (missing in manifest)\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191103-165320F/pg_data/below-fi-limit' from resumed"
                " backup (missing in manifest)\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191103-165320F/pg_data/time-change' from resumed"
                " backup (missing in manifest)\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-no-resume (24KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: resumed backup file pg_data/block-incr-no-resume did not have expected checksum"
                " ebdd38b69cd5b9f2d00d273c981e16960fbbb4f7. The file was recopied and backup will continue but this may be an issue"
                " unless the resumed backup path in the repository is known to be corrupted.\n"
                "            NOTE: this does not indicate a problem with the PostgreSQL page checksums.\n"
                "P01 DETAIL: checksum resumed file " TEST_PATH "/pg1/block-incr-grow (24KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (bundle 1/0, 2B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/normal-same (bundle 1/2, 4B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/grow-to-block-incr (bundle 1/6, 16.0KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/16389, 8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-shrink-block (bundle 1/24581, 16KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-shrink-below (bundle 1/40987, 16KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-shrink (bundle 1/57393, 16KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-same (bundle 1/73807, 16KB, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DC08C000000001, lsn = 5dc08c0/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DC08C000000000:0000000105DC08C000000001\n"
                "P00   INFO: new backup label = 20191103-165320F\n"
                "P00   INFO: full backup size = [SIZE], file total = 11");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191103-165320F}\n"
                "bundle/1/pg_data/PG_VERSION {s=2, ts=-100000}\n"
                "bundle/1/pg_data/block-incr-same {s=16384, m=0:{0,1}}\n"
                "bundle/1/pg_data/block-incr-shrink {s=16385, m=0:{0,1,2}}\n"
                "bundle/1/pg_data/block-incr-shrink-below {s=16384, m=0:{0,1}}\n"
                "bundle/1/pg_data/block-incr-shrink-block {s=16384, m=0:{0,1}}\n"
                "bundle/1/pg_data/global/pg_control {s=8192}\n"
                "bundle/1/pg_data/grow-to-block-incr {s=16383}\n"
                "bundle/1/pg_data/normal-same {s=4}\n"
                "pg_data/backup_label {s=17, ts=+2}\n"
                "pg_data/block-incr-grow.pgbi {s=24576, m=0:{0,1,2}, ts=-100000}\n"
                "pg_data/block-incr-no-resume.pgbi {s=24576, m=0:{0,1,2}, ts=-100000}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");

            HRN_STORAGE_REMOVE(storagePgWrite(), "block-incr-no-resume");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 diff backup with block incr");

        backupTimeStart = BACKUP_EPOCH + 3000000;

        {
            // Load the previous manifest and null out the checksum-page option to be sure it gets set to false in this backup
            const String *manifestPriorFile = STRDEF(STORAGE_REPO_BACKUP "/20191103-165320F/" BACKUP_MANIFEST_FILE);
            Manifest *manifestPrior = manifestNewLoad(storageReadIo(storageNewReadP(storageRepo(), manifestPriorFile)));
            manifestPrior->pub.data.backupOptionChecksumPage = NULL;
            manifestSave(manifestPrior, storageWriteIo(storageNewWriteP(storageRepoWrite(), manifestPriorFile)));

            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeDiff);
            hrnCfgArgRawZ(argList, cfgOptCompressType, "none");
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBundleLimit, "23kB");
            hrnCfgArgRawBool(argList, cfgOptRepoBlock, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MAX_FILE_SIZE) "=" STRINGIFY(BLOCK_MAX_SIZE));
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MIN_FILE_SIZE) "=" STRINGIFY(BLOCK_MIN_SIZE));
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MID_FILE_SIZE) "=" STRINGIFY(BLOCK_MID_SIZE));
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeSuper, "1MiB");
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Grow file size to check block incr delta. This is large enough that it would get a new block size if it were a new
            // file rather than a delta. Also split the first super block.
            Buffer *file = bufNew(BLOCK_MID_FILE_SIZE);
            memset(bufPtr(file), 0, bufSize(file));
            memset(bufPtr(file) + BLOCK_MIN_SIZE, 1, BLOCK_MIN_SIZE);
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-grow", file, .timeModified = backupTimeStart);

            // File that gets a larger block size and multiple super blocks
            file = bufNew(BLOCK_MAX_FILE_SIZE);
            memset(bufPtr(file), 0, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-larger", file, .timeModified = backupTimeStart);

            // Shrink file below the limit where it would get block incremental if it were new
            file = bufNew(BLOCK_MIN_FILE_SIZE - 1);
            memset(bufPtr(file), 55, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-shrink", file, .timeModified = backupTimeStart);

            // Shrink file to the size of a single block
            file = bufNew(BLOCK_MIN_SIZE);
            memset(bufPtr(file), 44, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-shrink-block", file, .timeModified = backupTimeStart);

            // Shrinks file below the size of a single block
            file = bufNew(8);
            memset(bufPtr(file), 44, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-shrink-below", file, .timeModified = backupTimeStart);

            // Update timestamps without changing contents of the files
            HRN_STORAGE_TIME(storagePgWrite(), "block-incr-same", backupTimeStart);
            HRN_STORAGE_TIME(storagePgWrite(), "normal-same", backupTimeStart);

            // Grow file above the limit
            file = bufNew(BLOCK_MIN_FILE_SIZE + 1);
            memset(bufPtr(file), 77, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "grow-to-block-incr", file, .timeModified = backupTimeStart);

            // File that gets truncated to zero during the backup
            HRN_STORAGE_PUT(storagePgWrite(), "truncate-to-zero", BUFSTRDEF("DATA"), .timeModified = backupTimeStart);

            // Run backup
            HRN_BACKUP_SCRIPT_SET(
                {.op = hrnBackupScriptOpUpdate, .file = storagePathP(storagePg(), STRDEF("truncate-to-zero")),
                 .time = backupTimeStart + 1});
            hrnBackupPqScriptP(PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 2, .walSwitch = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191103-165320F, version = " PROJECT_VERSION "\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DC213000000000, lsn = 5dc2130/0\n"
                "P00   INFO: check archive for segment 0000000105DC213000000000\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-larger (1.4MB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-grow (128KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: store truncated file " TEST_PATH "/pg1/truncate-to-zero (4B->0B, [PCT])\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/normal-same (4B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/grow-to-block-incr (bundle 1/0, 16KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/16411, 8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-shrink-block (bundle 1/24603, 8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-shrink-below (bundle 1/24620, 8B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-shrink (bundle 1/24628, 16.0KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/block-incr-same (16KB, [PCT]) checksum [SHA1]\n"
                "P00 DETAIL: reference pg_data/PG_VERSION to 20191103-165320F\n"
                "P00 DETAIL: reference pg_data/block-incr-same to 20191103-165320F\n"
                "P00 DETAIL: reference pg_data/normal-same to 20191103-165320F\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DC213000000001, lsn = 5dc2130/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DC213000000000:0000000105DC213000000001\n"
                "P00   INFO: new backup label = 20191103-165320F_20191106-002640D\n"
                "P00   INFO: diff backup size = [SIZE], file total = 12");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ".> {d=20191103-165320F_20191106-002640D}\n"
                "bundle/1/pg_data/block-incr-shrink {s=16383, m=0:{0},1:{0}}\n"
                "bundle/1/pg_data/block-incr-shrink-below {s=8}\n"
                "bundle/1/pg_data/block-incr-shrink-block {s=8192, m=0:{0}}\n"
                "bundle/1/pg_data/global/pg_control {s=8192}\n"
                "bundle/1/pg_data/grow-to-block-incr {s=16385, m=1:{0,1,2}}\n"
                "pg_data/backup_label {s=17, ts=+2}\n"
                "pg_data/block-incr-grow.pgbi {s=131072, m=0:{0},1:{0},0:{2},1:{1,2,3,4,5,6,7,8,9,10,11,12,13}}\n"
                "pg_data/block-incr-larger.pgbi {s=1507328, m=1:{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},1:{0,1,2,3,4,5,6}}\n"
                "20191103-165320F/bundle/1/pg_data/PG_VERSION {s=2, ts=-200000}\n"
                "20191103-165320F/bundle/1/pg_data/block-incr-same {s=16384, m=0:{0,1}}\n"
                "20191103-165320F/bundle/1/pg_data/normal-same {s=4}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");

            HRN_STORAGE_REMOVE(storagePgWrite(), "block-incr-grow");
            HRN_STORAGE_REMOVE(storagePgWrite(), "block-incr-larger");
            HRN_STORAGE_REMOVE(storagePgWrite(), "block-incr-same");
            HRN_STORAGE_REMOVE(storagePgWrite(), "block-incr-shrink");
            HRN_STORAGE_REMOVE(storagePgWrite(), "block-incr-shrink-below");
            HRN_STORAGE_REMOVE(storagePgWrite(), "block-incr-shrink-block");
            HRN_STORAGE_REMOVE(storagePgWrite(), "grow-to-block-incr");
            HRN_STORAGE_REMOVE(storagePgWrite(), "normal-same");
            HRN_STORAGE_REMOVE(storagePgWrite(), "truncate-to-zero");
        }

        // It is better to put as few tests here as possible because cmp/enc makes tests more expensive (especially with valgrind)
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 full backup with comp/enc");

        backupTimeStart = BACKUP_EPOCH + 3200000;

        {
            // Create encrypted stanza
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawBool(argList, cfgOptOnline, false);
            hrnCfgArgRawZ(argList, cfgOptRepoCipherType, "aes-256-cbc");
            hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
            HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

            HRN_STORAGE_PATH_REMOVE(storageRepoIdxWrite(0), NULL, .recurse = true);

            cmdStanzaCreate();
            TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'test1' on repo1");

            // Load options
            argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBundleLimit, "8KiB");
            hrnCfgArgRawBool(argList, cfgOptRepoBlock, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MAX_FILE_SIZE) "=" STRINGIFY(BLOCK_MAX_SIZE));
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MIN_FILE_SIZE) "=" STRINGIFY(BLOCK_MIN_SIZE));
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MID_FILE_SIZE) "=" STRINGIFY(BLOCK_MID_SIZE));
            hrnCfgArgRawZ(argList, cfgOptBufferSize, "16KiB");
            hrnCfgArgRawZ(argList, cfgOptRepoCipherType, "aes-256-cbc");
            hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // File that uses block incr and will grow
            Buffer *file = bufNew((size_t)(BLOCK_MIN_FILE_SIZE * 1.5));
            memset(bufPtr(file), 0, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-grow", file, .timeModified = backupTimeStart);

            // File that uses block incr and will not be resumed
            file = bufNew((size_t)(BLOCK_MIN_FILE_SIZE * 1.5));
            memset(bufPtr(file), 0, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-no-resume", file, .timeModified = backupTimeStart);

            // Run backup
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeNone, .cipherType = cipherTypeAes256Cbc,
                .cipherPass = TEST_CIPHER_PASS, .walTotal = 2, .walSwitch = true, .fullIncrNoOp = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DC520000000000, lsn = 5dc5200/0\n"
                "P00   INFO: check archive for segment 0000000105DC520000000000\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-no-resume (24KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-grow (24KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (bundle 1/0, 2B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/24, 8KB, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DC520000000001, lsn = 5dc5200/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DC520000000000:0000000105DC520000000001\n"
                "P00   INFO: new backup label = 20191108-080000F\n"
                "P00   INFO: full backup size = [SIZE], file total = 5");

            TEST_RESULT_STR_Z(
                testBackupValidateP(
                    storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest"), .cipherType = cipherTypeAes256Cbc,
                    .cipherPass = TEST_CIPHER_PASS),
                ".> {d=20191108-080000F}\n"
                "bundle/1/pg_data/PG_VERSION {s=2, ts=-400000}\n"
                "bundle/1/pg_data/global/pg_control {s=8192}\n"
                "pg_data/backup_label.gz {s=17, ts=+2}\n"
                "pg_data/block-incr-grow.pgbi {s=24576, m=0:{0,1,2}}\n"
                "pg_data/block-incr-no-resume.pgbi {s=24576, m=0:{0,1,2}}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 full backup with comp/enc resume");

        backupTimeStart = BACKUP_EPOCH + 3300000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawBool(argList, cfgOptBackupFullIncr, true);
            hrnCfgArgRawBool(argList, cfgOptDelta, true);
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBundleLimit, "8KiB");
            hrnCfgArgRawBool(argList, cfgOptRepoBlock, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MAX_FILE_SIZE) "=" STRINGIFY(BLOCK_MAX_SIZE));
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MIN_FILE_SIZE) "=" STRINGIFY(BLOCK_MIN_SIZE));
            hrnCfgArgRawZ(argList, cfgOptRepoBlockSizeMap, STRINGIFY(BLOCK_MID_FILE_SIZE) "=" STRINGIFY(BLOCK_MID_SIZE));
            hrnCfgArgRawZ(argList, cfgOptBufferSize, "16KiB");
            hrnCfgArgRawZ(argList, cfgOptRepoCipherType, "aes-256-cbc");
            hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Make this backup look resumable
            HRN_STORAGE_REMOVE(storageTest, "repo/backup/test1/20191108-080000F/backup.manifest");

            // File that will later have a timestamp far enough in the past to make the block size zero
            Buffer *file = bufNew((size_t)(BLOCK_MIN_FILE_SIZE));
            memset(bufPtr(file), 0, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-wayback", file, .timeModified = backupTimeStart);

            // Modify file that uses block incr and will not be resumed so it is caught by delta
            file = bufNew((size_t)(BLOCK_MIN_FILE_SIZE * 1.5));
            memset(bufPtr(file), 0xFE, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-no-resume", file, .timeModified = backupTimeStart - 100000);

            // Run backup
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeNone, .cipherType = cipherTypeAes256Cbc,
                .cipherPass = TEST_CIPHER_PASS, .walTotal = 2, .walSwitch = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   WARN: backup '20191108-080000F' missing manifest removed from backup.info\n"
                "P00   INFO: full/incr backup preliminary copy of files last modified before 2019-11-08 11:44:40\n"
                "P00   WARN: resumable backup 20191108-080000F of same type exists -- invalid files will be removed then the backup"
                " will resume\n"
                "P00 DETAIL: remove path '" TEST_PATH "/repo/backup/test1/20191108-080000F/bundle' from resumed backup\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191108-080000F/pg_data/backup_label.gz' from resumed"
                " backup (missing in manifest)\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-no-resume (24KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: checksum resumed file " TEST_PATH "/pg1/block-incr-grow (24KB, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DC6A7000000000, lsn = 5dc6a70/0\n"
                "P00   INFO: check archive for segment 0000000105DC6A7000000000\n"
                "P00   INFO: full/incr backup cleanup\n"
                "P00   INFO: full/incr backup final copy\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/block-incr-no-resume (24KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/block-incr-grow (24KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-wayback (16KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (bundle 1/0, 2B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/24, 8KB, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DC6A7000000001, lsn = 5dc6a70/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DC6A7000000000:0000000105DC6A7000000001\n"
                "P00   INFO: new backup label = 20191108-080000F\n"
                "P00   INFO: full backup size = [SIZE], file total = 6");

            TEST_RESULT_STR_Z(
                testBackupValidateP(
                    storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest"), .cipherType = cipherTypeAes256Cbc,
                    .cipherPass = TEST_CIPHER_PASS),
                ".> {d=20191108-080000F}\n"
                "bundle/1/pg_data/PG_VERSION {s=2, ts=-500000}\n"
                "bundle/1/pg_data/global/pg_control {s=8192}\n"
                "pg_data/backup_label.gz {s=17, ts=+2}\n"
                "pg_data/block-incr-grow.pgbi {s=24576, m=0:{0,1,2}, ts=-100000}\n"
                "pg_data/block-incr-no-resume.pgbi {s=24576, m=0:{0,1,2}, ts=-100000}\n"
                "pg_data/block-incr-wayback.pgbi {s=16384, m=0:{0,1}}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");

            HRN_STORAGE_REMOVE(storagePgWrite(), "block-incr-no-resume");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 diff backup with comp/enc and delta");

        backupTimeStart = BACKUP_EPOCH + 3400000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeDiff);
            hrnCfgArgRawZ(argList, cfgOptCompressType, "bz2");
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBundleLimit, "4MiB");
            hrnCfgArgRawBool(argList, cfgOptRepoBlock, true);
            hrnCfgArgRawZ(argList, cfgOptRepoCipherType, "aes-256-cbc");
            hrnCfgArgRawZ(argList, cfgOptRepoBlockAgeMap, "1=2");
            hrnCfgArgRawZ(argList, cfgOptRepoBlockAgeMap, "2=0");
            hrnCfgArgRawZ(argList, cfgOptRepoBlockChecksumSizeMap, "16KiB=16");
            hrnCfgArgRawZ(argList, cfgOptRepoBlockChecksumSizeMap, "8KiB=12");
            hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // File that uses block incr and grows and overwrites last block of prior map
            Buffer *file = bufNew(BLOCK_MIN_FILE_SIZE * 3);
            memset(bufPtr(file), 0, bufSize(file));
            memset(bufPtr(file) + (BLOCK_MIN_SIZE * 2), 1, BLOCK_MIN_SIZE);
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-grow", file, .timeModified = backupTimeStart - 200000);

            // File with age multiplier
            file = bufNew(BLOCK_MIN_FILE_SIZE * 2);
            memset(bufPtr(file), 0, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-age-multiplier", file, .timeModified = backupTimeStart - SEC_PER_DAY);

            // File with age old enough to not have block incr
            file = bufNew(BLOCK_MIN_FILE_SIZE);
            memset(bufPtr(file), 0, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-age-to-zero", file, .timeModified = backupTimeStart - 2 * SEC_PER_DAY);

            // File goes back in time far enough to not need block incr if it was new
            HRN_STORAGE_TIME(storagePgWrite(), "block-incr-wayback", backupTimeStart - 2 * SEC_PER_DAY);

            // Run backup
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeNone, .cipherType = cipherTypeAes256Cbc,
                .cipherPass = TEST_CIPHER_PASS, .walTotal = 2, .walSwitch = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191108-080000F, version = " PROJECT_VERSION "\n"
                "P00   WARN: diff backup cannot alter compress-type option to 'bz2', reset to value in 20191108-080000F\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DC82D000000000, lsn = 5dc82d0/0\n"
                "P00   INFO: check archive for segment 0000000105DC82D000000000\n"
                "P00   WARN: file 'block-incr-grow' has same timestamp (1573200000) as prior but different size (prior 24576,"
                " current 49152), enabling delta checksum\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/PG_VERSION (2B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-grow (bundle 1/0, 48KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/block-incr-wayback (16KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-age-to-zero (bundle 1/128, 16KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-age-multiplier (bundle 1/184, 32KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/312, 8KB, [PCT]) checksum [SHA1]\n"
                "P00 DETAIL: reference pg_data/PG_VERSION to 20191108-080000F\n"
                "P00 DETAIL: reference pg_data/block-incr-wayback to 20191108-080000F\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DC82D000000001, lsn = 5dc82d0/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DC82D000000000:0000000105DC82D000000001\n"
                "P00   INFO: new backup label = 20191108-080000F_20191110-153320D\n"
                "P00   INFO: diff backup size = [SIZE], file total = 7");

            TEST_RESULT_STR_Z(
                testBackupValidateP(
                    storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest"), .cipherType = cipherTypeAes256Cbc,
                    .cipherPass = TEST_CIPHER_PASS),
                ".> {d=20191108-080000F_20191110-153320D}\n"
                "bundle/1/pg_data/block-age-multiplier {s=32768, m=1:{0,1}, ts=-86400}\n"
                "bundle/1/pg_data/block-age-to-zero {s=16384, ts=-172800}\n"
                "bundle/1/pg_data/block-incr-grow {s=49152, m=0:{0,1},1:{0,1,2,3}, ts=-200000}\n"
                "bundle/1/pg_data/global/pg_control {s=8192}\n"
                "pg_data/backup_label.gz {s=17, ts=+2}\n"
                "20191108-080000F/bundle/1/pg_data/PG_VERSION {s=2, ts=-600000}\n"
                "20191108-080000F/pg_data/block-incr-wayback.pgbi {s=16384, m=0:{0,1}, ts=-172800}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");
        }

        // Ensure that disabling bundling does not break the backup. In particular this ensures the bundleRaw setting is preserved.
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 incr backup with comp/enc which disables bundling");

        backupTimeStart = BACKUP_EPOCH + 3450000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeIncr);
            hrnCfgArgRawZ(argList, cfgOptCompressType, "bz2");
            hrnCfgArgRawZ(argList, cfgOptRepoCipherType, "aes-256-cbc");
            hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Run backup
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeNone, .cipherType = cipherTypeAes256Cbc,
                .cipherPass = TEST_CIPHER_PASS, .walTotal = 1, .walSwitch = false);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191108-080000F_20191110-153320D, version = " PROJECT_VERSION "\n"
                "P00   WARN: incr backup cannot alter compress-type option to 'bz2', reset to value in 20191108-080000F_20191110-153320D\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DC8F1000000000, lsn = 5dc8f10/0\n"
                "P00   INFO: check archive for prior segment 0000000105DC8F0F000007FF\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P00 DETAIL: reference pg_data/PG_VERSION to 20191108-080000F\n"
                "P00 DETAIL: reference pg_data/block-age-multiplier to 20191108-080000F_20191110-153320D\n"
                "P00 DETAIL: reference pg_data/block-age-to-zero to 20191108-080000F_20191110-153320D\n"
                "P00 DETAIL: reference pg_data/block-incr-grow to 20191108-080000F_20191110-153320D\n"
                "P00 DETAIL: reference pg_data/block-incr-wayback to 20191108-080000F\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DC8F1000000000, lsn = 5dc8f10/100000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DC8F1000000000:0000000105DC8F1000000000\n"
                "P00   INFO: new backup label = 20191108-080000F_20191111-052640I\n"
                "P00   INFO: incr backup size = [SIZE], file total = 7");

            TEST_RESULT_STR_Z(
                testBackupValidateP(
                    storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest"), .cipherType = cipherTypeAes256Cbc,
                    .cipherPass = TEST_CIPHER_PASS),
                ".> {d=20191108-080000F_20191111-052640I}\n"
                "pg_data/backup_label.gz {s=17, ts=+2}\n"
                "pg_data/global/pg_control.gz {s=8192}\n"
                "20191108-080000F/bundle/1/pg_data/PG_VERSION {s=2, ts=-650000}\n"
                "20191108-080000F_20191110-153320D/bundle/1/pg_data/block-age-multiplier {s=32768, m=1:{0,1}, ts=-136400}\n"
                "20191108-080000F_20191110-153320D/bundle/1/pg_data/block-age-to-zero {s=16384, ts=-222800}\n"
                "20191108-080000F_20191110-153320D/bundle/1/pg_data/block-incr-grow {s=49152, m=0:{0,1},1:{0,1,2,3}, ts=-250000}\n"
                "20191108-080000F/pg_data/block-incr-wayback.pgbi {s=16384, m=0:{0,1}, ts=-222800}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 full backup with enc");

        backupTimeStart = BACKUP_EPOCH + 3500000;

        {
            // Remove old pg data
            HRN_STORAGE_PATH_REMOVE(storageTest, "pg1", .recurse = true);

            // Update pg_control
            HRN_PG_CONTROL_PUT(
                storagePgWrite(), PG_VERSION_11, .pageChecksumVersion = 1, .walSegmentSize = 2 * 1024 * 1024,
                .pageSize = pgPageSize4);

            // Update version
            HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, PG_VERSION_11_Z, .timeModified = backupTimeStart);

            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeFull);
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBundleLimit, "8KiB");
            hrnCfgArgRawZ(argList, cfgOptCompressType, "none");
            hrnCfgArgRawZ(argList, cfgOptRepoCipherType, "aes-256-cbc");
            hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // File that will grow during the backup
            Buffer *const fileGrow = bufNew(pgPageSize4 * 4);
            memset(bufPtr(fileGrow), 0, pgPageSize4 * 3);
            bufUsedSet(fileGrow, pgPageSize4 * 3);

            HRN_STORAGE_PUT(storagePgWrite(), "global/1", fileGrow, .timeModified = backupTimeStart);

            memset(bufPtr(fileGrow) + pgPageSize4 * 3, 0xFF, pgPageSize4);
            bufUsedSet(fileGrow, bufSize(fileGrow));

            // Also write a copy of it that will get a checksum error, just to be sure the read limit on global/1 is working
            HRN_STORAGE_PUT(storagePgWrite(), "global/2", fileGrow, .timeModified = backupTimeStart);

            // Run backup
            HRN_BACKUP_SCRIPT_SET(
                {.op = hrnBackupScriptOpUpdate, .file = storagePathP(storagePg(), STRDEF("global/1")),
                 .time = backupTimeStart + 1, .content = fileGrow});
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeNone, .cipherType = cipherTypeAes256Cbc,
                .cipherPass = TEST_CIPHER_PASS, .walTotal = 2, .walSwitch = true, .fullIncrNoOp = true);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            // Make sure that global/1 grew as expected but the extra bytes were not copied
            TEST_RESULT_UINT(storageInfoP(storagePgWrite(), STRDEF("global/1")).size, 16384, "check global/1 grew");

            TEST_RESULT_LOG(
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DC9B4000000000, lsn = 5dc9b40/0\n"
                "P00   INFO: check archive for segment 0000000105DC9B4000000000\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/2 (16KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: invalid page checksum found in file " TEST_PATH "/pg1/global/2 at page 3\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/1 (12KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/0, 8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (bundle 1/8224, 2B, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DC9B4000000001, lsn = 5dc9b40/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DC9B4000000000:0000000105DC9B4000000001\n"
                "P00   INFO: new backup label = 20191111-192000F\n"
                "P00   INFO: full backup size = [SIZE], file total = 5");

            TEST_RESULT_STR_Z(
                testBackupValidateP(
                    storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest"), .cipherType = cipherTypeAes256Cbc,
                    .cipherPass = TEST_CIPHER_PASS),
                ".> {d=20191111-192000F}\n"
                "bundle/1/pg_data/PG_VERSION {s=2}\n"
                "bundle/1/pg_data/global/pg_control {s=8192}\n"
                "pg_data/backup_label {s=17, ts=+2}\n"
                "pg_data/global/1 {s=12288, ckp=t}\n"
                "pg_data/global/2 {s=16384, ckp=[3]}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n",
                "compare file list");
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
