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
#include "common/type/json.h"
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
List *
backupFile(
    const CompressType repoFileCompressType, const int repoFileCompressLevel, const String *const backupLabel, const bool delta,
    const CipherType cipherType, const String *const cipherPass, const List *const fileList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, repoFileCompressType);             // Compress type for repo file
        FUNCTION_LOG_PARAM(INT,  repoFileCompressLevel);            // Compression level for repo file
        FUNCTION_LOG_PARAM(STRING, backupLabel);                    // Label of current backup
        FUNCTION_LOG_PARAM(BOOL, delta);                            // Is the delta option on?
        FUNCTION_LOG_PARAM(STRING_ID, cipherType);                  // Encryption type
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Password to access the repo file if encrypted
        FUNCTION_LOG_PARAM(LIST, fileList);                         // List of files to backup
    FUNCTION_LOG_END();

    ASSERT(backupLabel != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));
    ASSERT(fileList != NULL && !lstEmpty(fileList));

    // !!! GET THE FILE -- NEEDS TO BE A LOOP
    const BackupFile *const file = lstGet(fileList, 0);

    ASSERT(file->pgFile != NULL);
    ASSERT(file->repoFile != NULL);

    // Backup file results
    List *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = lstNewP(sizeof(BackupFileResult));
        BackupFileResult *fileResult = lstAdd(result, &(BackupFileResult){.backupCopyResult = backupCopyResultCopy});

        // Generate complete repo path and add compression extension if needed
        const String *repoPathFile = strNewFmt(
            STORAGE_REPO_BACKUP "/%s/%s%s", strZ(backupLabel), strZ(file->repoFile), strZ(compressExtStr(repoFileCompressType)));

        // If checksum is defined then the file needs to be checked. If delta option then check the DB and possibly the repo, else
        // just check the repo.
        if (file->pgFileChecksum != NULL)
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
                        storagePg(), file->pgFile, .ignoreMissing = file->pgFileIgnoreMissing,
                        .limit = file->pgFileCopyExactSize ? VARUINT64(file->pgFileSize) : NULL));
                ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(HASH_TYPE_SHA1_STR));
                ioFilterGroupAdd(ioReadFilterGroup(read), ioSizeNew());

                // If the pg file exists check the checksum/size
                if (ioReadDrain(read))
                {
                    const String *pgTestChecksum = pckReadStrP(
                        ioFilterGroupResultP(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE));
                    uint64_t pgTestSize = pckReadU64P(ioFilterGroupResultP(ioReadFilterGroup(read), SIZE_FILTER_TYPE));

                    // Does the pg file match?
                    if (file->pgFileSize == pgTestSize && strEq(file->pgFileChecksum, pgTestChecksum))
                    {
                        pgFileMatch = true;

                        // If it matches and is a reference to a previous backup then no need to copy the file
                        if (file->repoFileHasReference)
                        {
                            MEM_CONTEXT_BEGIN(lstMemContext(result))
                            {
                                fileResult->backupCopyResult = backupCopyResultNoOp;
                                fileResult->copySize = pgTestSize;
                                fileResult->copyChecksum = strDup(pgTestChecksum);
                            }
                            MEM_CONTEXT_END();
                        }
                    }
                }
                // Else the source file is missing from the database so skip this file
                else
                    fileResult->backupCopyResult = backupCopyResultSkip;
            }

            // If this is not a delta backup or it is and the file exists and the checksum from the DB matches, then also test the
            // checksum of the file in the repo (unless it is in a prior backup) and if the checksum doesn't match, then there may
            // be corruption in the repo, so recopy
            if (!delta || !file->repoFileHasReference)
            {
                // If this is a delta backup and the file is missing from the DB, then remove it from the repo (backupManifestUpdate
                // will remove it from the manifest)
                if (fileResult->backupCopyResult == backupCopyResultSkip)
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
                        const String *pgTestChecksum = pckReadStrP(
                            ioFilterGroupResultP(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE));
                        uint64_t pgTestSize = pckReadU64P(ioFilterGroupResultP(ioReadFilterGroup(read), SIZE_FILTER_TYPE));

                        // No need to recopy if checksum/size match
                        if (file->pgFileSize == pgTestSize && strEq(file->pgFileChecksum, pgTestChecksum))
                        {
                            MEM_CONTEXT_BEGIN(lstMemContext(result))
                            {
                                fileResult->backupCopyResult = backupCopyResultChecksum;
                                fileResult->copySize = pgTestSize;
                                fileResult->copyChecksum = strDup(pgTestChecksum);
                            }
                            MEM_CONTEXT_END();
                        }
                        // Else recopy when repo file is not as expected
                        else
                            fileResult->backupCopyResult = backupCopyResultReCopy;
                    }
                    // Recopy on any kind of error
                    CATCH_ANY()
                    {
                        fileResult->backupCopyResult = backupCopyResultReCopy;
                    }
                    TRY_END();
                }
            }
        }

        // Copy the file
        if (fileResult->backupCopyResult == backupCopyResultCopy || fileResult->backupCopyResult == backupCopyResultReCopy)
        {
            // Is the file compressible during the copy?
            bool compressible = repoFileCompressType == compressTypeNone && cipherType == cipherTypeNone;

            // Setup pg file for read. Only read as many bytes as passed in pgFileSize.  If the file is growing it does no good to
            // copy data past the end of the size recorded in the manifest since those blocks will need to be replayed from WAL
            // during recovery.
            StorageRead *read = storageNewReadP(
                storagePg(), file->pgFile, .ignoreMissing = file->pgFileIgnoreMissing, .compressible = compressible,
                .limit = file->pgFileCopyExactSize ? VARUINT64(file->pgFileSize) : NULL);
            ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), cryptoHashNew(HASH_TYPE_SHA1_STR));
            ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), ioSizeNew());

            // Add page checksum filter
            if (file->pgFileChecksumPage)
            {
                ioFilterGroupAdd(
                    ioReadFilterGroup(storageReadIo(read)), pageChecksumNew(segmentNumber(file->pgFile), PG_SEGMENT_PAGE_DEFAULT,
                    file->pgFileChecksumPageLsnLimit));
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
                MEM_CONTEXT_BEGIN(lstMemContext(result))
                {
                    // Get sizes and checksum
                    fileResult->copySize = pckReadU64P(
                        ioFilterGroupResultP(ioReadFilterGroup(storageReadIo(read)), SIZE_FILTER_TYPE));
                    fileResult->copyChecksum = strDup(
                        pckReadStrP(ioFilterGroupResultP(ioReadFilterGroup(storageReadIo(read)), CRYPTO_HASH_FILTER_TYPE)));
                    fileResult->repoSize =
                        pckReadU64P(ioFilterGroupResultP(ioWriteFilterGroup(storageWriteIo(write)), SIZE_FILTER_TYPE));

                    // Get results of page checksum validation
                    if (file->pgFileChecksumPage)
                    {
                        fileResult->pageChecksumResult = pckDup(
                            ioFilterGroupResultPackP(ioReadFilterGroup(storageReadIo(read)), PAGE_CHECKSUM_FILTER_TYPE));
                    }
                }
                MEM_CONTEXT_END();
            }
            // Else if source file is missing and the read setup indicated ignore a missing file, the database removed it so skip it
            else
                fileResult->backupCopyResult = backupCopyResultSkip;
        }

        // If the file was copied get the repo size only if the storage can store the files with a different size than what was
        // written. This has to be checked after the file is at rest because filesystem compression may affect the actual repo size
        // and this cannot be calculated in stream.
        //
        // If the file was checksummed then get the size in all cases since we don't already have it.
        if (((fileResult->backupCopyResult == backupCopyResultCopy || fileResult->backupCopyResult == backupCopyResultReCopy) &&
                storageFeature(storageRepo(), storageFeatureCompress)) ||
            fileResult->backupCopyResult == backupCopyResultChecksum)
        {
            fileResult->repoSize = storageInfoP(storageRepo(), repoPathFile).size;
        }

        lstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}
