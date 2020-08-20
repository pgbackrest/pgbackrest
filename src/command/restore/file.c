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
#include "storage/helper.h"

/**********************************************************************************************************************************/
bool
restoreFile(
    const String *repoFile, const String *repoFileReference, CompressType repoFileCompressType, const String *pgFile,
    const String *pgFileChecksum, bool pgFileZero, uint64_t pgFileSize, time_t pgFileModified, mode_t pgFileMode,
    const String *pgFileUser, const String *pgFileGroup, time_t copyTimeBegin, bool delta, bool deltaForce,
    const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, repoFile);
        FUNCTION_LOG_PARAM(STRING, repoFileReference);
        FUNCTION_LOG_PARAM(ENUM, repoFileCompressType);
        FUNCTION_LOG_PARAM(STRING, pgFile);
        FUNCTION_LOG_PARAM(STRING, pgFileChecksum);
        FUNCTION_LOG_PARAM(BOOL, pgFileZero);
        FUNCTION_LOG_PARAM(UINT64, pgFileSize);
        FUNCTION_LOG_PARAM(TIME, pgFileModified);
        FUNCTION_LOG_PARAM(MODE, pgFileMode);
        FUNCTION_LOG_PARAM(STRING, pgFileUser);
        FUNCTION_LOG_PARAM(STRING, pgFileGroup);
        FUNCTION_LOG_PARAM(TIME, copyTimeBegin);
        FUNCTION_LOG_PARAM(BOOL, delta);
        FUNCTION_LOG_PARAM(BOOL, deltaForce);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(repoFile != NULL);
    ASSERT(repoFileReference != NULL);
    ASSERT(pgFile != NULL);

    // Was the file copied?
    bool result = true;

    // Is the file compressible during the copy?
    bool compressible = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Perform delta if requested.  Delta zero-length files to avoid overwriting the file if the timestamp is correct.
        if (delta && !pgFileZero)
        {
            // Perform delta if the file exists
            StorageInfo info = storageInfoP(storagePg(), pgFile, .ignoreMissing = true, .followLink = true);

            if (info.exists)
            {
                // If force then use size/timestamp delta
                if (deltaForce)
                {
                    // Make sure that timestamp/size are equal and that timestamp is before the copy start time of the backup
                    if (info.size == pgFileSize && info.timeModified == pgFileModified && info.timeModified < copyTimeBegin)
                        result = false;
                }
                // Else use size and checksum
                else
                {
                    // Only continue delta if the file size is as expected
                    if (info.size == pgFileSize)
                    {
                        // Generate checksum for the file if size is not zero
                        IoRead *read = NULL;

                        if (info.size != 0)
                        {
                            read = storageReadIo(storageNewReadP(storagePgWrite(), pgFile));
                            ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(HASH_TYPE_SHA1_STR));
                            ioReadDrain(read);
                        }

                        // If size and checksum are equal then no need to copy the file
                        if (pgFileSize == 0 ||
                            strEq(
                                pgFileChecksum, varStr(ioFilterGroupResult(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE_STR))))
                        {
                            // Even if hash/size are the same set the time back to backup time.  This helps with unit testing, but
                            // also presents a pristine version of the database after restore.
                            if (info.timeModified != pgFileModified)
                            {
                                THROW_ON_SYS_ERROR_FMT(
                                    utime(
                                        strZ(storagePathP(storagePg(), pgFile)),
                                        &((struct utimbuf){.actime = pgFileModified, .modtime = pgFileModified})) == -1,
                                    FileInfoError, "unable to set time for '%s'", strZ(storagePathP(storagePg(), pgFile)));
                            }

                            result = false;
                        }
                    }
                }
            }
        }

        // Copy file from repository to database or create zero-length/sparse file
        if (result)
        {
            // Create destination file
            StorageWrite *pgFileWrite = storageNewWriteP(
                storagePgWrite(), pgFile, .modeFile = pgFileMode, .user = pgFileUser, .group = pgFileGroup,
                .timeModified = pgFileModified, .noAtomic = true, .noCreatePath = true, .noSyncPath = true);

            // If size is zero/sparse no need to actually copy
            if (pgFileSize == 0 || pgFileZero)
            {
                ioWriteOpen(storageWriteIo(pgFileWrite));

                // Truncate the file to specified length (note in this case the file with grow, not shrink)
                if (pgFileZero)
                {
                    THROW_ON_SYS_ERROR_FMT(
                        ftruncate(ioWriteFd(storageWriteIo(pgFileWrite)), (off_t)pgFileSize) == -1, FileWriteError,
                        "unable to truncate '%s'", strZ(pgFile));

                    // Report the file as not copied
                    result = false;
                }

                ioWriteClose(storageWriteIo(pgFileWrite));
            }
            // Else perform the copy
            else
            {
                IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(pgFileWrite));

                // Add decryption filter
                if (cipherPass != NULL)
                {
                    ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));
                    compressible = false;
                }

                // Add decompression filter
                if (repoFileCompressType != compressTypeNone)
                {
                    ioFilterGroupAdd(filterGroup, decompressFilter(repoFileCompressType));
                    compressible = false;
                }

                // Add sha1 filter
                ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));

                // Add size filter
                ioFilterGroupAdd(filterGroup, ioSizeNew());

                // Copy file
                storageCopyP(
                    storageNewReadP(
                        storageRepo(),
                        strNewFmt(
                            STORAGE_REPO_BACKUP "/%s/%s%s", strZ(repoFileReference), strZ(repoFile),
                            strZ(compressExtStr(repoFileCompressType))),
                        .compressible = compressible),
                    pgFileWrite);

                // Validate checksum
                if (!strEq(pgFileChecksum, varStr(ioFilterGroupResult(filterGroup, CRYPTO_HASH_FILTER_TYPE_STR))))
                {
                    THROW_FMT(
                        ChecksumError,
                        "error restoring '%s': actual checksum '%s' does not match expected checksum '%s'", strZ(pgFile),
                        strZ(varStr(ioFilterGroupResult(filterGroup, CRYPTO_HASH_FILTER_TYPE_STR))), strZ(pgFileChecksum));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}
