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
#include "info/manifest.h"
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
    FUNCTION_TEST_RETURN(UINT, regExpMatchOne(STRDEF("\\.[0-9]+$"), pgFile) ? cvtZToUInt(strrchr(strZ(pgFile), '.') + 1) : 0);
}

/**********************************************************************************************************************************/
List *
backupFile(
    const String *const repoFile, const CompressType repoFileCompressType, const int repoFileCompressLevel,
    const CipherType cipherType, const String *const cipherPass, const List *const fileList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, repoFile);                       // Repo file
        FUNCTION_LOG_PARAM(ENUM, repoFileCompressType);             // Compress type for repo file
        FUNCTION_LOG_PARAM(INT, repoFileCompressLevel);             // Compression level for repo file
        FUNCTION_LOG_PARAM(STRING_ID, cipherType);                  // Encryption type
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Password to access the repo file if encrypted
        FUNCTION_LOG_PARAM(LIST, fileList);                         // List of files to backup
    FUNCTION_LOG_END();

    ASSERT(repoFile != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));
    ASSERT(fileList != NULL && !lstEmpty(fileList));

    // Backup file results
    List *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = lstNewP(sizeof(BackupFileResult));

        // Check files to determine which ones need to be copied
        for (unsigned int fileIdx = 0; fileIdx < lstSize(fileList); fileIdx++)
        {
            // Use a per-file mem context to reduce memory usage
            MEM_CONTEXT_TEMP_BEGIN()
            {
                const BackupFile *const file = lstGet(fileList, fileIdx);
                ASSERT(file->pgFile != NULL);
                ASSERT(file->manifestFile != NULL);
                ASSERT((!file->pgFileDelta && !file->manifestFileResume) || file->pgFileChecksum != NULL);

                BackupFileResult *const fileResult = lstAdd(
                    result, &(BackupFileResult){.manifestFile = file->manifestFile, .backupCopyResult = backupCopyResultCopy});

                // Does the file in pg match the checksum and size passed?
                bool pgFileMatch = false;

                // If delta then check the pg checksum
                if (file->pgFileDelta)
                {
                    // Generate checksum/size for the pg file. Only read as many bytes as passed in pgFileSize. If the file has
                    // grown since the manifest was built we don't need to consider the extra bytes since they will be replayed from
                    // WAL during recovery.
                    IoRead *read = storageReadIo(
                        storageNewReadP(
                            storagePg(), file->pgFile, .ignoreMissing = file->pgFileIgnoreMissing,
                            .limit = file->pgFileCopyExactSize ? VARUINT64(file->pgFileSize) : NULL));
                    ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(hashTypeSha1));
                    ioFilterGroupAdd(ioReadFilterGroup(read), ioSizeNew());

                    // If the pg file exists check the checksum/size
                    if (ioReadDrain(read))
                    {
                        const String *const pgTestChecksum = bufHex(
                            pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE)));
                        uint64_t pgTestSize = pckReadU64P(ioFilterGroupResultP(ioReadFilterGroup(read), SIZE_FILTER_TYPE));

                        // Does the pg file match?
                        if (file->pgFileSize == pgTestSize && strEq(file->pgFileChecksum, pgTestChecksum))
                        {
                            pgFileMatch = true;

                            // If it matches and is a reference to a previous backup then no need to copy the file
                            if (file->manifestFileHasReference)
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

                // On resume check the manifest file
                if (file->manifestFileResume)
                {
                    // Resumed files should never have a reference to a prior backup
                    ASSERT(!file->manifestFileHasReference);

                    // If the file is missing from pg, then remove it from the repo (backupJobResult() will remove it from the
                    // manifest)
                    if (fileResult->backupCopyResult == backupCopyResultSkip)
                    {
                        storageRemoveP(storageRepoWrite(), repoFile);
                    }
                    // Else if the pg file matches or is unknown because delta was not performed then check the repo file
                    else if (!file->pgFileDelta || pgFileMatch)
                    {
                        // Check the repo file in a try block because on error (e.g. missing or corrupt file that can't be decrypted
                        // or decompressed) we should recopy rather than ending the backup.
                        TRY_BEGIN()
                        {
                            // Generate checksum/size for the repo file
                            IoRead *read = storageReadIo(storageNewReadP(storageRepo(), repoFile));

                            if (cipherType != cipherTypeNone)
                            {
                                ioFilterGroupAdd(
                                    ioReadFilterGroup(read),
                                    cipherBlockNew(cipherModeDecrypt, cipherType, BUFSTR(cipherPass), NULL));
                            }

                            // Decompress the file if compressed
                            if (repoFileCompressType != compressTypeNone)
                                ioFilterGroupAdd(ioReadFilterGroup(read), decompressFilter(repoFileCompressType));

                            ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(hashTypeSha1));
                            ioFilterGroupAdd(ioReadFilterGroup(read), ioSizeNew());

                            ioReadDrain(read);

                            // Test checksum/size
                            const String *const pgTestChecksum = bufHex(
                                pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE)));
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
            MEM_CONTEXT_TEMP_END();
        }

        // Are the files compressible during the copy?
        const bool compressible = repoFileCompressType == compressTypeNone && cipherType == cipherTypeNone;

        // Copy files that need to be copied
        StorageWrite *write = NULL;
        uint64_t bundleOffset = 0;

        for (unsigned int fileIdx = 0; fileIdx < lstSize(fileList); fileIdx++)
        {
            // Use a per-file mem context to reduce memory usage
            MEM_CONTEXT_TEMP_BEGIN()
            {
                const BackupFile *const file = lstGet(fileList, fileIdx);
                BackupFileResult *const fileResult = lstGet(result, fileIdx);

                if (fileResult->backupCopyResult == backupCopyResultCopy || fileResult->backupCopyResult == backupCopyResultReCopy)
                {
                    // Setup pg file for read. Only read as many bytes as passed in pgFileSize.  If the file is growing it does no
                    // good to copy data past the end of the size recorded in the manifest since those blocks will need to be
                    // replayed from WAL during recovery.
                    StorageRead *read = storageNewReadP(
                        storagePg(), file->pgFile, .ignoreMissing = file->pgFileIgnoreMissing, .compressible = compressible,
                        .limit = file->pgFileCopyExactSize ? VARUINT64(file->pgFileSize) : NULL);
                    ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), cryptoHashNew(hashTypeSha1));
                    ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), ioSizeNew());

                    // Add page checksum filter
                    if (file->pgFileChecksumPage)
                    {
                        ioFilterGroupAdd(
                            ioReadFilterGroup(storageReadIo(read)),
                            pageChecksumNew(
                                segmentNumber(file->pgFile), PG_SEGMENT_PAGE_DEFAULT, storagePathP(storagePg(), file->pgFile)));
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
                            ioReadFilterGroup(storageReadIo(read)),
                            cipherBlockNew(cipherModeEncrypt, cipherType, BUFSTR(cipherPass), NULL));
                    }

                    // Add size filter last to calculate repo size
                    ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), ioSizeNew());

                    // Open the source and destination and copy the file
                    if (ioReadOpen(storageReadIo(read)))
                    {
                        // Setup the repo file for write. There is no need to write the file atomically (e.g. via a temp file on
                        // Posix) because checksums are tested on resume after a failed backup. The path does not need to be synced
                        // for each file because all paths are synced at the end of the backup. It needs to be created in the prior
                        // context because it will live longer than a single loop when more than one file is being written.
                        if (write == NULL)
                        {
                            MEM_CONTEXT_PRIOR_BEGIN()
                            {
                                write = storageNewWriteP(
                                    storageRepoWrite(), repoFile, .compressible = compressible, .noAtomic = true,
                                    .noSyncPath = true);
                                ioWriteOpen(storageWriteIo(write));
                            }
                            MEM_CONTEXT_PRIOR_END();
                        }

                        // Copy data from source to destination
                        ioCopyP(storageReadIo(read), storageWriteIo(write));

                        // Close the source
                        ioReadClose(storageReadIo(read));

                        MEM_CONTEXT_BEGIN(lstMemContext(result))
                        {
                            // Get sizes and checksum
                            fileResult->copySize = pckReadU64P(
                                ioFilterGroupResultP(ioReadFilterGroup(storageReadIo(read)), SIZE_FILTER_TYPE, .idx = 0));
                            fileResult->bundleOffset = bundleOffset;
                            fileResult->copyChecksum = bufHex(
                                pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(storageReadIo(read)), CRYPTO_HASH_FILTER_TYPE)));
                            fileResult->repoSize = pckReadU64P(
                                ioFilterGroupResultP(ioReadFilterGroup(storageReadIo(read)), SIZE_FILTER_TYPE, .idx = 1));

                            // Get results of page checksum validation
                            if (file->pgFileChecksumPage)
                            {
                                fileResult->pageChecksumResult = pckDup(
                                    ioFilterGroupResultPackP(ioReadFilterGroup(storageReadIo(read)), PAGE_CHECKSUM_FILTER_TYPE));
                            }
                        }
                        MEM_CONTEXT_END();

                        bundleOffset += fileResult->repoSize;
                    }
                    // Else if source file is missing and the read setup indicated ignore a missing file, the database removed it so
                    // skip it
                    else
                        fileResult->backupCopyResult = backupCopyResultSkip;
                }
            }
            MEM_CONTEXT_TEMP_END();
        }

        // Close the repository file if it was opened
        if (write != NULL)
            ioWriteClose(storageWriteIo(write));

        lstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}
