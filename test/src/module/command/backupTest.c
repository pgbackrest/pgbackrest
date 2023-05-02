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
#include "common/harnessConfig.h"
#include "common/harnessManifest.h"
#include "common/harnessPack.h"
#include "common/harnessPostgres.h"
#include "common/harnessPq.h"
#include "common/harnessProtocol.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test block delta
***********************************************************************************************************************************/
static String *
testBlockDelta(const BlockMap *const blockMap, const size_t blockSize, const size_t checksumSize)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BLOCK_MAP, blockMap);
        FUNCTION_HARNESS_PARAM(SIZE, blockSize);
        FUNCTION_HARNESS_PARAM(SIZE, checksumSize);
    FUNCTION_HARNESS_END();

    ASSERT(blockMap != NULL);
    ASSERT(blockSize > 0);

    String *const result = strNew();
    BlockDelta *const blockDelta = blockDeltaNew(blockMap, blockSize, checksumSize, NULL, cipherTypeNone, NULL, compressTypeNone);

    for (unsigned int readIdx = 0; readIdx < blockDeltaReadSize(blockDelta); readIdx++)
    {
        const BlockDeltaRead *const read = blockDeltaReadGet(blockDelta, readIdx);

        strCatFmt(
            result, "read {reference: %u, bundleId: %" PRIu64 ", offset: %" PRIu64 ", size: %" PRIu64 "}\n", read->reference,
            read->bundleId, read->offset, read->size);

        for (unsigned int superBlockIdx = 0; superBlockIdx < lstSize(read->superBlockList); superBlockIdx++)
        {
            const BlockDeltaSuperBlock *const superBlock = lstGet(read->superBlockList, superBlockIdx);

            strCatFmt(
                result, "  super block {max: %" PRIu64 ", size: %" PRIu64 "}\n", superBlock->superBlockSize, superBlock->size);

            for (unsigned int blockIdx = 0; blockIdx < lstSize(superBlock->blockList); blockIdx++)
            {
                const BlockDeltaBlock *const block = lstGet(superBlock->blockList, blockIdx);

                strCatFmt(result, "    block {no: %" PRIu64 ", offset: %" PRIu64 "}\n", block->no, block->offset);
            }
        }
    }

    FUNCTION_HARNESS_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Get a list of all files in the backup and a redacted version of the manifest that can be tested against a static string
***********************************************************************************************************************************/
static String *
testBackupValidateList(
    const Storage *const storage, const String *const path, Manifest *const manifest, const ManifestData *const manifestData,
    const CipherType cipherType, const String *const cipherPass, String *const result)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STORAGE, storage);
        FUNCTION_HARNESS_PARAM(STRING, path);
        FUNCTION_HARNESS_PARAM(MANIFEST, manifest);
        FUNCTION_HARNESS_PARAM_P(VOID, manifestData);
        FUNCTION_HARNESS_PARAM(STRING_ID, cipherType);
        FUNCTION_HARNESS_PARAM(STRING, cipherPass);
        FUNCTION_HARNESS_PARAM(STRING, result);
    FUNCTION_HARNESS_END();

    // Output root path if it is a link so we can verify the destination
    const StorageInfo dotInfo = storageInfoP(storage, path);

    if (dotInfo.type == storageTypeLink)
        strCatFmt(result, ". {link, d=%s}\n", strZ(dotInfo.linkDestination));

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
                    ManifestFilePack **const filePack = *(ManifestFilePack ***)lstGet(fileList, fileIdx);
                    ManifestFile file = manifestFileUnpack(manifest, *filePack);

                    if (bundle)
                        strCatFmt(result, "%s/%s {file", strZ(info.name), strZ(file.name));
                    else
                        strCatFmt(result, "%s {file", strZ(info.name));

                    // Validate repo checksum
                    // -------------------------------------------------------------------------------------------------------------
                    if (file.checksumRepoSha1 != NULL)
                    {
                        StorageRead *read = storageNewReadP(
                            storage, strNewFmt("%s/%s", strZ(path), strZ(info.name)), .offset = file.bundleOffset,
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
                            storage, strNewFmt("%s/%s", strZ(path), strZ(info.name)),
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
                            storage, strNewFmt("%s/%s", strZ(path), strZ(info.name)), .offset = file.bundleOffset,
                            .limit = VARUINT64(file.sizeRepo));
                        const bool raw = bundle && manifest->pub.data.bundleRaw;

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

                    strCatFmt(result, ", s=%" PRIu64, size);

                    // Test size and repo-size
                    // -------------------------------------------------------------------------------------------------------------
                    if (size != file.size)
                        THROW_FMT(AssertError, "'%s' size does match manifest", strZ(file.name));

                    // Repo size can only be compared to file size when not bundled
                    if (!bundle)
                    {
                        if (info.size != file.sizeRepo)
                            THROW_FMT(AssertError, "'%s' repo size does match manifest", strZ(file.name));
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
                }

                break;
            }

            case storageTypeLink:
                strCatFmt(result, "%s {link, d=%s}\n", strZ(info.name), strZ(info.linkDestination));
                break;

            case storageTypePath:
            {
                strCatFmt(result, "%s {path", strZ(info.name));

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

                strCatZ(result, "}\n");
                break;
            }

            case storageTypeSpecial:
                THROW_FMT(AssertError, "unexpected special file '%s'", strZ(info.name));
        }
    }

    // Check all files in manifest. Since the scan above maps from files to the manifest, any referenced files will not be checked.
    // -----------------------------------------------------------------------------------------------------------------------------
    for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
    {
        ManifestFilePack **const filePack = lstGet(manifest->pub.fileList, fileIdx);
        ManifestFile file = manifestFileUnpack(manifest, *filePack);

        // If compressed or block incremental then set the repo-size to size so it will not be in test output. Even the same
        // compression algorithm can give slightly different results based on the version so repo-size is not deterministic for
        // compression. Block incremental maps increase repo-size in a non-obvious way.
        if (manifestData->backupOptionCompressType != compressTypeNone || file.blockIncrMapSize != 0)
            file.sizeRepo = file.size;

        // Bundle id/offset are too noisy so remove them. They are verified against size/checksum and listed with the files.
        file.bundleId = 0;
        file.bundleOffset = 0;

        // Remove repo checksum since it has been validated
        file.checksumRepoSha1 = NULL;

        // Update changes to manifest file
        manifestFilePackUpdate(manifest, filePack, &file);
    }

    FUNCTION_HARNESS_RETURN(STRING, result);
}

typedef struct TestBackupValidateParam
{
    VAR_PARAM_HEADER;
    CipherType cipherType;                                          // Cipher type
    const char *cipherPass;                                         // Cipher pass
} TestBackupValidateParam;

#define testBackupValidateP(storage, path, ...)                                                                       \
    testBackupValidate(storage, path, (TestBackupValidateParam){VAR_PARAM_INIT, __VA_ARGS__})

