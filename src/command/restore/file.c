/***********************************************************************************************************************************
Restore File
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <stdio.h> // !!!
#include <unistd.h>
#include <utime.h>

#include "command/backup/blockMap.h"
#include "command/restore/deltaMap.h"
#include "command/restore/file.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
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
                                    IoWrite *const pgWriteTruncate = storageWriteIo(
                                        storageNewWriteP(
                                            storagePgWrite(), file->name, .noAtomic = true, .noCreatePath = true,
                                            .noSyncPath = true, .noTruncate = true));
                                    ioWriteOpen(pgWriteTruncate);

                                    // Truncate to original size
                                    THROW_ON_SYS_ERROR_FMT(
                                        ftruncate(ioWriteFd(pgWriteTruncate), (off_t)file->size) == -1, FileWriteError,
                                        "unable to truncate file '%s'", fileName);

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
                                        ioFilterGroupAdd(ioReadFilterGroup(read), deltaMapNew(file->blockIncrSize));

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

                                // If block incremental and not preserving the file, store the delta map for later use
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

                            // Determine how many files can be copied with one read
                            for (unsigned int fileNextIdx = fileIdx + 1; fileNextIdx < lstSize(fileList); fileNextIdx++)
                            {
                                // Only files that are being copied are considered
                                if (((const RestoreFileResult *)lstGet(result, fileNextIdx))->result == restoreResultCopy)
                                {
                                    const RestoreFile *const fileNext = lstGet(fileList, fileNextIdx);
                                    ASSERT(fileNext->limit != NULL && varUInt64(fileNext->limit) != 0);

                                    // Break if the offset is not the first file's offset + the limit of all additional files so far
                                    if (fileNext->offset != file->offset + repoFileLimit)
                                        break;

                                    repoFileLimit += varUInt64(fileNext->limit);
                                }
                                // Else if the file was not copied then there is a gap so break
                                else
                                    break;
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
                    const String *checksum = NULL;

                    if (file->blockIncrMapSize != 0)
                    {
                        ASSERT(referenceList != NULL);

                        // fprintf(stdout, "!!!ABOUT TO READ BLOCK MAP\n"); fflush(stdout);
                        // Read block map
                        const BlockMap *const blockMap = blockMapNewRead(storageReadIo(repoFileRead));
                        // fprintf(stdout, "!!!DONE TO READ BLOCK MAP\n"); fflush(stdout);

                        // Write changed blocks
                        ioWriteOpen(storageWriteIo(pgFileWrite));

                        for (unsigned int blockMapIdx = 0; blockMapIdx < blockMapSize(blockMap); blockMapIdx++)
                        {
                            const BlockMapItem *const blockMapItem = blockMapGet(blockMap, blockMapIdx);

                            if (file->deltaMap == NULL || blockMapIdx >= bufUsed(file->deltaMap) / HASH_TYPE_SHA1_SIZE ||
                                !bufEq(BUF(blockMapItem->checksum, HASH_TYPE_SHA1_SIZE), BUF(bufPtrConst(file->deltaMap) + blockMapIdx * HASH_TYPE_SHA1_SIZE, HASH_TYPE_SHA1_SIZE)))
                            {
                                // Construct repo file
                                String *const blockFile = strCatFmt(
                                    strNew(), STORAGE_REPO_BACKUP "/%s/", strZ(strLstGet(referenceList, blockMapItem->reference)));

                                if (blockMapItem->bundleId != 0)
                                    strCatFmt(blockFile, "bundle/%" PRIu64, blockMapItem->bundleId);
                                else
                                    strCatFmt(blockFile, "%s" BACKUP_BLOCK_INCR_EXT, strZ(file->manifestFile));

                                fprintf(stdout, "!!!READ %u from %s\n", blockMapIdx, strZ(blockFile)); fflush(stdout);

                                const Buffer *const block = storageGetP(
                                    storageNewReadP(storageRepo(), blockFile, .offset = blockMapItem->offset,
                                    .limit = VARUINT64(blockMapItem->size)));

                                THROW_ON_SYS_ERROR_FMT(
                                    lseek(
                                        ioWriteFd(storageWriteIo(pgFileWrite)), (off_t)(blockMapIdx * file->blockIncrSize),
                                        SEEK_SET) == -1,
                                    FileOpenError, STORAGE_ERROR_READ_SEEK, (uint64_t)(blockMapIdx * file->blockIncrSize),
                                    strZ(storagePathP(storagePg(), file->name)));

                                if (write(ioWriteFd(storageWriteIo(pgFileWrite)), bufPtrConst(block), bufUsed(block)) !=
                                    (ssize_t)bufUsed(block))
                                {
                                    THROW_SYS_ERROR_FMT(
                                        FileWriteError, "unable to write '%s'", strZ(storagePathP(storagePg(), file->name)));
                                }
                            }
                        }

                        ioWriteClose(storageWriteIo(pgFileWrite));

                        // Calculate checksum
                        IoRead *const read = storageReadIo(storageNewReadP(storagePg(), file->name));

                        ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(hashTypeSha1));
                        ioReadDrain(read);

                        checksum = bufHex(pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE)));
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
                        checksum = bufHex(pckReadBinP(ioFilterGroupResultP(filterGroup, CRYPTO_HASH_FILTER_TYPE)));
                    }

                    // If more than one file is being copied from a single read then decrement the limit
                    if (repoFileLimit != 0)
                        repoFileLimit -= varUInt64(file->limit);

                    // Free the repo file when there are no more files to copy from it
                    if (repoFileLimit == 0)
                        storageReadFree(repoFileRead);

                    // Validate checksum
                    if (!strEq(file->checksum, checksum))
                    {
                        THROW_FMT(
                            ChecksumError,
                            "error restoring '%s': actual checksum '%s' does not match expected checksum '%s'", strZ(file->name),
                            strZ(checksum), strZ(file->checksum));
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
