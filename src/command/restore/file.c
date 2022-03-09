/***********************************************************************************************************************************
Restore File
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <unistd.h>
#include <utime.h>

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
    const bool delta, const bool deltaForce, const String *const cipherPass, const List *const fileList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, repoFile);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(ENUM, repoFileCompressType);
        FUNCTION_LOG_PARAM(TIME, copyTimeBegin);
        FUNCTION_LOG_PARAM(BOOL, delta);
        FUNCTION_LOG_PARAM(BOOL, deltaForce);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
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
            const RestoreFile *const file = lstGet(fileList, fileIdx);
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
                        // Make sure that timestamp/size are equal and that timestamp is before the copy start time of the backup
                        if (info.size == file->size && info.timeModified == file->timeModified && info.timeModified < copyTimeBegin)
                            fileResult->result = restoreResultPreserve;
                    }
                    // Else use size and checksum
                    else
                    {
                        // Only continue delta if the file size is as expected
                        if (info.size == file->size)
                        {
                            // Generate checksum for the file if size is not zero
                            IoRead *read = NULL;

                            if (info.size != 0)
                            {
                                read = storageReadIo(storageNewReadP(storagePgWrite(), file->name));
                                ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(HASH_TYPE_SHA1_STR));
                                ioReadDrain(read);
                            }

                            // If the checksum is also equal then no need to copy the file
                            if (file->size == 0 ||
                                strEq(
                                    file->checksum,
                                    pckReadStrP(ioFilterGroupResultP(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE))))
                            {
                                // Even if hash/size are the same set the time back to backup time. This helps with unit testing,
                                // but also presents a pristine version of the database after restore.
                                if (info.timeModified != file->timeModified)
                                {
                                    THROW_ON_SYS_ERROR_FMT(
                                        utime(
                                            strZ(storagePathP(storagePg(), file->name)),
                                            &((struct utimbuf){.actime = file->timeModified, .modtime = file->timeModified})) == -1,
                                        FileInfoError, "unable to set time for '%s'", strZ(storagePathP(storagePg(), file->name)));
                                }

                                fileResult->result = restoreResultPreserve;
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

        // Copy files from repository to database
        StorageRead *repoFileRead = NULL;
        uint64_t repoFileLimit = 0;

        for (unsigned int fileIdx = 0; fileIdx < lstSize(fileList); fileIdx++)
        {
            const RestoreFile *const file = lstGet(fileList, fileIdx);
            const RestoreFileResult *const fileResult = lstGet(result, fileIdx);

            // Copy file from repository to database
            if (fileResult->result == restoreResultCopy)
            {
                // If no repo file is currently open
                if (repoFileLimit == 0)
                {
                    // If a limit is specified then we need to use it, even if there is only one pg file to copy, because we might
                    // be reading from the middle of a repo file containing many pg files
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

                    // Create and open the repo file
                    repoFileRead =  storageNewReadP(
                        storageRepoIdx(repoIdx), repoFile,
                        .compressible = repoFileCompressType == compressTypeNone && cipherPass == NULL, .offset = file->offset,
                        .limit = repoFileLimit != 0 ? VARUINT64(repoFileLimit) : NULL);
                    ioReadOpen(storageReadIo(repoFileRead));
                }

                // Create pg file
                StorageWrite *pgFileWrite = storageNewWriteP(
                    storagePgWrite(), file->name, .modeFile = file->mode, .user = file->user, .group = file->group,
                    .timeModified = file->timeModified, .noAtomic = true, .noCreatePath = true, .noSyncPath = true);

                IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(pgFileWrite));

                // Add decryption filter
                if (cipherPass != NULL)
                    ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));

                // Add decompression filter
                if (repoFileCompressType != compressTypeNone)
                    ioFilterGroupAdd(filterGroup, decompressFilter(repoFileCompressType));

                // Add sha1 filter
                ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));

                // Add size filter
                ioFilterGroupAdd(filterGroup, ioSizeNew());

                // Copy file
                ioWriteOpen(storageWriteIo(pgFileWrite));
                ioCopyP(storageReadIo(repoFileRead), storageWriteIo(pgFileWrite), .limit = file->limit);
                ioWriteClose(storageWriteIo(pgFileWrite));

                // If more than one file is being copied from a single read then decrement the limit
                if (repoFileLimit != 0)
                    repoFileLimit -= varUInt64(file->limit);

                // Free the repo file when there are no more files to copy from it
                if (repoFileLimit == 0)
                    storageReadFree(repoFileRead);

                // Validate checksum
                if (!strEq(file->checksum, pckReadStrP(ioFilterGroupResultP(filterGroup, CRYPTO_HASH_FILTER_TYPE))))
                {
                    THROW_FMT(
                        ChecksumError,
                        "error restoring '%s': actual checksum '%s' does not match expected checksum '%s'", strZ(file->name),
                        strZ(pckReadStrP(ioFilterGroupResultP(filterGroup, CRYPTO_HASH_FILTER_TYPE))), strZ(file->checksum));
                }

                // Free the pg file
                storageWriteFree(pgFileWrite);
            }
        }

        lstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(LIST, result);
}
