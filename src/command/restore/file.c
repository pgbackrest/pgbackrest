/***********************************************************************************************************************************
Restore File
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <unistd.h>
#include <utime.h>

#include "command/backup/blockMap.h"
#include "command/restore/deltaMap.h"
#include "command/restore/file.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/chunkedRead.h"
#include "common/io/fdWrite.h"
#include "common/io/filter/group.h"
#include "common/io/filter/size.h"
#include "common/io/io.h"
#include "common/log.h"
#include "config/config.h"
#include "info/manifest.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
List *restoreFile(
    const String *const repoFile, const unsigned int repoIdx, const CompressType repoFileCompressType, const time_t copyTimeBegin,
    const bool delta, const bool deltaForce, const String *const cipherPass, const StringList *const referenceList,
    List *const fileList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, repoFile);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(ENUM, repoFileCompressType);
        FUNCTION_LOG_PARAM(TIME, copyTimeBegin);
        FUNCTION_LOG_PARAM(BOOL, delta);
        FUNCTION_LOG_PARAM(BOOL, deltaForce);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_LOG_PARAM(STRING_LIST, referenceList);             // List of references (for block incremental)
        FUNCTION_LOG_PARAM(LIST, fileList);                         // List of files to restore
    FUNCTION_LOG_END();

    ASSERT(repoFile != NULL);

    // Restore file results
    List *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = lstNewP(sizeof(RestoreFileResult));

        // Check files to determine which ones need to be restored
        for (unsigned int fileIdx = 0; fileIdx < lstSize(fileList); fileIdx++)
        {
            // Use a per-file mem context to reduce memory usage
            MEM_CONTEXT_TEMP_BEGIN()
            {
                RestoreFile *const file = lstGet(fileList, fileIdx);
                ASSERT(file->name != NULL);
                ASSERT(file->limit == NULL || varType(file->limit) == varTypeUInt64);

                RestoreFileResult *const fileResult = lstAdd(
                    result, &(RestoreFileResult){.manifestFile = file->manifestFile, .result = restoreResultCopy});

                // Perform delta if requested. Delta zero-length files to avoid overwriting the file if the timestamp is correct.
                if (delta && !file->zero)
                {
                    // Perform delta if the file exists
                    StorageInfo info = storageInfoP(storagePg(), file->name, .ignoreMissing = true, .followLink = true);

                    if (info.exists)
                    {
                        // If force then use size/timestamp delta
                        if (deltaForce)
                        {
                            // Make sure that timestamp/size are equal and the timestamp is before the copy start time of the backup
                            if (info.size == file->size && info.timeModified == file->timeModified &&
                                info.timeModified < copyTimeBegin)
                            {
                                fileResult->result = restoreResultPreserve;
                            }
                        }
                        // Else use size and checksum
                        else
                        {
                            // Only continue delta if the file size is as expected or larger
                            if (info.size >= file->size)
                            {
                                const char *const fileName = strZ(storagePathP(storagePg(), file->name));

                                // If the file was extended since the backup, then truncate it to the size it was during the backup
                                // as it might have only been appended to with the earlier portion being unchanged (we will verify
                                // this using the checksum below)
                                if (info.size > file->size)
                                {
                                    // Open file for write
                                    IoWrite *const pgWriteTruncate = storageWriteIo(
                                        storageNewWriteP(
                                            storagePgWrite(), file->name, .noAtomic = true, .noCreatePath = true,
                                            .noSyncPath = true, .noTruncate = true));
                                    ioWriteOpen(pgWriteTruncate);

                                    // Truncate to original size
                                    THROW_ON_SYS_ERROR_FMT(
                                        ftruncate(ioWriteFd(pgWriteTruncate), (off_t)file->size) == -1, FileWriteError,
                                        "unable to truncate file '%s'", fileName);

                                    // Close file
                                    ioWriteClose(pgWriteTruncate);

                                    // Update info
                                    info = storageInfoP(storagePg(), file->name, .followLink = true);
                                }

                                // Generate checksum for the file if size is not zero
                                IoRead *read = NULL;

                                if (file->size != 0)
                                {
                                    read = storageReadIo(storageNewReadP(storagePg(), file->name));
                                    ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(hashTypeSha1));

                                    // Generate delta map if block incremental
                                    if (file->blockIncrMapSize != 0)
                                        ioFilterGroupAdd(ioReadFilterGroup(read), deltaMapNew((size_t)file->blockIncrSize));

                                    ioReadDrain(read);
                                }

                                // If the checksum is the same (or file is zero size) then no need to copy the file
                                if (file->size == 0 ||
                                    strEq(
                                        file->checksum,
                                        bufHex(
                                            pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE)))))
                                {
                                    // If the hash/size are now the same but the time is not, then set the time back to the backup
                                    // time. This helps with unit testing, but also presents a pristine version of the database
                                    // after restore.
                                    if (info.timeModified != file->timeModified)
                                    {
                                        THROW_ON_SYS_ERROR_FMT(
                                            utime(
                                                fileName,
                                                &((struct utimbuf){
                                                    .actime = file->timeModified, .modtime = file->timeModified})) == -1,
                                            FileInfoError, "unable to set time for '%s'", fileName);
                                    }

                                    fileResult->result = restoreResultPreserve;
                                }

                                // If block incremental and not preserving the file, store the delta map for later use in
                                // reconstructing the pg file
                                if (file->blockIncrMapSize != 0 && fileResult->result != restoreResultPreserve)
                                {
                                    PackRead *const deltaMapResult = ioFilterGroupResultP(
                                        ioReadFilterGroup(read), DELTA_MAP_FILTER_TYPE);

                                    MEM_CONTEXT_OBJ_BEGIN(fileList)
                                    {
                                        file->deltaMap = pckReadBinP(deltaMapResult);
                                    }
                                    MEM_CONTEXT_OBJ_END();
                                }
                            }
                        }
                    }
                }

                // Create zeroed and zero-length files
                if (fileResult->result == restoreResultCopy && (file->size == 0 || file->zero))
                {
                    // Create destination file
                    StorageWrite *pgFileWrite = storageNewWriteP(
                        storagePgWrite(), file->name, .modeFile = file->mode, .user = file->user, .group = file->group,
                        .timeModified = file->timeModified, .noAtomic = true, .noCreatePath = true, .noSyncPath = true);

                    ioWriteOpen(storageWriteIo(pgFileWrite));

                    // Truncate the file to specified length (note in this case the file will grow, not shrink)
                    if (file->zero)
                    {
                        THROW_ON_SYS_ERROR_FMT(
                            ftruncate(ioWriteFd(storageWriteIo(pgFileWrite)), (off_t)file->size) == -1, FileWriteError,
                            "unable to truncate '%s'", strZ(file->name));
                    }

                    ioWriteClose(storageWriteIo(pgFileWrite));

                    // Report the file as zeroed or zero-length
                    fileResult->result = restoreResultZero;
                }
            }
            MEM_CONTEXT_TEMP_END();
        }

        // Copy files from repository to database
        StorageRead *repoFileRead = NULL;
        uint64_t repoFileLimit = 0;

        for (unsigned int fileIdx = 0; fileIdx < lstSize(fileList); fileIdx++)
        {
            // Use a per-file mem context to reduce memory usage
            MEM_CONTEXT_TEMP_BEGIN()
            {
                const RestoreFile *const file = lstGet(fileList, fileIdx);
                const RestoreFileResult *const fileResult = lstGet(result, fileIdx);

                // Copy file from repository to database
                if (fileResult->result == restoreResultCopy)
                {
                    // If no repo file is currently open
                    if (repoFileLimit == 0)
                    {
                        // If a limit is specified then we need to use it, even if there is only one pg file to copy, because we
                        // might be reading from the middle of a repo file containing many pg files
                        if (file->limit != NULL)
                        {
                            ASSERT(varUInt64(file->limit) != 0);
                            repoFileLimit = varUInt64(file->limit);

                            // !!! WHY THIS EXCEPTION -- ONLY NEEDED FOR REMOTE
                            if (file->blockIncrMapSize == 0)
                            {
                                // Determine how many files can be copied with one read
                                for (unsigned int fileNextIdx = fileIdx + 1; fileNextIdx < lstSize(fileList); fileNextIdx++)
                                {
                                    // Only files that are being copied are considered
                                    if (((const RestoreFileResult *)lstGet(result, fileNextIdx))->result == restoreResultCopy)
                                    {
                                        const RestoreFile *const fileNext = lstGet(fileList, fileNextIdx);
                                        ASSERT(fileNext->limit != NULL && varUInt64(fileNext->limit) != 0);

                                        // Break if the offset is not the first file's offset + limit of all additional files so far
                                        if (fileNext->offset != file->offset + repoFileLimit)
                                            break;

                                        repoFileLimit += varUInt64(fileNext->limit);
                                    }
                                    // Else if the file was not copied then there is a gap so break
                                    else
                                        break;
                                }
                            }
                        }

                        // Create and open the repo file. It needs to be created in the prior context because it will live longer
                        // than a single loop when more than one file is being read.
                        MEM_CONTEXT_PRIOR_BEGIN()
                        {
                            repoFileRead = storageNewReadP(
                                storageRepoIdx(repoIdx), repoFile,
                                .compressible = repoFileCompressType == compressTypeNone && cipherPass == NULL,
                                .offset = file->offset, .limit = repoFileLimit != 0 ? VARUINT64(repoFileLimit) : NULL);

                            // Add decryption filter for block incremental map
                            if (cipherPass != NULL && file->blockIncrMapSize != 0) // {uncovered - !!!}
                            {
                                ioFilterGroupAdd( // {uncovered - !!!}
                                    ioReadFilterGroup(storageReadIo(repoFileRead)),
                                    cipherBlockNew(
                                        cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));  // {uncovered - !!!}
                            }

                            ioReadOpen(storageReadIo(repoFileRead));
                        }
                        MEM_CONTEXT_PRIOR_END();
                    }

                    // Create pg file
                    StorageWrite *pgFileWrite = storageNewWriteP(
                        storagePgWrite(), file->name, .modeFile = file->mode, .user = file->user, .group = file->group,
                        .timeModified = file->timeModified, .noAtomic = true, .noCreatePath = true, .noSyncPath = true,
                        .noTruncate = file->deltaMap != NULL);

                    // If block incremental file
                    const Buffer *checksum = NULL;

                    if (file->blockIncrMapSize != 0)
                    {
                        ASSERT(referenceList != NULL);

                        // Read block map. This will be compared to the delta map already created to determine which blocks need to
                        // be fetched from the repository. If we got here there must be at least one block to fetch.
                        const BlockMap *const blockMap = blockMapNewRead(storageReadIo(repoFileRead));

                        // !!! EXPLAIN WHY THIS IS NEEDED
                        ioReadClose(storageReadIo(repoFileRead));
                        // !!! DECRYPT FILTER NEEDED HERE

                        // Size of delta map. If there is no delta map because the pg file does not exist then set to zero, which
                        // will force all blocks to be updated.
                        const unsigned int deltaMapSize = file->deltaMap == NULL ?
                            0 : (unsigned int)(bufUsed(file->deltaMap) / HASH_TYPE_SHA1_SIZE);

                        // Find and write updated blocks
                        bool updateFound = false;                   // Is there a block list to be updated?
                        unsigned int blockMapMinIdx = 0;            // Min block in the list
                        unsigned int blockMapMaxIdx = 0;            // Max block in the list
                        uint64_t blockListOffset = 0;               // Offset to start of block list
                        uint64_t blockListSize = 0;                 // Size of all blocks in list

                        ioWriteOpen(storageWriteIo(pgFileWrite));

                        for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(blockMap); blockMapIdx++)
                        {
                            const BlockMapItem *const blockMapItem = blockMapGet(blockMap, blockMapIdx);

                            // The block must be updated if it beyond the blocks that exist in the delta map or when the checksum
                            // stored in the repository is different from the delta map
                            if (blockMapIdx >= deltaMapSize ||
                                !bufEq(
                                    BUF(blockMapItem->checksum, HASH_TYPE_SHA1_SIZE),
                                    BUF(bufPtrConst(file->deltaMap) + blockMapIdx * HASH_TYPE_SHA1_SIZE, HASH_TYPE_SHA1_SIZE)))
                            {
                                // If no block list is currently being built then start a new one
                                if (!updateFound)
                                {
                                    updateFound = true;
                                    blockMapMinIdx = blockMapIdx;
                                    blockMapMaxIdx = blockMapIdx;
                                    blockListOffset = blockMapItem->offset;
                                    blockListSize = blockMapItem->size;
                                }
                                // Else add to the current block list
                                else
                                {
                                    blockMapMaxIdx = blockMapIdx;
                                    blockListSize += blockMapItem->size;
                                }

                                // Check if the next block should be part of this list. If so, continue so the block will be added
                                // to the list on the next iteration. Otherwise, write out the current block list below.
                                if (blockMapIdx < blockMapSize(blockMap) - 1)
                                {
                                    const BlockMapItem *const blockMapItemNext = blockMapGet(blockMap, blockMapIdx + 1);

                                    // Similar to the check above, but also make sure the reference is the same. For blocks to be
                                    // in a common list they must be contiguous and from the same reference.
                                    if (blockMapItem->reference == blockMapItemNext->reference &&
                                        (blockMapIdx + 1 >= deltaMapSize ||
                                         !bufEq(
                                             BUF(blockMapItemNext->checksum, HASH_TYPE_SHA1_SIZE),
                                             BUF(
                                                bufPtrConst(file->deltaMap) + (blockMapIdx + 1) * HASH_TYPE_SHA1_SIZE,
                                                HASH_TYPE_SHA1_SIZE))))
                                    {
                                        continue;
                                    }
                                }
                            }

                            // Update blocks in the list when found
                            if (updateFound)
                            {
                                // Use a per-block-list mem context to reduce memory usage
                                MEM_CONTEXT_TEMP_BEGIN()
                                {
                                    // Seek to the min block offset. It is possible we are already at the correct position but it
                                    // is easier and safer to let lseek() figure this out.
                                    THROW_ON_SYS_ERROR_FMT(
                                        lseek(
                                            ioWriteFd(storageWriteIo(pgFileWrite)), (off_t)(blockMapMinIdx * file->blockIncrSize),
                                            SEEK_SET) == -1,
                                        FileOpenError, STORAGE_ERROR_READ_SEEK, (uint64_t)(blockMapMinIdx * file->blockIncrSize),
                                        strZ(storagePathP(storagePg(), file->name)));

                                    // Open the block list for read. Using one read for all blocks is cheaper than reading from the
                                    // file multiple times, which is especially noticeable on object stores. Use the last block in
                                    // the list to construct the name of the repo file where the blocks are stored since it is
                                    // available and must have the same reference and bundle id as the other blocks.
                                    StorageRead *const blockRead = storageNewReadP(
                                        storageRepo(),
                                        backupFileRepoPathP(
                                            strLstGet(referenceList, blockMapItem->reference), .manifestName = file->manifestFile,
                                            .bundleId = blockMapItem->bundleId, .blockIncr = true),
                                        .offset = blockListOffset, .limit = VARUINT64(blockListSize));
                                    ioReadOpen(storageReadIo(blockRead));

                                    for (unsigned int blockMapIdx = blockMapMinIdx; blockMapIdx <= blockMapMaxIdx; blockMapIdx++)
                                    {
                                        // Use a per-block mem context to reduce memory usage
                                        MEM_CONTEXT_TEMP_BEGIN()
                                        {
                                            // !!!
                                            IoRead *const chunkedRead = ioChunkedReadNew(storageReadIo(blockRead));

                                            // Add decryption filter
                                            if (cipherPass != NULL) // {uncovered - !!!}
                                            {
                                                ioFilterGroupAdd( // {uncovered - !!!}
                                                    ioReadFilterGroup(chunkedRead),
                                                    cipherBlockNew(
                                                        cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));  // {uncovered - !!!}
                                            }

                                            // Add decompression filter
                                            if (repoFileCompressType != compressTypeNone) // {uncovered - !!!}
                                            {
                                                ioFilterGroupAdd( // {uncovered - !!!}
                                                    ioReadFilterGroup(chunkedRead), decompressFilter(repoFileCompressType));
                                            }

                                            // !!!
                                            ioReadOpen(chunkedRead);

                                            // Read and discard the block no since we already know it
                                            ioReadVarIntU64(chunkedRead);

                                            // Copy chunked block
                                            ioCopyP(chunkedRead, storageWriteIo(pgFileWrite));

                                            // Flush writes since !!!
                                            ioWriteFlush(storageWriteIo(pgFileWrite));
                                        }
                                        MEM_CONTEXT_TEMP_END();
                                    }
                                }
                                MEM_CONTEXT_TEMP_END();

                                updateFound = false;
                            }
                        }

                        // Close the file to complete the update
                        ioWriteClose(storageWriteIo(pgFileWrite));

                        // Calculate checksum. In theory this is not needed because the file should always be reconstructed
                        // correctly. However, it seems better to check and the pages should still be buffered making the operation
                        // very fast.
                        IoRead *const read = storageReadIo(storageNewReadP(storagePg(), file->name));

                        ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(hashTypeSha1));
                        ioReadDrain(read);

                        checksum = pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE));
                    }
                    // Else normal file
                    else
                    {
                        IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(pgFileWrite));

                        // Add decryption filter
                        if (cipherPass != NULL)
                        {
                            ioFilterGroupAdd(
                                filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));
                        }

                        // Add decompression filter
                        if (repoFileCompressType != compressTypeNone)
                            ioFilterGroupAdd(filterGroup, decompressFilter(repoFileCompressType));

                        // Add sha1 filter
                        ioFilterGroupAdd(filterGroup, cryptoHashNew(hashTypeSha1));

                        // Add size filter
                        ioFilterGroupAdd(filterGroup, ioSizeNew());

                        // Copy file
                        ioWriteOpen(storageWriteIo(pgFileWrite));
                        ioCopyP(storageReadIo(repoFileRead), storageWriteIo(pgFileWrite), .limit = file->limit);
                        ioWriteClose(storageWriteIo(pgFileWrite));

                        // Get checksum result
                        checksum = pckReadBinP(ioFilterGroupResultP(filterGroup, CRYPTO_HASH_FILTER_TYPE));
                    }

                    // If more than one file is being copied from a single read then decrement the limit
                    if (repoFileLimit != 0)
                        repoFileLimit -= varUInt64(file->limit);

                    // Free the repo file when there are no more files to copy from it
                    if (repoFileLimit == 0)
                        storageReadFree(repoFileRead);

                    // Validate checksum
                    if (!strEq(file->checksum, bufHex(checksum)))
                    {
                        THROW_FMT(
                            ChecksumError,
                            "error restoring '%s': actual checksum '%s' does not match expected checksum '%s'", strZ(file->name),
                            strZ(bufHex(checksum)), strZ(file->checksum));
                    }
                }
            }
            MEM_CONTEXT_TEMP_END();
        }

        lstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(LIST, result);
}
