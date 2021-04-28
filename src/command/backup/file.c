/***********************************************************************************************************************************
Backup File
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "command/backup/file.h"
#include "command/backup/pageChecksum.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/filter/group.h"
#include "common/io/filter/size.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/type/convert.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
static unsigned int
segmentNumber(const String *pgFile)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, pgFile);
    FUNCTION_TEST_END();

    // Determine which segment number this is by checking for a numeric extension.  No extension means segment 0.
    FUNCTION_TEST_RETURN(regExpMatchOne(STRDEF("\\.[0-9]+$"), pgFile) ? cvtZToUInt(strrchr(strZ(pgFile), '.') + 1) : 0);
}

/**********************************************************************************************************************************/
BackupFileResult
backupFile(
    const String *pgFile, bool pgFileIgnoreMissing, uint64_t pgFileSize, bool pgFileCopyExactSize, const String *pgFileChecksum,
    bool pgFileChecksumPage, uint64_t pgFileChecksumPageLsnLimit, const String *repoFile, bool repoFileHasReference,
    CompressType repoFileCompressType, int repoFileCompressLevel, const String *backupLabel, bool delta, CipherType cipherType,
    const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pgFile);                         // Database file to copy to the repo
        FUNCTION_LOG_PARAM(BOOL, pgFileIgnoreMissing);              // Is it OK if the database file is missing?
        FUNCTION_LOG_PARAM(UINT64, pgFileSize);                     // Size of the database file
        FUNCTION_LOG_PARAM(BOOL, pgFileCopyExactSize);              // Copy only pgFileSize bytes even if the file has grown
        FUNCTION_LOG_PARAM(STRING, pgFileChecksum);                 // Checksum to verify the database file
        FUNCTION_LOG_PARAM(BOOL, pgFileChecksumPage);               // Should page checksums be validated
        FUNCTION_LOG_PARAM(UINT64, pgFileChecksumPageLsnLimit);     // Upper LSN limit to which page checksums must be valid
        FUNCTION_LOG_PARAM(STRING, repoFile);                       // Destination in the repo to copy the pg file
        FUNCTION_LOG_PARAM(BOOL, repoFileHasReference);             // Does the repo file exist in a prior backup in the set?
        FUNCTION_LOG_PARAM(ENUM, repoFileCompressType);             // Compress type for repo file
        FUNCTION_LOG_PARAM(INT,  repoFileCompressLevel);            // Compression level for repo file
        FUNCTION_LOG_PARAM(STRING, backupLabel);                    // Label of current backup
        FUNCTION_LOG_PARAM(BOOL, delta);                            // Is the delta option on?
        FUNCTION_LOG_PARAM(STRING_ID, cipherType);                  // Encryption type
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Password to access the repo file if encrypted
    FUNCTION_LOG_END();

    ASSERT(pgFile != NULL);
    ASSERT(repoFile != NULL);
    ASSERT(backupLabel != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    // Backup file results
    BackupFileResult result = {.backupCopyResult = backupCopyResultCopy};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Generate complete repo path and add compression extension if needed
        const String *repoPathFile = strNewFmt(
            STORAGE_REPO_BACKUP "/%s/%s%s", strZ(backupLabel), strZ(repoFile), strZ(compressExtStr(repoFileCompressType)));

        // If checksum is defined then the file needs to be checked. If delta option then check the DB and possibly the repo, else
        // just check the repo.
        if (pgFileChecksum != NULL)
        {
            // Does the file in pg match the checksum and size passed?
            bool pgFileMatch = false;

            // If delta, then check the DB checksum and possibly the repo. If the checksum does not match in either case then
            // recopy.
            if (delta)
            {
                // Generate checksum/size for the pg file. Only read as many bytes as passed in pgFileSize.  If the file has grown
                // since the manifest was built we don't need to consider the extra bytes since they will be replayed from WAL
                // during recovery.
                IoRead *read = storageReadIo(
                    storageNewReadP(
                        storagePg(), pgFile, .ignoreMissing = pgFileIgnoreMissing,
                        .limit = pgFileCopyExactSize ? VARUINT64(pgFileSize) : NULL));
                ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(HASH_TYPE_SHA1_STR));
                ioFilterGroupAdd(ioReadFilterGroup(read), ioSizeNew());

                // If the pg file exists check the checksum/size
                if (ioReadDrain(read))
                {
                    const String *pgTestChecksum = varStr(
                        ioFilterGroupResult(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE_STR));
                    uint64_t pgTestSize = varUInt64Force(ioFilterGroupResult(ioReadFilterGroup(read), SIZE_FILTER_TYPE_STR));

                    // Does the pg file match?
                    if (pgFileSize == pgTestSize && strEq(pgFileChecksum, pgTestChecksum))
                    {
                        pgFileMatch = true;

                        // If it matches and is a reference to a previous backup then no need to copy the file
                        if (repoFileHasReference)
                        {
                            MEM_CONTEXT_PRIOR_BEGIN()
                            {
                                result.backupCopyResult = backupCopyResultNoOp;
                                result.copySize = pgTestSize;
                                result.copyChecksum = strDup(pgTestChecksum);
                            }
                            MEM_CONTEXT_PRIOR_END();
                        }
                    }
                }
                // Else the source file is missing from the database so skip this file
                else
                    result.backupCopyResult = backupCopyResultSkip;
            }

            // If this is not a delta backup or it is and the file exists and the checksum from the DB matches, then also test the
            // checksum of the file in the repo (unless it is in a prior backup) and if the checksum doesn't match, then there may
            // be corruption in the repo, so recopy
            if (!delta || !repoFileHasReference)
            {
                // If this is a delta backup and the file is missing from the DB, then remove it from the repo (backupManifestUpdate
                // will remove it from the manifest)
                if (result.backupCopyResult == backupCopyResultSkip)
                {
                    storageRemoveP(storageRepoWrite(), repoPathFile);
                }
                else if (!delta || pgFileMatch)
                {
                    // Check the repo file in a try block because on error (e.g. missing or corrupt file that can't be decrypted or
                    // decompressed) we should recopy rather than ending the backup.
                    TRY_BEGIN()
                    {
                        // Generate checksum/size for the repo file
                        IoRead *read = storageReadIo(storageNewReadP(storageRepo(), repoPathFile));

                        if (cipherType != cipherTypeNone)
                        {
                            ioFilterGroupAdd(
                                ioReadFilterGroup(read), cipherBlockNew(cipherModeDecrypt, cipherType, BUFSTR(cipherPass), NULL));
                        }

                        // Decompress the file if compressed
                        if (repoFileCompressType != compressTypeNone)
                            ioFilterGroupAdd(ioReadFilterGroup(read), decompressFilter(repoFileCompressType));

                        ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(HASH_TYPE_SHA1_STR));
                        ioFilterGroupAdd(ioReadFilterGroup(read), ioSizeNew());

                        ioReadDrain(read);

                        // Test checksum/size
                        const String *pgTestChecksum = varStr(
                            ioFilterGroupResult(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE_STR));
                        uint64_t pgTestSize = varUInt64Force(ioFilterGroupResult(ioReadFilterGroup(read), SIZE_FILTER_TYPE_STR));

                        // No need to recopy if checksum/size match
                        if (pgFileSize == pgTestSize && strEq(pgFileChecksum, pgTestChecksum))
                        {
                            MEM_CONTEXT_PRIOR_BEGIN()
                            {
                                result.backupCopyResult = backupCopyResultChecksum;
                                result.copySize = pgTestSize;
                                result.copyChecksum = strDup(pgTestChecksum);
                            }
                            MEM_CONTEXT_PRIOR_END();
                        }
                        // Else recopy when repo file is not as expected
                        else
                            result.backupCopyResult = backupCopyResultReCopy;
                    }
                    // Recopy on any kind of error
                    CATCH_ANY()
                    {
                        result.backupCopyResult = backupCopyResultReCopy;
                    }
                    TRY_END();
                }
            }
        }

        // Copy the file
        if (result.backupCopyResult == backupCopyResultCopy || result.backupCopyResult == backupCopyResultReCopy)
        {
            // Is the file compressible during the copy?
            bool compressible = repoFileCompressType == compressTypeNone && cipherType == cipherTypeNone;

            // Setup pg file for read. Only read as many bytes as passed in pgFileSize.  If the file is growing it does no good to
            // copy data past the end of the size recorded in the manifest since those blocks will need to be replayed from WAL
            // during recovery.
            StorageRead *read = storageNewReadP(
                storagePg(), pgFile, .ignoreMissing = pgFileIgnoreMissing, .compressible = compressible,
                .limit = pgFileCopyExactSize ? VARUINT64(pgFileSize) : NULL);
            ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), cryptoHashNew(HASH_TYPE_SHA1_STR));
            ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), ioSizeNew());

            // Add page checksum filter
            if (pgFileChecksumPage)
            {
                ioFilterGroupAdd(
                    ioReadFilterGroup(storageReadIo(read)), pageChecksumNew(segmentNumber(pgFile), PG_SEGMENT_PAGE_DEFAULT,
                    pgFileChecksumPageLsnLimit));
            }

            // Add compression
            if (repoFileCompressType != compressTypeNone)
            {
                ioFilterGroupAdd(
                    ioReadFilterGroup(storageReadIo(read)), compressFilter(repoFileCompressType, repoFileCompressLevel));
            }

            // If there is a cipher then add the encrypt filter
            if (cipherType != cipherTypeNone)
            {
                ioFilterGroupAdd(
                    ioReadFilterGroup(
                        storageReadIo(read)), cipherBlockNew(cipherModeEncrypt, cipherType, BUFSTR(cipherPass), NULL));
            }

            // Setup the repo file for write. There is no need to write the file atomically (e.g. via a temp file on Posix) because
            // checksums are tested on resume after a failed backup. The path does not need to be synced for each file because all
            // paths are synced at the end of the backup.
            StorageWrite *write = storageNewWriteP(
                storageRepoWrite(), repoPathFile, .compressible = compressible, .noAtomic = true, .noSyncPath = true);
            ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(write)), ioSizeNew());

            // Open the source and destination and copy the file
            if (storageCopy(read, write))
            {
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    // Get sizes and checksum
                    result.copySize = varUInt64Force(
                        ioFilterGroupResult(ioReadFilterGroup(storageReadIo(read)), SIZE_FILTER_TYPE_STR));
                    result.copyChecksum = strDup(
                        varStr(ioFilterGroupResult(ioReadFilterGroup(storageReadIo(read)), CRYPTO_HASH_FILTER_TYPE_STR)));
                    result.repoSize =
                        varUInt64Force(ioFilterGroupResult(ioWriteFilterGroup(storageWriteIo(write)), SIZE_FILTER_TYPE_STR));

                    // Get results of page checksum validation
                    if (pgFileChecksumPage)
                    {
                        result.pageChecksumResult = kvDup(
                            varKv(ioFilterGroupResult(ioReadFilterGroup(storageReadIo(read)), PAGE_CHECKSUM_FILTER_TYPE_STR)));
                    }
                }
                MEM_CONTEXT_PRIOR_END();
            }
            // Else if source file is missing and the read setup indicated ignore a missing file, the database removed it so skip it
            else
                result.backupCopyResult = backupCopyResultSkip;
        }

        // If the file was copied get the repo size only if the storage can store the files with a different size than what was
        // written. This has to be checked after the file is at rest because filesystem compression may affect the actual repo size
        // and this cannot be calculated in stream.
        //
        // If the file was checksummed then get the size in all cases since we don't already have it.
        if (((result.backupCopyResult == backupCopyResultCopy || result.backupCopyResult == backupCopyResultReCopy) &&
                storageFeature(storageRepo(), storageFeatureCompress)) ||
            result.backupCopyResult == backupCopyResultChecksum)
        {
            result.repoSize = storageInfoP(storageRepo(), repoPathFile).size;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}
