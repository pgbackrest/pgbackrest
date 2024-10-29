/***********************************************************************************************************************************
Manifest Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "command/backup/blockMap.h"
#include "command/manifest/manifest.h"
#include "command/restore/blockChecksum.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/fdWrite.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/type/json.h"
#include "config/config.h"
#include "info/manifest.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Block map render
***********************************************************************************************************************************/
typedef struct ManifestBlockDeltaReference
{
    unsigned int reference;                                         // Reference
    List *blockList;                                                // List of blocks in the block map for the reference
} ManifestBlockDeltaReference;

// Reads that must be performed in order to extract blocks
typedef struct ManifestBlockDeltaRead
{
    unsigned int reference;                                         // Reference to read from
    uint64_t bundleId;                                              // Bundle to read from
    uint64_t offset;                                                // Offset to begin read from
    uint64_t size;                                                  // Size of the read
    List *superBlockList;                                           // Super block list
} ManifestBlockDeltaRead;

// Super blocks to be extracted from the read
typedef struct ManifestBlockDeltaSuperBlock
{
    uint64_t superBlockSize;                                        // Super block size
    uint64_t size;                                                  // Stored super block size (with compression, etc.)
    List *blockList;                                                // Block list
} ManifestBlockDeltaSuperBlock;

// Blocks to be extracted from the super block
typedef struct ManifestBlockDeltaBlock
{
    uint64_t no;                                                    // Block number in the super block
    uint64_t offset;                                                // Offset into original file
    unsigned char checksum[XX_HASH_SIZE_MAX];                       // Checksum of the block
} ManifestBlockDeltaBlock;