static String *
testBackupValidate(const Storage *const storage, const String *const path, TestBackupValidateParam param)
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
        testBackupValidateList(
            storage, path, manifest, manifestData(manifest), param.cipherType == 0 ? cipherTypeNone : param.cipherType,
            param.cipherPass == NULL ? NULL : manifestCipherSubPass(manifest), result);

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

                if (strEqZ(section, INFO_SECTION_BACKREST) ||
                    strEqZ(section, INFO_SECTION_CIPHER) ||
                    strEqZ(section, MANIFEST_SECTION_BACKUP) ||
                    strEqZ(section, MANIFEST_SECTION_BACKUP_DB) ||
                    strEqZ(section, MANIFEST_SECTION_BACKUP_OPTION) ||
                    strEqZ(section, MANIFEST_SECTION_DB) ||
                    strEqZ(section, MANIFEST_SECTION_TARGET_FILE_DEFAULT) ||
                    strEqZ(section, MANIFEST_SECTION_TARGET_LINK_DEFAULT) ||
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
    setenv("TZ", "UTC", true);

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
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("segment page default");

        TEST_RESULT_UINT(PG_SEGMENT_PAGE_DEFAULT, 131072, "check pages per segment");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("two misaligned buffers in a row");

        Buffer *buffer = bufNew(513);
        bufUsedSet(buffer, bufSize(buffer));
        memset(bufPtr(buffer), 0, bufSize(buffer));

        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0};

        Buffer *bufferOut = bufNew(513);
        IoWrite *write = ioBufferWriteNew(bufferOut);
        ioFilterGroupAdd(
            ioWriteFilterGroup(write),
            pageChecksumNewPack(ioFilterParamList(pageChecksumNew(0, PG_SEGMENT_PAGE_DEFAULT, true, STRDEF(BOGUS_STR)))));
        ioWriteOpen(write);
        ioWrite(write, buffer);
        TEST_ERROR(ioWrite(write, buffer), AssertError, "should not be possible to see two misaligned pages in a row");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retry a page with an invalid checksum");

        // Write to file with valid checksums
        buffer = bufNew(PG_PAGE_SIZE_DEFAULT * 4);
        memset(bufPtr(buffer), 0, bufSize(buffer));
        bufUsedSet(buffer, bufSize(buffer));

        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0x00};
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x01)) = (PageHeaderData){.pd_upper = 0xFF};
        ((PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x01)))->pd_checksum = pgPageChecksum(
            bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x01), 1);
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x02)) = (PageHeaderData){.pd_upper = 0x00};
        *(PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x03)) = (PageHeaderData){.pd_upper = 0xFE};
        ((PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x03)))->pd_checksum = pgPageChecksum(
            bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x03), 3);

        HRN_STORAGE_PUT(storageTest, "relation", buffer);

        // Now break the checksum to force a retry
        ((PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x01)))->pd_checksum = 0;
        ((PageHeaderData *)(bufPtr(buffer) + (PG_PAGE_SIZE_DEFAULT * 0x03)))->pd_checksum = 0;

        write = ioBufferWriteNew(bufferOut);
        ioFilterGroupAdd(
            ioWriteFilterGroup(write),
            pageChecksumNew(0, PG_SEGMENT_PAGE_DEFAULT, true, storagePathP(storageTest, STRDEF("relation"))));
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
            testBlockDelta(blockMapNewRead(ioBufferReadNewOpen(buffer), 1, 5), 1, 5),
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
            "00"                                        // Blocks are unequal

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
            testBlockDelta(blockMapNewRead(ioBufferReadNewOpen(buffer), 3, 8), 3, 8),
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
            testBlockDelta(blockMapNewRead(ioBufferReadNewOpen(map), 3, 8), 3, 8),
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
            testBlockDelta(blockMapNewRead(ioBufferReadNewOpen(map), 3, 8), 3, 8),
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
            testBlockDelta(blockMapNewRead(ioBufferReadNewOpen(map), 3, 8), 3, 8),
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
        TEST_TITLE("full backup with larger super block");

        ioBufferSizeSet(2);

        source = BUFSTRZ("ABCXYZ123");
        destination = bufNew(256);
        write = ioBufferWriteNew(destination);

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
            testBlockDelta(blockMapNewRead(ioBufferReadNewOpen(map), 3, 8), 3, 8),
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
    if (testBegin("backupFile()"))
    {
        const String *pgFile = STRDEF("testfile");
        const String *missingFile = STRDEF("missing");
        const String *backupLabel = STRDEF("20190718-155825F");
        const String *backupPathFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strZ(backupLabel), strZ(pgFile));
        BackupFileResult result = {0};

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

        List *fileList = lstNewP(sizeof(BackupFile));

        BackupFile file =
        {
            .pgFile = missingFile,
            .pgFileIgnoreMissing = true,
            .pgFileSize = 0,
            .pgFileCopyExactSize = true,
            .pgFileChecksum = NULL,
            .pgFileChecksumPage = false,
            .manifestFile = missingFile,
            .manifestFileHasReference = false,
        };

        lstAdd(fileList, &file);

        const String *repoFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strZ(backupLabel), strZ(file.manifestFile));

        TEST_ASSIGN(
            result,
            *(BackupFileResult *)lstGet(
                backupFile(repoFile, 0, false, 0, compressTypeNone, 1, cipherTypeNone, NULL, fileList), 0),
            "pg file missing, ignoreMissing=true, no delta");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 0, "copy/repo size 0");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultSkip, "skip file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg file missing - ignoreMissing=false");

        fileList = lstNewP(sizeof(BackupFile));

        file = (BackupFile)
        {
            .pgFile = missingFile,
            .pgFileIgnoreMissing = false,
            .pgFileSize = 0,
            .pgFileCopyExactSize = true,
            .pgFileChecksum = NULL,
            .pgFileChecksumPage = false,
            .manifestFile = missingFile,
            .manifestFileHasReference = false,
        };

        lstAdd(fileList, &file);

        TEST_ERROR(
            backupFile(repoFile, 0, false, 0, compressTypeNone, 1, cipherTypeNone, NULL, fileList), FileMissingError,
            "unable to open missing file '" TEST_PATH "/pg/missing' for read");

        // Create a pg file to backup
        HRN_STORAGE_PUT_Z(storagePgWrite(), strZ(pgFile), "atestfile");

        // Remove repo file
        HRN_STORAGE_REMOVE(storageRepoWrite(), strZ(backupPathFile));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("test pagechecksum while db file grows");

        // Increase the file size but most of the following tests will still treat the file as size 9. This tests the common case
        // where a file grows while a backup is running.
        HRN_STORAGE_PUT_Z(storagePgWrite(), strZ(pgFile), "atestfile###");

        fileList = lstNewP(sizeof(BackupFile));

        file = (BackupFile)
        {
            .pgFile = pgFile,
            .pgFileIgnoreMissing = false,
            .pgFileSize = 9,
            .pgFileCopyExactSize = true,
            .pgFileChecksum = NULL,
            .pgFileChecksumPage = true,
            .manifestFile = pgFile,
            .manifestFileHasReference = false,
        };

        lstAdd(fileList, &file);

        repoFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strZ(backupLabel), strZ(file.manifestFile));

        TEST_ASSIGN(
            result,
            *(BackupFileResult *)lstGet(
                backupFile(repoFile, 0, false, 0, compressTypeNone, 1, cipherTypeNone, NULL, fileList), 0),
            "file checksummed with pageChecksum enabled");
        TEST_RESULT_UINT(result.copySize, 9, "copy=pgFile size");
        TEST_RESULT_UINT(result.repoSize, 9, "repo=pgFile size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, result.copyChecksum), "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum matches");
        TEST_RESULT_STR_Z(hrnPackToStr(result.pageChecksumResult), "2:bool:false, 3:bool:false", "pageChecksumResult");
        TEST_STORAGE_EXISTS(storageRepoWrite(), strZ(backupPathFile), .remove = true, .comment = "check exists in repo, remove");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pgFileSize, ignoreMissing=false, backupLabel, pgFileChecksumPage");

        fileList = lstNewP(sizeof(BackupFile));

        file = (BackupFile)
        {
            .pgFile = pgFile,
            .pgFileIgnoreMissing = false,
            .pgFileSize = 8,
            .pgFileCopyExactSize = false,
            .pgFileChecksum = NULL,
            .pgFileChecksumPage = true,
            .manifestFile = pgFile,
            .manifestFileHasReference = false,
        };

        lstAdd(fileList, &file);

        TEST_ASSIGN(
            result,
            *(BackupFileResult *)lstGet(
                backupFile(repoFile, 0, false, 0, compressTypeNone, 1, cipherTypeNone, NULL, fileList), 0),
            "backup file");
        TEST_RESULT_UINT(result.copySize, 12, "copy size");
        TEST_RESULT_UINT(result.repoSize, 12, "repo size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(strNewEncode(encodingHex, result.copyChecksum), "c3ae4687ea8ccd47bfdb190dbe7fd3b37545fdb9", "checksum");
        TEST_RESULT_STR_Z(hrnPackToStr(result.pageChecksumResult), "2:bool:false, 3:bool:false", "page checksum");
        TEST_STORAGE_GET(storageRepo(), strZ(backupPathFile), "atestfile###");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists in repo and db, checksum match - NOOP");

        fileList = lstNewP(sizeof(BackupFile));

        file = (BackupFile)
        {
            .pgFile = pgFile,
            .pgFileDelta = true,
            .pgFileIgnoreMissing = false,
            .pgFileSize = 9,
            .pgFileCopyExactSize = true,
            .pgFileChecksum = bufNewDecode(encodingHex, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67")),
            .pgFileChecksumPage = false,
            .manifestFile = pgFile,
            .manifestFileHasReference = true,
        };

        lstAdd(fileList, &file);

        // File exists in repo and db, pg checksum match, delta set, ignoreMissing false, hasReference - NOOP
        TEST_ASSIGN(
            result,
            *(BackupFileResult *)lstGet(
                backupFile(repoFile, 0, false, 0, compressTypeNone, 1, cipherTypeNone, NULL, fileList), 0),
            "file in db and repo, checksum equal, no ignoreMissing, no pageChecksum, delta, hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "copy size set");
        TEST_RESULT_UINT(result.repoSize, 0, "repo size not set since already exists in repo");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultNoOp, "noop file");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, result.copyChecksum), "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum matches");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_GET(storageRepo(), strZ(backupPathFile), "atestfile###", .comment = "file not modified");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists in repo and db, checksum mismatch - COPY");

        fileList = lstNewP(sizeof(BackupFile));

        file = (BackupFile)
        {
            .pgFile = pgFile,
            .pgFileDelta = true,
            .pgFileIgnoreMissing = false,
            .pgFileSize = 9,
            .pgFileCopyExactSize = true,
            .pgFileChecksum = bufNewDecode(encodingHex, STRDEF("1234567890123456789012345678901234567890")),
            .pgFileChecksumPage = false,
            .manifestFile = pgFile,
            .manifestFileHasReference = true,
        };

        lstAdd(fileList, &file);

        // File exists in repo and db, pg checksum mismatch, delta set, ignoreMissing false, hasReference - COPY
        TEST_ASSIGN(
            result,
            *(BackupFileResult *)lstGet(
                backupFile(repoFile, 0, false, 0, compressTypeNone, 1, cipherTypeNone, NULL, fileList), 0),
            "file in db and repo, pg checksum not equal, no ignoreMissing, no pageChecksum, delta, hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "copy 9 bytes");
        TEST_RESULT_UINT(result.repoSize, 9, "repo=copy size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, result.copyChecksum), "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67",
            "copy checksum for file size 9");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_GET(storageRepo(), strZ(backupPathFile), "atestfile", .comment = "9 bytes copied");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists in repo and pg, copy only exact file even if size passed is greater - COPY");

        fileList = lstNewP(sizeof(BackupFile));

        file = (BackupFile)
        {
            .pgFile = pgFile,
            .pgFileDelta = true,
            .pgFileIgnoreMissing = false,
            .pgFileSize = 9999999,
            .pgFileCopyExactSize = true,
            .pgFileChecksum = bufNewDecode(encodingHex, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67")),
            .pgFileChecksumPage = false,
            .manifestFile = pgFile,
            .manifestFileHasReference = true,
        };

        lstAdd(fileList, &file);

        // File exists in repo and pg, pg checksum same, pg size passed is different, delta set, ignoreMissing false, hasReference
        TEST_ASSIGN(
            result,
            *(BackupFileResult *)lstGet(
                backupFile(repoFile, 0, false, 0, compressTypeNone, 1, cipherTypeNone, NULL, fileList), 0),
            "db & repo file, pg checksum same, pg size different, no ignoreMissing, no pageChecksum, delta, hasReference");
        TEST_RESULT_UINT(result.copySize, 12, "copy=pgFile size");
        TEST_RESULT_UINT(result.repoSize, 12, "repo=pgFile size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, result.copyChecksum), "c3ae4687ea8ccd47bfdb190dbe7fd3b37545fdb9", "copy checksum updated");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_GET(storageRepo(), strZ(backupPathFile), "atestfile###", .comment = "confirm contents");

        TEST_STORAGE_LIST(
            storageRepoWrite(), STORAGE_REPO_BACKUP "/20190718-155825F", "testfile\n", .remove = true,
            .comment = "resumed file is missing in repo");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists in repo but missing from db, checksum same in repo - SKIP");

        fileList = lstNewP(sizeof(BackupFile));

        file = (BackupFile)
        {
            .pgFile = missingFile,
            .pgFileDelta = true,
            .pgFileIgnoreMissing = true,
            .pgFileSize = 9,
            .pgFileCopyExactSize = true,
            .pgFileChecksum = bufNewDecode(encodingHex, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67")),
            .pgFileChecksumPage = false,
            .manifestFile = pgFile,
            .repoFileChecksum = bufNewDecode(encodingHex, STRDEF("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67")),
            .repoFileSize = 9,
            .manifestFileResume = true,
            .manifestFileHasReference = false,
        };

        lstAdd(fileList, &file);

        TEST_ASSIGN(
            result,
            *(BackupFileResult *)lstGet(
                backupFile(repoFile, 0, false, 0, compressTypeNone, 1, cipherTypeNone, NULL, fileList), 0),
            "file in repo only, checksum in repo equal, ignoreMissing=true, no pageChecksum, delta, no hasReference");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 0, "copy=repo=0 size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultSkip, "skip file");
        TEST_RESULT_PTR(result.copyChecksum, NULL, "copy checksum NULL");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP "/20190718-155825F", NULL, .comment = "file removed from repo");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("compression set, all other boolean parameters false - COPY");

        fileList = lstNewP(sizeof(BackupFile));

        file = (BackupFile)
        {
            .pgFile = pgFile,
            .pgFileIgnoreMissing = false,
            .pgFileSize = 9,
            .pgFileCopyExactSize = true,
            .pgFileChecksum = NULL,
            .pgFileChecksumPage = false,
            .manifestFile = pgFile,
            .manifestFileHasReference = false,
        };

        lstAdd(fileList, &file);

        repoFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strZ(backupLabel), strZ(file.manifestFile));

        TEST_ASSIGN(
            result,
            *(BackupFileResult *)lstGet(
                backupFile(repoFile, 0, false, 0, compressTypeGz, 3, cipherTypeNone, NULL, fileList), 0),
            "pg file exists, no checksum, no ignoreMissing, compression, no pageChecksum, no delta, no hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "copy=pgFile size");
        TEST_RESULT_UINT(result.repoSize, 29, "repo compress size");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, result.copyChecksum), "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_EXISTS(
            storageRepo(), zNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strZ(backupLabel), strZ(pgFile)),
            .comment = "copy file to repo compress success");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create a zero sized file - checksum will be set but in backupManifestUpdate it will not be copied");

        // Create zero sized file in pg
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "zerofile");

        fileList = lstNewP(sizeof(BackupFile));

        file = (BackupFile)
        {
            .pgFile = STRDEF("zerofile"),
            .pgFileIgnoreMissing = false,
            .pgFileSize = 0,
            .pgFileCopyExactSize = true,
            .pgFileChecksum = NULL,
            .pgFileChecksumPage = false,
            .manifestFile = STRDEF("zerofile"),
            .manifestFileHasReference = false,
        };

        lstAdd(fileList, &file);

        repoFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strZ(backupLabel), strZ(file.manifestFile));

        // No prior checksum, no compression, no pageChecksum, no delta, no hasReference
        TEST_ASSIGN(
            result,
            *(BackupFileResult *)lstGet(
                backupFile(repoFile, 0, false, 0, compressTypeNone, 1, cipherTypeNone, NULL, fileList), 0),
            "zero-sized pg file exists, no repo file, no ignoreMissing, no pageChecksum, no delta, no hasReference");
        TEST_RESULT_UINT(result.copySize + result.repoSize, 0, "copy=repo=pgFile size 0");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_PTR_NE(result.copyChecksum, NULL, "checksum set");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum result is NULL");
        TEST_STORAGE_LIST(
            storageRepo(), STORAGE_REPO_BACKUP "/20190718-155825F",
            "testfile.gz\n"
            "zerofile\n",
            .comment = "copy zero file to repo success");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("copy file to encrypted repo");

        // Load Parameters
        argList = strLstNew();
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

        fileList = lstNewP(sizeof(BackupFile));

        file = (BackupFile)
        {
            .pgFile = pgFile,
            .pgFileIgnoreMissing = false,
            .pgFileSize = 9,
            .pgFileCopyExactSize = true,
            .pgFileChecksum = NULL,
            .pgFileChecksumPage = false,
            .manifestFile = pgFile,
            .manifestFileHasReference = false,
        };

        lstAdd(fileList, &file);

        repoFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strZ(backupLabel), strZ(file.manifestFile));

        // No prior checksum, no compression, no pageChecksum, no delta, no hasReference
        TEST_ASSIGN(
            result,
            *(BackupFileResult *)lstGet(
                backupFile(repoFile, 0, false, 0, compressTypeNone, 1, cipherTypeAes256Cbc, STRDEF(TEST_CIPHER_PASS), fileList), 0),
            "pg file exists, no repo file, no ignoreMissing, no pageChecksum, no delta, no hasReference");
        TEST_RESULT_UINT(result.copySize, 9, "copy size set");
        TEST_RESULT_UINT(result.repoSize, 32, "repo size set");
        TEST_RESULT_UINT(result.backupCopyResult, backupCopyResultCopy, "copy file");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, result.copyChecksum), "9bc8ab2dda60ef4beed07d1e19ce0676d5edde67", "copy checksum");
        TEST_RESULT_PTR(result.pageChecksumResult, NULL, "page checksum NULL");
        TEST_STORAGE_GET(
            storageRepo(), strZ(backupPathFile), "atestfile", .cipherType = cipherTypeAes256Cbc,
            .comment = "copy file to encrypted repo success");
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
            storageRepoWrite(),
            zNewFmt(
                STORAGE_REPO_BACKUP "/backup.history/2019/%s.manifest.gz",
                strZ(backupLabelFormat(backupTypeFull, NULL, timestamp - 4))));

        TEST_RESULT_STR(backupLabelCreate(backupTypeFull, NULL, timestamp), backupLabel, "create label");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("assign label when backup is older");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(backupLabelFormat(backupTypeFull, NULL, timestamp - 2))));

        TEST_RESULT_STR(backupLabelCreate(backupTypeFull, NULL, timestamp), backupLabel, "create label");

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
            backupInit(infoBackupNew(PG_VERSION_96, HRN_PG_SYSTEMID_96, hrnPgCatalogVersion(PG_VERSION_96), NULL)),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptBackupStandby), false, "check backup-standby");

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
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_93);

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

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_93(1, "dbname='postgres' port=5432", PG_VERSION_93, TEST_PATH "/pg1", false, NULL, NULL),

            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(
            dbFree(
                backupInit(infoBackupNew(PG_VERSION_93, HRN_PG_SYSTEMID_93, hrnPgCatalogVersion(PG_VERSION_93), NULL))->dbPrimary),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "check checksum-page");

        TEST_RESULT_LOG(
            "P00   WARN: checksum-page option set to true but checksums are not enabled on the cluster, resetting to false");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ok if cluster checksums are enabled and checksum-page is any value");

        // Create pg_control with page checksums
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_93, .pageChecksum = true);

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
            HRNPQ_MACRO_OPEN_GE_93(1, "dbname='postgres' port=5432", PG_VERSION_93, TEST_PATH "/pg1", false, NULL, NULL),

            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(
            dbFree(
                backupInit(infoBackupNew(PG_VERSION_93, HRN_PG_SYSTEMID_93, hrnPgCatalogVersion(PG_VERSION_93), NULL))->dbPrimary),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "check checksum-page");

        // Create pg_control without page checksums
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_93);

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_93(1, "dbname='postgres' port=5432", PG_VERSION_93, TEST_PATH "/pg1", false, NULL, NULL),

            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(
            dbFree(
                backupInit(infoBackupNew(PG_VERSION_93, HRN_PG_SYSTEMID_93, hrnPgCatalogVersion(PG_VERSION_93), NULL))->dbPrimary),
            "backup init");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptChecksumPage), false, "check checksum-page");
    }

    // *****************************************************************************************************************************
    if (testBegin("backupTime()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sleep retries and stall error");

        // Create pg_control
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_93);

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

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_93(1, "dbname='postgres' port=5432", PG_VERSION_93, TEST_PATH "/pg1", false, NULL, NULL),

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
            infoBackupNew(PG_VERSION_93, HRN_PG_SYSTEMID_93, hrnPgCatalogVersion(PG_VERSION_93), NULL));

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

        ProtocolParallelJob *job = protocolParallelJobNew(VARSTRDEF("key"), protocolCommandNew(strIdFromZ("x")));
        protocolParallelJobErrorSet(job, errorTypeCode(&AssertError), STRDEF("error message"));

        unsigned int currentPercentComplete = 0;

        TEST_ERROR(
            backupJobResult((Manifest *)1, NULL, storageTest, strLstNew(), job, false, 0, NULL, &currentPercentComplete),
            AssertError, "error message");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("report host/100% progress on noop result");

        // Create job that skips file
        job = protocolParallelJobNew(VARSTRDEF("pg_data/test"), protocolCommandNew(strIdFromZ("x")));

        PackWrite *const resultPack = protocolPackNew();
        pckWriteStrP(resultPack, STRDEF("pg_data/test"));
        pckWriteU32P(resultPack, backupCopyResultNoOp);
        pckWriteU64P(resultPack, 0);
        pckWriteU64P(resultPack, 0);
        pckWriteStrP(resultPack, NULL);
        pckWriteStrP(resultPack, NULL);
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

        lockInit(TEST_PATH_STR, cfgOptionStr(cfgOptExecId), cfgOptionStr(cfgOptStanza), lockTypeBackup);
        TEST_RESULT_VOID(lockAcquireP(), "acquire backup lock");

        TEST_RESULT_VOID(
            backupJobResult(
                manifest, STRDEF("host"), storageTest, strLstNew(), job, false, 0, &sizeProgress, &currentPercentComplete),
            "log noop result");
        TEST_RESULT_VOID(lockRelease(true), "release backup lock");

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
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_93);

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
                (TEST_BIG_ENDIAN() ? "542c539fabf827717812b872913d593766848434" : "cc603d502a8ecede30fa96b0b0dc35014950aba2") :
                "aca8af76589171a70c901e13080e359efedd7a08");

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
            "P00   WARN: backup '[DIFF-1]' cannot be resumed: new backup type 'incr' does not match resumable backup type 'diff'\n"
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

        sleepMSec(MSEC_PER_SEC - (timeMSec() % MSEC_PER_SEC));
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

        unsigned int backupCount = strLstSize(storageListP(storageRepoIdx(1), strNewFmt(STORAGE_PATH_BACKUP "/test1")));

        TEST_RESULT_VOID(hrnCmdBackup(), "backup");

        TEST_RESULT_LOG(
            "P00   INFO: last backup label = [FULL-2], version = " PROJECT_VERSION "\n"
            "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (3B, 100.00%) checksum c8663c2525f44b6d9c687fbceb4aafc63ed8b451\n"
            "P00 DETAIL: reference pg_data/global/pg_control to [FULL-2]\n"
            "P00 DETAIL: reference pg_data/postgresql.conf to [FULL-2]\n"
            "P00   INFO: new backup label = [DIFF-4]\n"
            "P00   INFO: diff backup size = 3B, file total = 3");
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
            ManifestData *manifestResumeData = (ManifestData *)manifestData(manifestResume);

            manifestResumeData->backupType = backupTypeFull;
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
            hrnBackupPqScriptP(PG_VERSION_95, backupTimeStart, .noArchiveCheck = true, .noWal = true);

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
            hrnCfgArgRawBool(argList, cfgOptArchiveCopy, true);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // Create a backup manifest that looks like a halted backup manifest
            Manifest *manifestResume = manifestNewBuild(
                storagePg(), PG_VERSION_95, hrnPgCatalogVersion(PG_VERSION_95), 0, true, false, false, false, NULL, NULL, NULL);
            ManifestData *manifestResumeData = (ManifestData *)manifestData(manifestResume);

            manifestResumeData->backupType = backupTypeFull;
            manifestResumeData->backupOptionCompressType = compressTypeGz;
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
                "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1570100000}\n"
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
                storagePg(), PG_VERSION_95, hrnPgCatalogVersion(PG_VERSION_95), 0, true, false, false, false, NULL, NULL, NULL);
            ManifestData *manifestResumeData = (ManifestData *)manifestData(manifestResume);

            manifestResumeData->backupType = backupTypeDiff;
            manifestResumeData->backupLabelPrior = manifestData(manifestPrior)->backupLabel;
            manifestResumeData->backupOptionCompressType = compressTypeGz;
            const String *resumeLabel = backupLabelCreate(
                backupTypeDiff, manifestData(manifestPrior)->backupLabel, backupTimeStart);
            manifestBackupLabelSet(manifestResume, resumeLabel);
            strLstAddZ(manifestResume->pub.referenceList, "BOGUS");

            // Reference in manifest
            HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/PG_VERSION.gz", strZ(resumeLabel)));

            // Reference in resumed manifest
            HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "resume-ref", .timeModified = backupTimeStart);
            HRN_STORAGE_PUT_EMPTY(storageRepoWrite(), zNewFmt(STORAGE_REPO_BACKUP "/%s/pg_data/resume-ref.gz", strZ(resumeLabel)));
            HRN_MANIFEST_FILE_ADD(manifestResume, .name = "pg_data/resume-ref", .reference = "BOGUS");

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
            hrnBackupPqScriptP(PG_VERSION_95, backupTimeStart);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            // Check log
            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191003-105320F, version = " PROJECT_VERSION "\n"
                "P00   WARN: diff backup cannot alter compress-type option to 'none', reset to value in 20191003-105320F\n"
                "P00   INFO: execute exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105D9759000000000, lsn = 5d97590/0\n"
                "P00   INFO: check archive for prior segment 0000000105D9758F000000FF\n"
                "P00   WARN: file 'time-mismatch2' has timestamp (1570200100) in the future (relative to copy start 1570200000),"
                " enabling delta checksum\n"
                "P00   WARN: resumable backup 20191003-105320F_20191004-144000D of same type exists -- invalid files will be"
                " removed then the backup will resume\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191003-105320F_20191004-144000D/pg_data/PG_VERSION.gz'"
                " from resumed backup (reference in manifest)\n"
                "P00   WARN: remove special file '" TEST_PATH "/repo/backup/test1/20191003-105320F_20191004-144000D/pg_data/pipe'"
                " from resumed backup\n"
                "P00 DETAIL: remove file '" TEST_PATH "/repo/backup/test1/20191003-105320F_20191004-144000D/pg_data/resume-ref.gz'"
                " from resumed backup (reference in resumed manifest)\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P00   WARN: resumed backup file pg_data/time-mismatch2 does not have expected checksum"
                " 984816fd329622876e14907634264e6f332e9fb3. The file will be recopied and backup will continue but this may be"
                " an issue unless the resumed backup path in the repository is known to be corrupted.\n"
                "            NOTE: this does not indicate a problem with the PostgreSQL page checksums.\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/time-mismatch2 (4B, [PCT]) checksum [SHA1]\n"
                "P00   WARN: resumed backup file pg_data/repo-size-mismatch does not have expected checksum"
                " 984816fd329622876e14907634264e6f332e9fb3. The file will be recopied and backup will continue but this may be"
                " an issue unless the resumed backup path in the repository is known to be corrupted.\n"
                "            NOTE: this does not indicate a problem with the PostgreSQL page checksums.\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/repo-size-mismatch (4B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/content-mismatch (4B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/PG_VERSION (3B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/resume-ref (0B, [PCT])\n"
                "P00 DETAIL: hardlink pg_data/PG_VERSION to 20191003-105320F\n"
                "P00 DETAIL: hardlink pg_data/postgresql.conf to 20191003-105320F\n"
                "P00   INFO: execute exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105D9759000000000, lsn = 5d97590/800000\n"
                "P00   INFO: check archive for segment(s) 0000000105D9759000000000:0000000105D9759000000000\n"
                "P00   INFO: new backup label = 20191003-105320F_20191004-144000D\n"
                "P00   INFO: diff backup size = [SIZE], file total = 7");

            // Check repo directory
            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ". {link, d=20191003-105320F_20191004-144000D}\n"
                "pg_data {path}\n"
                "pg_data/PG_VERSION.gz {file, s=3}\n"
                "pg_data/content-mismatch.gz {file, s=4}\n"
                "pg_data/global {path}\n"
                "pg_data/global/pg_control.gz {file, s=8192}\n"
                "pg_data/pg_xlog {path}\n"
                "pg_data/postgresql.conf.gz {file, s=11}\n"
                "pg_data/repo-size-mismatch.gz {file, s=4}\n"
                "pg_data/resume-ref.gz {file, s=0}\n"
                "pg_data/time-mismatch2.gz {file, s=4}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"06d06bb31b570b94d7b4325f511f853dbe771c21\",\"reference\":\"20191003-105320F\""
                ",\"size\":3,\"timestamp\":1570000000}\n"
                "pg_data/content-mismatch={\"checksum\":\"984816fd329622876e14907634264e6f332e9fb3\",\"size\":4"
                ",\"timestamp\":1570200000}\n"
                "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1570200000}\n"
                "pg_data/postgresql.conf={\"checksum\":\"e3db315c260e79211b7b52587123b7aa060f30ab\""
                ",\"reference\":\"20191003-105320F\",\"size\":11,\"timestamp\":1570000000}\n"
                "pg_data/repo-size-mismatch={\"checksum\":\"984816fd329622876e14907634264e6f332e9fb3\",\"size\":4"
                ",\"timestamp\":1570200000}\n"
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
            HRN_STORAGE_REMOVE(storagePgWrite(), "content-mismatch", .errorOnMissing = true);
            HRN_STORAGE_REMOVE(storagePgWrite(), "repo-size-mismatch", .errorOnMissing = true);
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
            HRN_STORAGE_PUT_EMPTY(storagePgIdxWrite(0), PG_PATH_BASE "/1/1", .timeModified = backupTimeStart);
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
                PG_VERSION_96, backupTimeStart, .noPriorWal = true, .backupStandby = true, .walCompressType = compressTypeGz);
            TEST_ERROR(
                hrnCmdBackup(), ArchiveTimeoutError,
                "WAL segment 0000000105DA69BF000000FF was not archived before the 100ms timeout\n"
                "HINT: check the archive_command to ensure that all options are correct (especially --stanza).\n"
                "HINT: check the PostgreSQL server log for errors.\n"
                "HINT: run the 'start' command if the stanza was previously stopped.");

            // Run backup but error on archive check
            hrnBackupPqScriptP(
                PG_VERSION_96, backupTimeStart, .noWal = true, .backupStandby = true, .walCompressType = compressTypeGz);
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
            hrnBackupPqScriptP(PG_VERSION_96, backupTimeStart, .backupStandby = true, .walCompressType = compressTypeGz);
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
                "pg_data/base/1/1={\"size\":0,\"timestamp\":1571200000}\n"
                "pg_data/base/1/2={\"checksum\":\"54ceb91256e8190e474aa752a6e0650a2df5ba37\",\"size\":2,\"timestamp\":1571200000}\n"
                "pg_data/base/1/3={\"checksum\":\"3c01bdbb26f358bab27f267924aa2c9a03fcfdb8\",\"size\":3,\"timestamp\":1571200000}\n"
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
            HRN_STORAGE_PATH_REMOVE(storagePgIdxWrite(1), NULL, .recurse = true);
            HRN_STORAGE_PATH_REMOVE(storagePgWrite(), "base/1", .recurse = true);
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 full backup with tablespaces and page checksums");

        backupTimeStart = BACKUP_EPOCH + 2200000;

        {
            // Update pg_control
            HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_11, .pageChecksum = true, .walSegmentSize = 1024 * 1024);

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
            Buffer *relation = bufNew(PG_PAGE_SIZE_DEFAULT);
            memset(bufPtr(relation), 0, bufSize(relation));
            bufUsedSet(relation, bufSize(relation));

            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0};

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/1", relation, .timeModified = backupTimeStart);

            // File which will fail on alignment
            relation = bufNew(PG_PAGE_SIZE_DEFAULT + 512);
            memset(bufPtr(relation), 0, bufSize(relation));
            bufUsedSet(relation, bufSize(relation));

            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0xFE};
            ((PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x00)))->pd_checksum = pgPageChecksum(
                bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x00), 0);
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x01)) = (PageHeaderData){.pd_upper = 0xFF};

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/2", relation, .timeModified = backupTimeStart);
            const char *rel1_2Sha1 = strZ(strNewEncode(encodingHex, cryptoHashOne(hashTypeSha1, relation)));

            // File with bad page checksums
            relation = bufNew(PG_PAGE_SIZE_DEFAULT * 5);
            memset(bufPtr(relation), 0, bufSize(relation));
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0xFF};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x01)) = (PageHeaderData){.pd_upper = 0x00};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x02)) = (PageHeaderData){.pd_upper = 0xFE};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x03)) = (PageHeaderData){.pd_upper = 0xEF};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x04)) = (PageHeaderData){.pd_upper = 0x00};
            (bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x04))[PG_PAGE_SIZE_DEFAULT - 1] = 0xFF;
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/3", relation, .timeModified = backupTimeStart);
            const char *rel1_3Sha1 = strZ(strNewEncode(encodingHex, cryptoHashOne(hashTypeSha1, relation)));

            // File with bad page checksum
            relation = bufNew(PG_PAGE_SIZE_DEFAULT * 3);
            memset(bufPtr(relation), 0, bufSize(relation));
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0x00};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x01)) = (PageHeaderData){.pd_upper = 0x08};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x02)) = (PageHeaderData){.pd_upper = 0xFF};
            ((PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x02)))->pd_checksum = pgPageChecksum(
                bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x02), 2);
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/4", relation, .timeModified = backupTimeStart);
            const char *rel1_4Sha1 = strZ(strNewEncode(encodingHex, cryptoHashOne(hashTypeSha1, relation)));

            // Add a tablespace
            HRN_STORAGE_PATH_CREATE(storagePgWrite(), PG_PATH_PGTBLSPC);
            THROW_ON_SYS_ERROR(
                symlink("../../pg1-tblspc/32768", strZ(storagePathP(storagePg(), STRDEF(PG_PATH_PGTBLSPC "/32768")))) == -1,
                FileOpenError, "unable to create symlink");

            HRN_STORAGE_PUT_EMPTY(
                storageTest,
                zNewFmt("pg1-tblspc/32768/%s/1/5", strZ(pgTablespaceId(PG_VERSION_11, hrnPgCatalogVersion(PG_VERSION_11)))),
                .timeModified = backupTimeStart);

            // Disable storageFeatureSymLink so tablespace (and latest) symlinks will not be created
            ((Storage *)storageRepoWrite())->pub.interface.feature ^= 1 << storageFeatureSymLink;

            // Disable storageFeatureHardLink so hardlinks will not be created
            ((Storage *)storageRepoWrite())->pub.interface.feature ^= 1 << storageFeatureHardLink;

            // Run backup
            hrnBackupPqScriptP(PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 3);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            // Reset storage features
            ((Storage *)storageRepoWrite())->pub.interface.feature |= 1 << storageFeatureSymLink;
            ((Storage *)storageRepoWrite())->pub.interface.feature |= 1 << storageFeatureHardLink;

            TEST_RESULT_LOG(
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DB5DE000000000, lsn = 5db5de0/0\n"
                "P00   INFO: check archive for segment 0000000105DB5DE000000000\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/3 (40KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: invalid page checksums found in file " TEST_PATH "/pg1/base/1/3 at pages 0, 2-4\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/4 (24KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: invalid page checksum found in file " TEST_PATH "/pg1/base/1/4 at page 1\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/2 (8.5KB, [PCT]) checksum [SHA1]\n"
                "P00   WARN: page misalignment in file " TEST_PATH "/pg1/base/1/2: file size 8704 is not divisible by page size"
                " 8192\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/1 (8KB, [PCT]) checksum [SHA1]\n"
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

            TEST_RESULT_STR(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/20191027-181320F")),
                strNewFmt(
                    "pg_data {path}\n"
                    "pg_data/PG_VERSION.gz {file, s=2}\n"
                    "pg_data/backup_label.gz {file, s=17}\n"
                    "pg_data/base {path}\n"
                    "pg_data/base/1 {path}\n"
                    "pg_data/base/1/1.gz {file, s=8192}\n"
                    "pg_data/base/1/2.gz {file, s=8704}\n"
                    "pg_data/base/1/3.gz {file, s=40960}\n"
                    "pg_data/base/1/4.gz {file, s=24576}\n"
                    "pg_data/global {path}\n"
                    "pg_data/global/pg_control.gz {file, s=8192}\n"
                    "pg_data/pg_tblspc {path}\n"
                    "pg_data/pg_wal {path}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000000.gz {file, s=1048576}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000001.gz {file, s=1048576}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000002.gz {file, s=1048576}\n"
                    "pg_data/postgresql.conf.gz {file, s=11}\n"
                    "pg_data/tablespace_map.gz {file, s=19}\n"
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
                    ",\"size\":8192,\"timestamp\":1572200000}\n"
                    "pg_data/base/1/2={\"checksum\":\"%s\",\"checksum-page\":false,\"size\":8704,\"timestamp\":1572200000}\n"
                    "pg_data/base/1/3={\"checksum\":\"%s\",\"checksum-page\":false,\"checksum-page-error\":[0,[2,4]]"
                    ",\"size\":40960,\"timestamp\":1572200000}\n"
                    "pg_data/base/1/4={\"checksum\":\"%s\",\"checksum-page\":false,\"checksum-page-error\":[1],\"size\":24576"
                    ",\"timestamp\":1572200000}\n"
                    "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1572200000}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000000={\"size\":1048576,\"timestamp\":1572200002}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000001={\"size\":1048576,\"timestamp\":1572200002}\n"
                    "pg_data/pg_wal/0000000105DB5DE000000002={\"size\":1048576,\"timestamp\":1572200002}\n"
                    "pg_data/postgresql.conf={\"checksum\":\"e3db315c260e79211b7b52587123b7aa060f30ab\",\"size\":11"
                    ",\"timestamp\":1570000000}\n"
                    "pg_data/tablespace_map={\"checksum\":\"87fe624d7976c2144e10afcb7a9a49b071f35e9c\",\"size\":19"
                    ",\"timestamp\":1572200002}\n"
                    "pg_tblspc/32768/PG_11_201809051/1/5={\"checksum-page\":true,\"size\":0,\"timestamp\":1572200000}\n"
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
                    rel1_2Sha1, rel1_3Sha1, rel1_4Sha1),
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
            Buffer *relation = bufNew(PG_PAGE_SIZE_DEFAULT * 4);
            memset(bufPtr(relation), 0, bufSize(relation));
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x00)) = (PageHeaderData){.pd_upper = 0xFF};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x01)) = (PageHeaderData){.pd_upper = 0x00};
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x02)) = (PageHeaderData){.pd_upper = 0x00};
            (bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x02))[PG_PAGE_SIZE_DEFAULT - 1] = 0xFF;
            ((PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x02)))->pd_checksum = pgPageChecksum(
                bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x02), 2);
            *(PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x03)) = (PageHeaderData){.pd_upper = 0x00};
            (bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x03))[PG_PAGE_SIZE_DEFAULT - 1] = 0xEE;
            ((PageHeaderData *)(bufPtr(relation) + (PG_PAGE_SIZE_DEFAULT * 0x03)))->pd_checksum = 1;
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/3", relation, .timeModified = backupTimeStart);
            const char *rel1_3Sha1 = strZ(strNewEncode(encodingHex, cryptoHashOne(hashTypeSha1, relation)));

            // Run backup. Make sure that the timeline selected converts to hexdecimal that can't be interpreted as decimal.
            hrnBackupPqScriptP(PG_VERSION_11, backupTimeStart, .timeline = 0x2C, .walTotal = 2);
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
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/base/1/1 (8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/postgresql.conf (11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: match file from prior backup " TEST_PATH "/pg1/PG_VERSION (2B, [PCT]) checksum [SHA1]\n"
                "P00 DETAIL: hardlink pg_data/PG_VERSION to 20191027-181320F\n"
                "P00 DETAIL: hardlink pg_data/base/1/1 to 20191027-181320F\n"
                "P00 DETAIL: hardlink pg_data/postgresql.conf to 20191027-181320F\n"
                "P00 DETAIL: hardlink pg_tblspc/32768/PG_11_201809051/1/5 to 20191027-181320F\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000002C05DB8EB000000001, lsn = 5db8eb0/180000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00 DETAIL: wrote 'tablespace_map' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000002C05DB8EB000000000:0000002C05DB8EB000000001\n"
                "P00   INFO: new backup label = 20191027-181320F_20191030-014640I\n"
                "P00   INFO: incr backup size = [SIZE], file total = 8");

            TEST_RESULT_STR(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                strNewFmt(
                    ". {link, d=20191027-181320F_20191030-014640I}\n"
                    "pg_data {path}\n"
                    "pg_data/PG_VERSION.gz {file, s=2}\n"
                    "pg_data/backup_label.gz {file, s=17}\n"
                    "pg_data/base {path}\n"
                    "pg_data/base/1 {path}\n"
                    "pg_data/base/1/1.gz {file, s=8192}\n"
                    "pg_data/base/1/3.gz {file, s=32768}\n"
                    "pg_data/global {path}\n"
                    "pg_data/global/pg_control.gz {file, s=8192}\n"
                    "pg_data/pg_tblspc {path}\n"
                    "pg_data/pg_tblspc/32768 {link, d=../../pg_tblspc/32768}\n"
                    "pg_data/pg_wal {path}\n"
                    "pg_data/postgresql.conf.gz {file, s=11}\n"
                    "pg_data/tablespace_map.gz {file, s=19}\n"
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
                    "pg_data/PG_VERSION={\"checksum\":\"17ba0791499db908433b80f37c5fbc89b870084b\""
                    ",\"reference\":\"20191027-181320F\",\"size\":2,\"timestamp\":1572200000}\n"
                    "pg_data/backup_label={\"checksum\":\"8e6f41ac87a7514be96260d65bacbffb11be77dc\",\"size\":17"
                    ",\"timestamp\":1572400002}\n"
                    "pg_data/base/1/1={\"checksum\":\"0631457264ff7f8d5fb1edc2c0211992a67c73e6\",\"checksum-page\":true"
                    ",\"reference\":\"20191027-181320F\",\"size\":8192,\"timestamp\":1572200000}\n"
                    "pg_data/base/1/3={\"checksum\":\"%s\",\"checksum-page\":false,\"checksum-page-error\":[0,3],\"size\":32768"
                    ",\"timestamp\":1572400000}\n"
                    "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1572400000}\n"
                    "pg_data/postgresql.conf={\"checksum\":\"e3db315c260e79211b7b52587123b7aa060f30ab\""
                    ",\"reference\":\"20191027-181320F\",\"size\":11,\"timestamp\":1570000000}\n"
                    "pg_data/tablespace_map={\"checksum\":\"87fe624d7976c2144e10afcb7a9a49b071f35e9c\",\"size\":19"
                    ",\"timestamp\":1572400002}\n"
                    "pg_tblspc/32768/PG_11_201809051/1/5={\"checksum-page\":true,\"reference\":\"20191027-181320F\",\"size\":0"
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
                    rel1_3Sha1),
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
            HRN_PG_CONTROL_OVERRIDE_PUT(
                storagePgWrite(), PG_VERSION_11, 1501, .catalogVersion = 202211110, .pageChecksum = true,
                .walSegmentSize = 1024 * 1024);

            // Set to a smaller values than the defaults allow
            cfgOptionSet(cfgOptRepoBundleSize, cfgSourceParam, VARINT64(PG_PAGE_SIZE_DEFAULT));
            cfgOptionSet(cfgOptRepoBundleLimit, cfgSourceParam, VARINT64(PG_PAGE_SIZE_DEFAULT));

            // Zero-length file to be stored
            HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "zero", .timeModified = backupTimeStart);

            // Zeroed file which passes page checksums
            Buffer *relation = bufNew(PG_PAGE_SIZE_DEFAULT * 3);
            memset(bufPtr(relation), 0, bufSize(relation));
            bufUsedSet(relation, bufSize(relation));

            HRN_STORAGE_PUT(storagePgWrite(), PG_PATH_BASE "/1/2", relation, .timeModified = backupTimeStart);

            // Old files
            HRN_STORAGE_PUT_Z(storagePgWrite(), "postgresql.auto.conf", "CONFIGSTUFF2", .timeModified = 1500000000);
            HRN_STORAGE_PUT_Z(storagePgWrite(), "stuff.conf", "CONFIGSTUFF3", .timeModified = 1500000000);

            // File that will get skipped while bundling smaller files and end up a bundle by itself
            Buffer *bigish = bufNew(PG_PAGE_SIZE_DEFAULT - 1);
            memset(bufPtr(bigish), 0, bufSize(bigish));
            bufUsedSet(bigish, bufSize(bigish));

            HRN_STORAGE_PUT(storagePgWrite(), "bigish.dat", bigish, .timeModified = 1500000001);

            // Run backup
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 2, .pgVersionForce = STRDEF("11"));
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DB8EB000000000, lsn = 5db8eb0/0\n"
                "P00   INFO: check archive for segment 0000000105DB8EB000000000\n"
                "P00 DETAIL: store zero-length file " TEST_PATH "/pg1/zero\n"
                "P00 DETAIL: store zero-length file " TEST_PATH "/pg1/pg_tblspc/32768/PG_11_201809051/1/5\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/2 (24KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/stuff.conf (bundle 1/0, 12B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.auto.conf (bundle 1/32, 12B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/postgresql.conf (bundle 1/64, 11B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (bundle 1/95, 2B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/bigish.dat (bundle 2/0, 8.0KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/base/1/1 (bundle 3/0, 8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 4/0, 8KB, [PCT]) checksum [SHA1]\n"
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
                ". {link, d=20191030-014640F}\n"
                "bundle {path}\n"
                "bundle/1/pg_data/PG_VERSION {file, s=2}\n"
                "bundle/1/pg_data/postgresql.auto.conf {file, s=12}\n"
                "bundle/1/pg_data/postgresql.conf {file, s=11}\n"
                "bundle/1/pg_data/stuff.conf {file, s=12}\n"
                "bundle/2/pg_data/bigish.dat {file, s=8191}\n"
                "bundle/3/pg_data/base/1/1 {file, s=8192}\n"
                "bundle/4/pg_data/global/pg_control {file, s=8192}\n"
                "pg_data {path}\n"
                "pg_data/backup_label.gz {file, s=17}\n"
                "pg_data/base {path}\n"
                "pg_data/base/1 {path}\n"
                "pg_data/base/1/2.gz {file, s=24576}\n"
                "pg_data/pg_wal {path}\n"
                "pg_data/pg_wal/0000000105DB8EB000000000.gz {file, s=1048576}\n"
                "pg_data/pg_wal/0000000105DB8EB000000001.gz {file, s=1048576}\n"
                "pg_data/tablespace_map.gz {file, s=19}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "pg_tblspc/32768={\"path\":\"../../pg1-tblspc/32768\",\"tablespace-id\":\"32768\""
                ",\"tablespace-name\":\"tblspc32768\",\"type\":\"link\"}\n"
                "\n"
                "[metadata]\n"
                "annotation={\"extra key\":\"this is an annotation\",\"source\":\"this is another annotation\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"17ba0791499db908433b80f37c5fbc89b870084b\",\"size\":2"
                ",\"timestamp\":1572200000}\n"
                "pg_data/backup_label={\"checksum\":\"8e6f41ac87a7514be96260d65bacbffb11be77dc\",\"size\":17"
                ",\"timestamp\":1572400002}\n"
                "pg_data/base/1/1={\"checksum\":\"0631457264ff7f8d5fb1edc2c0211992a67c73e6\",\"checksum-page\":true,\"size\":8192"
                ",\"timestamp\":1572200000}\n"
                "pg_data/base/1/2={\"checksum\":\"ebdd38b69cd5b9f2d00d273c981e16960fbbb4f7\",\"checksum-page\":true,\"size\":24576"
                ",\"timestamp\":1572400000}\n"
                "pg_data/bigish.dat={\"checksum\":\"3e5175386be683d2f231f3fa3eab892a799082f7\",\"size\":8191"
                ",\"timestamp\":1500000001}\n"
                "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1572400000}\n"
                "pg_data/pg_wal/0000000105DB8EB000000000={\"size\":1048576,\"timestamp\":1572400002}\n"
                "pg_data/pg_wal/0000000105DB8EB000000001={\"size\":1048576,\"timestamp\":1572400002}\n"
                "pg_data/postgresql.auto.conf={\"checksum\":\"e873a5cb5a67e48761e7b619c531311404facdce\",\"size\":12"
                ",\"timestamp\":1500000000}\n"
                "pg_data/postgresql.conf={\"checksum\":\"e3db315c260e79211b7b52587123b7aa060f30ab\",\"size\":11"
                ",\"timestamp\":1570000000}\n"
                "pg_data/stuff.conf={\"checksum\":\"55a9d0d18b77789c7722abe72aa905e2dc85bb5d\",\"size\":12"
                ",\"timestamp\":1500000000}\n"
                "pg_data/tablespace_map={\"checksum\":\"87fe624d7976c2144e10afcb7a9a49b071f35e9c\",\"size\":19"
                ",\"timestamp\":1572400002}\n"
                "pg_data/zero={\"size\":0,\"timestamp\":1572400000}\n"
                "pg_tblspc/32768/PG_11_201809051/1/5={\"checksum-page\":true,\"size\":0,\"timestamp\":1572200000}\n"
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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 diff backup with bundles");

        backupTimeStart = BACKUP_EPOCH + 2600000;

        {
            // Remove old pg data
            HRN_STORAGE_PATH_REMOVE(storageTest, "pg1-data", .recurse = true);
            HRN_STORAGE_PATH_REMOVE(storageTest, "pg1-tblspc", .recurse = true);
            HRN_STORAGE_REMOVE(storageTest, "pg1");

            // Update pg_control
            HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_11, .pageChecksum = false, .walSegmentSize = 2 * 1024 * 1024);

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
            hrnBackupPqScriptP(PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 2);
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
                "P00 DETAIL: wrote 'tablespace_map' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DBBF8000000000:0000000105DBBF8000000001\n"
                "P00   INFO: new backup label = 20191030-014640F_20191101-092000D\n"
                "P00   INFO: diff backup size = [SIZE], file total = 5");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ". {link, d=20191030-014640F_20191101-092000D}\n"
                "bundle {path}\n"
                "bundle/1/pg_data/global/pg_control {file, s=8192}\n"
                "pg_data {path}\n"
                "pg_data/backup_label.gz {file, s=17}\n"
                "pg_data/tablespace_map.gz {file, s=19}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"17ba0791499db908433b80f37c5fbc89b870084b\",\"reference\":\"20191030-014640F\""
                ",\"size\":2,\"timestamp\":1572600000}\n"
                "pg_data/backup_label={\"checksum\":\"8e6f41ac87a7514be96260d65bacbffb11be77dc\",\"size\":17"
                ",\"timestamp\":1572600002}\n"
                "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1572600000}\n"
                "pg_data/tablespace_map={\"checksum\":\"87fe624d7976c2144e10afcb7a9a49b071f35e9c\",\"size\":19"
                ",\"timestamp\":1572600002}\n"
                "pg_data/zero={\"size\":0,\"timestamp\":1572600000}\n"
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/global={}\n",
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
            HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_11, .pageChecksum = false, .walSegmentSize = 2 * 1024 * 1024);

            // Update version
            HRN_STORAGE_PUT_Z(storagePgWrite(), PG_FILE_PGVERSION, PG_VERSION_11_Z, .timeModified = backupTimeStart);

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

            // File that uses block incr and will grow
            Buffer *file = bufNew(BLOCK_MIN_SIZE * 3);
            memset(bufPtr(file), 0, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-grow", file, .timeModified = backupTimeStart);

            // File that shrinks below the limit
            file = bufNew(BLOCK_MIN_FILE_SIZE + 1);
            memset(bufPtr(file), 55, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-shrink", file, .timeModified = backupTimeStart);

            // File that grows above the limit
            file = bufNew(BLOCK_MIN_FILE_SIZE - 1);
            memset(bufPtr(file), 77, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "grow-to-block-incr", file, .timeModified = backupTimeStart);

            // Run backup
            hrnBackupPqScriptP(PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 2);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DBF06000000000, lsn = 5dbf060/0\n"
                "P00   INFO: check archive for segment 0000000105DBF06000000000\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-grow (24KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/grow-to-block-incr (bundle 1/0, 16.0KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/16383, 8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-shrink (bundle 1/24575, 16KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (bundle 1/40989, 2B, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DBF06000000001, lsn = 5dbf060/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00 DETAIL: wrote 'tablespace_map' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DBF06000000000:0000000105DBF06000000001\n"
                "P00   INFO: new backup label = 20191103-165320F\n"
                "P00   INFO: full backup size = [SIZE], file total = 7");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ". {link, d=20191103-165320F}\n"
                "bundle {path}\n"
                "bundle/1/pg_data/PG_VERSION {file, s=2}\n"
                "bundle/1/pg_data/block-incr-shrink {file, m=0:{0,1,2}, s=16385}\n"
                "bundle/1/pg_data/global/pg_control {file, s=8192}\n"
                "bundle/1/pg_data/grow-to-block-incr {file, s=16383}\n"
                "pg_data {path}\n"
                "pg_data/backup_label {file, s=17}\n"
                "pg_data/block-incr-grow.pgbi {file, m=0:{0,1,2}, s=24576}\n"
                "pg_data/tablespace_map {file, s=19}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"17ba0791499db908433b80f37c5fbc89b870084b\",\"size\":2"
                ",\"timestamp\":1572800000}\n"
                "pg_data/backup_label={\"checksum\":\"8e6f41ac87a7514be96260d65bacbffb11be77dc\",\"size\":17"
                ",\"timestamp\":1572800002}\n"
                "pg_data/block-incr-grow={\"bi\":1,\"bim\":24,\"checksum\":\"ebdd38b69cd5b9f2d00d273c981e16960fbbb4f7\""
                ",\"size\":24576,\"timestamp\":1572800000}\n"
                "pg_data/block-incr-shrink={\"bi\":1,\"bim\":29,\"checksum\":\"ce5f8864058b1bb274244b512cb9641355987134\""
                ",\"size\":16385,\"timestamp\":1572800000}\n"
                "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1572800000}\n"
                "pg_data/grow-to-block-incr={\"checksum\":\"f5a5c308cf5fcb52bccebe2365f8ed56acbcc41d\",\"size\":16383"
                ",\"timestamp\":1572800000}\n"
                "pg_data/tablespace_map={\"checksum\":\"87fe624d7976c2144e10afcb7a9a49b071f35e9c\",\"size\":19"
                ",\"timestamp\":1572800002}\n"
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/global={}\n",
                "compare file list");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 diff backup with block incr");

        backupTimeStart = BACKUP_EPOCH + 3000000;

        {
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

            // Shrink file below the limit
            file = bufNew(BLOCK_MIN_FILE_SIZE - 1);
            memset(bufPtr(file), 55, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-shrink", file, .timeModified = backupTimeStart);

            // Grow file above the limit
            file = bufNew(BLOCK_MIN_FILE_SIZE + 1);
            memset(bufPtr(file), 77, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "grow-to-block-incr", file, .timeModified = backupTimeStart);

            // Run backup
            hrnBackupPqScriptP(PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeGz, .walTotal = 2);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191103-165320F, version = " PROJECT_VERSION "\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DC213000000000, lsn = 5dc2130/0\n"
                "P00   INFO: check archive for segment 0000000105DC213000000000\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-larger (1.4MB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-grow (128KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/grow-to-block-incr (bundle 1/0, 16KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/16411, 8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-shrink (bundle 1/24603, 16.0KB, [PCT]) checksum [SHA1]\n"
                "P00 DETAIL: reference pg_data/PG_VERSION to 20191103-165320F\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DC213000000001, lsn = 5dc2130/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00 DETAIL: wrote 'tablespace_map' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DC213000000000:0000000105DC213000000001\n"
                "P00   INFO: new backup label = 20191103-165320F_20191106-002640D\n"
                "P00   INFO: diff backup size = [SIZE], file total = 8");

            TEST_RESULT_STR_Z(
                testBackupValidateP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest")),
                ". {link, d=20191103-165320F_20191106-002640D}\n"
                "bundle {path}\n"
                "bundle/1/pg_data/block-incr-shrink {file, s=16383}\n"
                "bundle/1/pg_data/global/pg_control {file, s=8192}\n"
                "bundle/1/pg_data/grow-to-block-incr {file, m=1:{0,1,2}, s=16385}\n"
                "pg_data {path}\n"
                "pg_data/backup_label {file, s=17}\n"
                "pg_data/block-incr-grow.pgbi {file, m=0:{0},1:{0},0:{2},1:{1,2,3,4,5,6,7,8,9,10,11,12,13}, s=131072}\n"
                "pg_data/block-incr-larger.pgbi {file, m=1:{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},1:{0,1,2,3,4,5,6}, s=1507328}\n"
                "pg_data/tablespace_map {file, s=19}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"17ba0791499db908433b80f37c5fbc89b870084b\",\"reference\":\"20191103-165320F\""
                ",\"size\":2,\"timestamp\":1572800000}\n"
                "pg_data/backup_label={\"checksum\":\"8e6f41ac87a7514be96260d65bacbffb11be77dc\",\"size\":17"
                ",\"timestamp\":1573000002}\n"
                "pg_data/block-incr-grow={\"bi\":1,\"bim\":114,\"checksum\":\"1ddde8db92dd9019be0819ae4f9ad9cea2fae399\""
                ",\"size\":131072,\"timestamp\":1573000000}\n"
                "pg_data/block-incr-larger={\"bi\":8,\"bic\":7,\"bim\":173"
                ",\"checksum\":\"eec53a6da79c00b3c658a7e09f44b3e9efefd960\",\"size\":1507328,\"timestamp\":1573000000}\n"
                "pg_data/block-incr-shrink={\"checksum\":\"1c6a17f67562d8b3f64f1b5f2ee592a4c2809b3b\",\"size\":16383"
                ",\"timestamp\":1573000000}\n"
                "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1573000000}\n"
                "pg_data/grow-to-block-incr={\"bi\":1,\"bim\":26,\"checksum\":\"4f560611d9dc9212432970e5c4bec15d876c226e\","
                "\"size\":16385,\"timestamp\":1573000000}\n"
                "pg_data/tablespace_map={\"checksum\":\"87fe624d7976c2144e10afcb7a9a49b071f35e9c\",\"size\":19"
                ",\"timestamp\":1573000002}\n"
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/global={}\n",
                "compare file list");

            HRN_STORAGE_REMOVE(storagePgWrite(), "block-incr-grow");
            HRN_STORAGE_REMOVE(storagePgWrite(), "block-incr-larger");
            HRN_STORAGE_REMOVE(storagePgWrite(), "block-incr-shrink");
            HRN_STORAGE_REMOVE(storagePgWrite(), "grow-to-block-incr");
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

            // Run backup
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeNone, .cipherType = cipherTypeAes256Cbc,
                .cipherPass = TEST_CIPHER_PASS, .walTotal = 2);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DC520000000000, lsn = 5dc5200/0\n"
                "P00   INFO: check archive for segment 0000000105DC520000000000\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-grow (24KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/PG_VERSION (bundle 1/0, 2B, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/24, 8KB, [PCT]) checksum [SHA1]\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DC520000000001, lsn = 5dc5200/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00 DETAIL: wrote 'tablespace_map' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DC520000000000:0000000105DC520000000001\n"
                "P00   INFO: new backup label = 20191108-080000F\n"
                "P00   INFO: full backup size = [SIZE], file total = 5");

            TEST_RESULT_STR_Z(
                testBackupValidateP(
                    storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest"), .cipherType = cipherTypeAes256Cbc,
                    .cipherPass = TEST_CIPHER_PASS),
                ". {link, d=20191108-080000F}\n"
                "bundle {path}\n"
                "bundle/1/pg_data/PG_VERSION {file, s=2}\n"
                "bundle/1/pg_data/global/pg_control {file, s=8192}\n"
                "pg_data {path}\n"
                "pg_data/backup_label.gz {file, s=17}\n"
                "pg_data/block-incr-grow.pgbi {file, m=0:{0,1,2}, s=24576}\n"
                "pg_data/tablespace_map.gz {file, s=19}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"17ba0791499db908433b80f37c5fbc89b870084b\",\"size\":2"
                ",\"timestamp\":1572800000}\n"
                "pg_data/backup_label={\"checksum\":\"8e6f41ac87a7514be96260d65bacbffb11be77dc\",\"size\":17"
                ",\"timestamp\":1573200002}\n"
                "pg_data/block-incr-grow={\"bi\":1,\"bim\":40,\"checksum\":\"ebdd38b69cd5b9f2d00d273c981e16960fbbb4f7\","
                "\"size\":24576,\"timestamp\":1573200000}\n"
                "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1573200000}\n"
                "pg_data/tablespace_map={\"checksum\":\"87fe624d7976c2144e10afcb7a9a49b071f35e9c\",\"size\":19"
                ",\"timestamp\":1573200002}\n"
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/global={}\n",
                "compare file list");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("online 11 diff backup with comp/enc");

        backupTimeStart = BACKUP_EPOCH + 3400000;

        {
            // Load options
            StringList *argList = strLstNew();
            hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
            hrnCfgArgRaw(argList, cfgOptRepoPath, repoPath);
            hrnCfgArgRaw(argList, cfgOptPgPath, pg1Path);
            hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
            hrnCfgArgRawStrId(argList, cfgOptType, backupTypeDiff);
            hrnCfgArgRawBool(argList, cfgOptRepoBundle, true);
            hrnCfgArgRawZ(argList, cfgOptRepoBundleLimit, "4MiB");
            hrnCfgArgRawBool(argList, cfgOptRepoBlock, true);
            hrnCfgArgRawZ(argList, cfgOptRepoCipherType, "aes-256-cbc");
            hrnCfgArgRawZ(argList, cfgOptRepoBlockAgeMap, "1=2");
            hrnCfgArgRawZ(argList, cfgOptRepoBlockChecksumSizeMap, "16KiB=16");
            hrnCfgArgRawZ(argList, cfgOptRepoBlockChecksumSizeMap, "8KiB=12");
            hrnCfgEnvRawZ(cfgOptRepoCipherPass, TEST_CIPHER_PASS);
            HRN_CFG_LOAD(cfgCmdBackup, argList);

            // File that uses block incr and grows and overwrites last block of prior map
            Buffer *file = bufNew(BLOCK_MIN_FILE_SIZE * 3);
            memset(bufPtr(file), 0, bufSize(file));
            memset(bufPtr(file) + (BLOCK_MIN_SIZE * 2), 1, BLOCK_MIN_SIZE);
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-incr-grow", file, .timeModified = backupTimeStart);

            // File with age multiplier
            file = bufNew(BLOCK_MIN_FILE_SIZE * 2);
            memset(bufPtr(file), 0, bufSize(file));
            bufUsedSet(file, bufSize(file));

            HRN_STORAGE_PUT(storagePgWrite(), "block-age-multiplier", file, .timeModified = backupTimeStart - SEC_PER_DAY);

            // Run backup
            hrnBackupPqScriptP(
                PG_VERSION_11, backupTimeStart, .walCompressType = compressTypeNone, .cipherType = cipherTypeAes256Cbc,
                .cipherPass = TEST_CIPHER_PASS, .walTotal = 2);
            TEST_RESULT_VOID(hrnCmdBackup(), "backup");

            TEST_RESULT_LOG(
                "P00   INFO: last backup label = 20191108-080000F, version = " PROJECT_VERSION "\n"
                "P00   INFO: execute non-exclusive backup start: backup begins after the next regular checkpoint completes\n"
                "P00   INFO: backup start archive = 0000000105DC82D000000000, lsn = 5dc82d0/0\n"
                "P00   INFO: check archive for segment 0000000105DC82D000000000\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-age-multiplier (bundle 1/0, 32KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/global/pg_control (bundle 1/128, 8KB, [PCT]) checksum [SHA1]\n"
                "P01 DETAIL: backup file " TEST_PATH "/pg1/block-incr-grow (bundle 1/216, 48KB, [PCT]) checksum [SHA1]\n"
                "P00 DETAIL: reference pg_data/PG_VERSION to 20191108-080000F\n"
                "P00   INFO: execute non-exclusive backup stop and wait for all WAL segments to archive\n"
                "P00   INFO: backup stop archive = 0000000105DC82D000000001, lsn = 5dc82d0/300000\n"
                "P00 DETAIL: wrote 'backup_label' file returned from backup stop function\n"
                "P00 DETAIL: wrote 'tablespace_map' file returned from backup stop function\n"
                "P00   INFO: check archive for segment(s) 0000000105DC82D000000000:0000000105DC82D000000001\n"
                "P00   INFO: new backup label = 20191108-080000F_20191110-153320D\n"
                "P00   INFO: diff backup size = [SIZE], file total = 6");

            TEST_RESULT_STR_Z(
                testBackupValidateP(
                    storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/latest"), .cipherType = cipherTypeAes256Cbc,
                    .cipherPass = TEST_CIPHER_PASS),
                ". {link, d=20191108-080000F_20191110-153320D}\n"
                "bundle {path}\n"
                "bundle/1/pg_data/block-age-multiplier {file, m=1:{0,1}, s=32768}\n"
                "bundle/1/pg_data/block-incr-grow {file, m=0:{0,1},1:{0,1,2,3}, s=49152}\n"
                "bundle/1/pg_data/global/pg_control {file, s=8192}\n"
                "pg_data {path}\n"
                "pg_data/backup_label.gz {file, s=17}\n"
                "pg_data/tablespace_map.gz {file, s=19}\n"
                "--------\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"" TEST_PATH "/pg1\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"checksum\":\"17ba0791499db908433b80f37c5fbc89b870084b\",\"reference\":\"20191108-080000F\""
                ",\"size\":2,\"timestamp\":1572800000}\n"
                "pg_data/backup_label={\"checksum\":\"8e6f41ac87a7514be96260d65bacbffb11be77dc\",\"size\":17"
                ",\"timestamp\":1573400002}\n"
                "pg_data/block-age-multiplier={\"bi\":2,\"bic\":16,\"bim\":56"
                ",\"checksum\":\"5188431849b4613152fd7bdba6a3ff0a4fd6424b\",\"size\":32768,\"timestamp\":1573313600}\n"
                "pg_data/block-incr-grow={\"bi\":1,\"bim\":72,\"checksum\":\"bd4716c88f38d2540e3024b54308b0b95f34a0cc\""
                ",\"size\":49152,\"timestamp\":1573400000}\n"
                "pg_data/global/pg_control={\"size\":8192,\"timestamp\":1573400000}\n"
                "pg_data/tablespace_map={\"checksum\":\"87fe624d7976c2144e10afcb7a9a49b071f35e9c\",\"size\":19"
                ",\"timestamp\":1573400002}\n"
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/global={}\n",
                "compare file list");
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