static List *
cmdManifestBlockDelta(
    const BlockMap *const blockMap, const size_t blockSize, const size_t checksumSize, const Buffer *const blockChecksum)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_MAP, blockMap);
        FUNCTION_TEST_PARAM(SIZE, blockSize);
        FUNCTION_TEST_PARAM(SIZE, checksumSize);
        FUNCTION_TEST_PARAM(BUFFER, blockChecksum);
    FUNCTION_TEST_END();

    ASSERT(blockMap != NULL);
    ASSERT(blockSize > 0);

    List *const result = lstNewP(sizeof(ManifestBlockDeltaRead));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build list of references and for each reference the list of blocks for that reference
        const unsigned int blockChecksumSize =
            blockChecksum == NULL ? 0 : (unsigned int)(bufUsed(blockChecksum) / checksumSize);
        List *const referenceList = lstNewP(sizeof(ManifestBlockDeltaReference), .comparator = lstComparatorUInt);

        for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(blockMap); blockMapIdx++)
        {
            const BlockMapItem *const blockMapItem = blockMapGet(blockMap, blockMapIdx);

            // The block must be updated if it is beyond the blocks that exist in the block checksum list or when the checksum
            // stored in the repository is different from the block checksum list
            if (blockMapIdx >= blockChecksumSize ||
                !bufEq(
                    BUF(blockMapItem->checksum, checksumSize),
                    BUF(bufPtrConst(blockChecksum) + blockMapIdx * checksumSize, checksumSize)))
            {
                const unsigned int reference = blockMapItem->reference;
                ManifestBlockDeltaReference *const referenceData = lstFind(referenceList, &reference);

                // If the reference has not been added
                if (referenceData == NULL)
                {
                    ManifestBlockDeltaReference *referenceData = lstAdd(
                        referenceList,
                        &(ManifestBlockDeltaReference){.reference = reference, .blockList = lstNewP(sizeof(unsigned int))});
                    lstAdd(referenceData->blockList, &blockMapIdx);
                }
                // Else add the new block
                else
                    lstAdd(referenceData->blockList, &blockMapIdx);
            }
        }

        // Sort the reference list ascending. This is an arbitrary choice as the order does not matter.
        lstSort(referenceList, sortOrderAsc);

        // Build delta
        for (unsigned int referenceIdx = 0; referenceIdx < lstSize(referenceList); referenceIdx++)
        {
            const ManifestBlockDeltaReference *const referenceData = (const ManifestBlockDeltaReference *)lstGet(
                referenceList, referenceIdx);
            ManifestBlockDeltaRead *blockDeltaRead = NULL;
            ManifestBlockDeltaSuperBlock *blockDeltaSuperBlock = NULL;
            const BlockMapItem *blockMapItemPrior = NULL;

            for (unsigned int blockIdx = 0; blockIdx < lstSize(referenceData->blockList); blockIdx++)
            {
                const unsigned int blockMapIdx = *(unsigned int *)lstGet(referenceData->blockList, blockIdx);
                const BlockMapItem *const blockMapItem = blockMapGet(blockMap, blockMapIdx);

                // Add read when it has changed
                if (blockMapItemPrior == NULL ||
                    (blockMapItemPrior->offset != blockMapItem->offset &&
                     blockMapItemPrior->offset + blockMapItemPrior->size != blockMapItem->offset))
                {
                    MEM_CONTEXT_OBJ_BEGIN(result)
                    {
                        ManifestBlockDeltaRead blockDeltaReadNew =
                        {
                            .reference = blockMapItem->reference,
                            .bundleId = blockMapItem->bundleId,
                            .offset = blockMapItem->offset,
                            .superBlockList = lstNewP(sizeof(ManifestBlockDeltaSuperBlock)),
                        };

                        blockDeltaRead = lstAdd(result, &blockDeltaReadNew);
                    }
                    MEM_CONTEXT_OBJ_END();
                }

                // Add super block when it has changed
                if (blockMapItemPrior == NULL || blockMapItemPrior->offset != blockMapItem->offset)
                {
                    MEM_CONTEXT_OBJ_BEGIN(blockDeltaRead->superBlockList)
                    {
                        ManifestBlockDeltaSuperBlock blockDeltaSuperBlockNew =
                        {
                            .superBlockSize = blockMapItem->superBlockSize,
                            .size = blockMapItem->size,
                            .blockList = lstNewP(sizeof(ManifestBlockDeltaBlock)),
                        };

                        blockDeltaSuperBlock = lstAdd(blockDeltaRead->superBlockList, &blockDeltaSuperBlockNew);
                        blockDeltaRead->size += blockMapItem->size;
                    }
                    MEM_CONTEXT_OBJ_END();
                }

                // Add block
                ManifestBlockDeltaBlock blockDeltaBlockNew =
                {
                    .no = blockMapItem->block,
                    .offset = blockMapIdx * blockSize,
                };

                memcpy(
                    blockDeltaBlockNew.checksum, blockMapItem->checksum, SIZE_OF_STRUCT_MEMBER(ManifestBlockDeltaBlock, checksum));
                lstAdd(blockDeltaSuperBlock->blockList, &blockDeltaBlockNew);

                // Set prior item for comparison on the next loop
                blockMapItemPrior = blockMapItem;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(LIST, result);
}

/***********************************************************************************************************************************
Block map render
***********************************************************************************************************************************/
static String *
cmdManifestBlockDeltaRender(const Manifest *const manifest, const ManifestFile *const file, const bool json)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(MANIFEST_FILE, file);
        FUNCTION_LOG_PARAM(BOOL, json);
    FUNCTION_LOG_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Load block checksums for the file if the pg option is explicitly set
        const Buffer *blockChecksum = NULL;
        const Buffer *checksum = NULL;

        if (cfgOptionSource(cfgOptPg) != cfgSourceDefault)
        {
            IoRead *const read = storageReadIo(storageNewReadP(storagePg(), manifestPathPg(file->name)));
            ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(hashTypeSha1));
            ioFilterGroupAdd(ioReadFilterGroup(read), blockChecksumNew(file->blockIncrSize, file->blockIncrChecksumSize));
            ioReadDrain(read);

            checksum = pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE));
            blockChecksum = pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(read), BLOCK_CHECKSUM_FILTER_TYPE));
        }

        // If the file is up-to-date
        if (checksum != NULL && bufEq(checksum, BUF(file->checksumSha1, HASH_TYPE_SHA1_SIZE)))
        {
            if (json)
                strCatZ(result, "null");
            else
                strCatZ(result, " file is up-to-date\n");
        }
        // Else render the block delta
        else
        {
            StorageRead *const read = storageNewReadP(
                storageRepo(),
                backupFileRepoPathP(
                    file->reference != NULL ? file->reference : manifestData(manifest)->backupLabel, .manifestName = file->name,
                    .bundleId = file->bundleId, .compressType = manifestData(manifest)->backupOptionCompressType,
                    .blockIncr = true),
                .offset = file->bundleOffset + file->sizeRepo - file->blockIncrMapSize,
                .limit = VARUINT64(file->blockIncrMapSize));

            if (manifestCipherSubPass(manifest) != NULL)
            {
                ioFilterGroupAdd(
                    ioReadFilterGroup(storageReadIo(read)),
                    cipherBlockNewP(
                        cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(manifestCipherSubPass(manifest)), .raw = true));
            }

            ioReadOpen(storageReadIo(read));

            List *const blockDelta = cmdManifestBlockDelta(
                blockMapNewRead(storageReadIo(read), file->blockIncrSize, file->blockIncrChecksumSize), file->blockIncrSize,
                file->blockIncrChecksumSize, blockChecksum);

            unsigned int referenceRead = 0;
            uint64_t referenceReadSize = 0;
            unsigned int referenceSuperBlock = 0;
            uint64_t referenceSuperBlockSize = 0;
            unsigned int referenceBlock = 0;

            unsigned int totalRead = 0;
            uint64_t totalReadSize = 0;
            unsigned int totalSuperBlock = 0;
            uint64_t totalSuperBlockSize = 0;
            unsigned int totalBlock = 0;

            bool first = true;

            if (json)
                strCatChr(result, '[');
            else
                strCatChr(result, '\n');

            for (unsigned int readIdx = 0; readIdx < lstSize(blockDelta); readIdx++)
            {
                const ManifestBlockDeltaRead *const read = lstGet(blockDelta, readIdx);

                referenceRead++;
                referenceReadSize += read->size;
                referenceSuperBlock += lstSize(read->superBlockList);

                for (unsigned int superBlockIdx = 0; superBlockIdx < lstSize(read->superBlockList); superBlockIdx++)
                {
                    const ManifestBlockDeltaSuperBlock *const superBlock = lstGet(read->superBlockList, superBlockIdx);

                    referenceSuperBlockSize += superBlock->superBlockSize;
                    referenceBlock += lstSize(superBlock->blockList);
                }

                if (readIdx == lstSize(blockDelta) - 1 ||
                    read->reference != ((const ManifestBlockDeltaRead *)lstGet(blockDelta, readIdx + 1))->reference)
                {
                    if (json)
                    {
                        if (first)
                            first = false;
                        else
                            strCatChr(result, ',');

                        strCatFmt(
                            result,
                            "{\"reference\":%u,\"read\":{\"total\":%u,\"size\":%" PRIu64 "},\"superBlock\":{\"total\":%u"
                            ",\"size\":%" PRIu64 "},\"block\":{\"total\":%u}}",
                            read->reference, referenceRead, referenceReadSize, referenceSuperBlock, referenceSuperBlockSize,
                            referenceBlock);
                    }
                    else
                    {
                        const String *const reference = strSub(
                            backupFileRepoPathP(
                                strLstGet(manifestReferenceList(manifest), read->reference), .manifestName = file->name,
                                .bundleId = read->bundleId,
                                .compressType = manifestData(manifest)->backupOptionCompressType, .blockIncr = true),
                            sizeof(STORAGE_REPO_BACKUP));

                        strCatFmt(
                            result, "        reference: %s, read: %u/%s, superBlock: %u/%s, block: %u/%s\n",
                            strZ(reference), referenceRead, strZ(strSizeFormat(referenceReadSize)), referenceSuperBlock,
                            strZ(strSizeFormat(referenceSuperBlockSize)), referenceBlock,
                            strZ(strSizeFormat(referenceBlock * file->blockIncrSize)));
                    }

                    totalRead += referenceRead;
                    totalReadSize += referenceReadSize;
                    totalSuperBlock += referenceSuperBlock;
                    totalSuperBlockSize += referenceSuperBlockSize;
                    totalBlock += referenceBlock;

                    referenceRead = 0;
                    referenceReadSize = 0;
                    referenceSuperBlock = 0;
                    referenceSuperBlockSize = 0;
                    referenceBlock = 0;
                }
            }

            if (json)
                strCatChr(result, ']');
            else
            {
                strCatFmt(
                    result, "        total read: %u/%s, superBlock: %u/%s, block: %u/%s\n",
                    totalRead, strZ(strSizeFormat(totalReadSize)), totalSuperBlock, strZ(strSizeFormat(totalSuperBlockSize)),
                    totalBlock, strZ(strSizeFormat(totalBlock * file->blockIncrSize)));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Manifest file render
***********************************************************************************************************************************/
static String *
cmdManifestFileRender(const Manifest *const manifest, const ManifestFile *const file, const bool blockDelta, const bool json)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(MANIFEST_FILE, file);
        FUNCTION_LOG_PARAM(BOOL, blockDelta);
        FUNCTION_LOG_PARAM(BOOL, json);
    FUNCTION_LOG_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (json)
        {
            strCatFmt(result, "{\"name\":%s", strZ(jsonFromVar(VARSTR(file->name))));

            if (file->reference != NULL)
                strCatFmt(result, ",\"reference\":%s", strZ(jsonFromVar(VARSTR(file->reference))));

            strCatFmt(result, ",\"size\":%" PRIu64, file->size);
            strCatFmt(
                result, ",\"checksum\":\"%s\"", strZ(strNewEncode(encodingHex, BUF(file->checksumSha1, HASH_TYPE_SHA1_SIZE))));
            strCatFmt(result, ",\"repo\":{\"size\":%" PRIu64 "}", file->sizeRepo);

            if (file->bundleId != 0)
                strCatFmt(result, ",\"bundle\":{\"id\":%" PRIu64 ",\"offset\":%" PRIu64 "}", file->bundleId, file->bundleOffset);

            if (file->blockIncrSize != 0)
            {
                strCatFmt(
                    result, ",\"block\":{\"size\":%zu,\"map\":{\"size\":%" PRIu64, file->blockIncrSize, file->blockIncrMapSize);

                if (blockDelta)
                    strCatFmt(result, ",\"delta\":%s", strZ(cmdManifestBlockDeltaRender(manifest, file, json)));

                strCatFmt(result, "},\"checksum\":{\"size\":%zu}}", file->blockIncrChecksumSize);
            }

            strCatChr(result, '}');
        }
        else
        {
            strCatFmt(result, "  - %s\n", strZ(file->name));

            if (file->reference != NULL)
                strCatFmt(result, "      reference: %s\n", strZ(file->reference));

            strCatFmt(
                result, "      size: %s, repo %s\n", strZ(strSizeFormat(file->size)), strZ(strSizeFormat(file->sizeRepo)));
            strCatFmt(result, "      checksum: %s\n", strZ(strNewEncode(encodingHex, BUF(file->checksumSha1, HASH_TYPE_SHA1_SIZE))));

            if (file->bundleId != 0)
                strCatFmt(result, "      bundle: %" PRIu64 "\n", file->bundleId);

            if (file->blockIncrSize != 0)
            {
                strCatFmt(
                    result, "      block: size %s, map size %s, checksum size %s\n",
                    strZ(strSizeFormat(file->blockIncrSize)), strZ(strSizeFormat(file->blockIncrMapSize)),
                    strZ(strSizeFormat(file->blockIncrChecksumSize)));

                if (blockDelta)
                {
                    strCatZ(result, "      block delta:");
                    strCat(result, cmdManifestBlockDeltaRender(manifest, file, json));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Manifest render
***********************************************************************************************************************************/
static String *
cmdManifestRender(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    String *const result = strNew();

    // Set dry run to make sure this command never writes anything
    storageHelperDryRunInit(true);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Load backup.info and cipher
        const InfoBackup *const infoBackup = infoBackupLoadFile(
            storageRepo(), INFO_BACKUP_PATH_FILE_STR, cfgOptionStrId(cfgOptRepoCipherType),
            cfgOptionStrNull(cfgOptRepoCipherPass));
        const String *const cipherPass = infoPgCipherPass(infoBackupPg(infoBackup));
        const CipherType cipherType = cipherPass == NULL ? cipherTypeNone : cipherTypeAes256Cbc;

        // Load manifest
        const Manifest *const manifest = manifestLoadFile(
            storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(cfgOptionStr(cfgOptSet))),
            cipherType, cipherPass);

        // Manifest info
        const ManifestData *const data = manifestData(manifest);
        const bool json = cfgOptionStrId(cfgOptOutput) == CFGOPTVAL_OUTPUT_JSON;

        if (json)
        {
            strCatFmt(result, "{\"label\":\"%s\"", strZ(data->backupLabel));
            strCatFmt(result, ",\"reference\":[%s]", strZ(strLstJoinQuote(manifestReferenceList(manifest), ",", "\"")));
            strCatFmt(result, ",\"type\":\"%s\"", strZ(strIdToStr(data->backupType)));
            strCatFmt(
                result, ",\"time\":{\"start\":%" PRId64 ",\"copy\":%" PRId64 ",\"stop\":%" PRId64 "}",
                (int64_t)data->backupTimestampStart, (int64_t)data->backupTimestampCopyStart, (int64_t)data->backupTimestampStop);
            strCatFmt(
                result, ",\"bundle\":{\"bundle\":%s,\"raw\":%s}", cvtBoolToConstZ(data->bundle), cvtBoolToConstZ(data->bundleRaw));
            strCatFmt(result, ",\"block\":{\"block\":%s}", cvtBoolToConstZ(data->blockIncr));
        }
        else
        {
            strCatFmt(result, "label: %s\n", strZ(data->backupLabel));
            strCatFmt(result, "reference: %s\n", strZ(strLstJoin(manifestReferenceList(manifest), ", ")));
            strCatFmt(result, "type: %s\n", strZ(strIdToStr(data->backupType)));

            int64_t duration = (int64_t)(data->backupTimestampStop - data->backupTimestampStart);

            strCatFmt(
                result, "time: start: %s, stop: %s, duration: %" PRId64 ":%02" PRId64 ":%02" PRId64 "\n",
                strZ(strNewTimeP("%Y-%m-%d %H:%M:%S", data->backupTimestampStart)),
                strZ(strNewTimeP("%Y-%m-%d %H:%M:%S", data->backupTimestampStop)), duration / 3600, duration % 3600 / 60,
                duration % 60);

            strCatFmt(result, "bundle: %s\n", cvtBoolToConstZ(data->bundle));
            strCatFmt(result, "block: %s\n", cvtBoolToConstZ(data->blockIncr));

            strCatChr(result, '\n');
        }

        // File list
        if (json)
            strCatZ(result, ",\"fileList\":[");
        else
            strCatZ(result, "file list:\n");

        const String *const filter = cfgOptionStrNull(cfgOptFilter);

        // Single file
        if (filter != NULL)
        {
            const ManifestFile file = manifestFileFind(manifest, filter);

            strCat(result, cmdManifestFileRender(manifest, &file, true, json));
        }
        // Multiple files
        else
        {
            bool first = true;

            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
            {
                const ManifestFile file = manifestFile(manifest, fileIdx);

                // Filter on reference
                if (cfgOptionTest(cfgOptReference))
                {
                    if (strEqZ(cfgOptionStr(cfgOptReference), "latest"))
                    {
                        if (file.reference != NULL)
                            continue;
                    }
                    else if (!strEq(cfgOptionStr(cfgOptReference), file.reference))
                        continue;
                }

                if (!first)
                {
                    if (json)
                        strCatChr(result, ',');
                    else
                        strCatChr(result, '\n');
                }
                else
                    first = false;

                strCat(result, cmdManifestFileRender(manifest, &file, false, json));
            }
        }

        if (json)
            strCatZ(result, "]}");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdManifest(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ioFdWriteOneStr(STDOUT_FILENO, cmdManifestRender());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
