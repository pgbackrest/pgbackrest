/***********************************************************************************************************************************
Backup File
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "command/backup/blockIncr.h"
#include "command/backup/file.h"
#include "command/backup/pageChecksum.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/bufferRead.h"
#include "common/io/filter/group.h"
#include "common/io/filter/size.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/type/convert.h"
#include "common/type/json.h"
#include "info/manifest.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
static unsigned int
segmentNumber(const String *const pgFile)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, pgFile);
    FUNCTION_TEST_END();

    // Determine which segment number this is by checking for a numeric extension. No extension means segment 0.
    FUNCTION_TEST_RETURN(UINT, regExpMatchOne(STRDEF("\\.[0-9]+$"), pgFile) ? cvtZToUInt(strrchr(strZ(pgFile), '.') + 1) : 0);
}

/**********************************************************************************************************************************/
FN_EXTERN List *
backupFile(
    const String *const repoFile, const uint64_t bundleId, const bool bundleRaw, const unsigned int blockIncrReference,
    const CompressType repoFileCompressType, const int repoFileCompressLevel, const CipherType cipherType,
    const String *const cipherPass, const String *const pgVersionForce, const PgPageSize pageSize, const List *const fileList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, repoFile);                       // Repo file
        FUNCTION_LOG_PARAM(UINT64, bundleId);                       // Bundle id (0 if none)
        FUNCTION_LOG_PARAM(BOOL, bundleRaw);                        // Raw compress/encrypt format in bundles?
        FUNCTION_LOG_PARAM(UINT, blockIncrReference);               // Block incremental reference to use in map
        FUNCTION_LOG_PARAM(ENUM, repoFileCompressType);             // Compress type for repo file
        FUNCTION_LOG_PARAM(INT, repoFileCompressLevel);             // Compression level for repo file
        FUNCTION_LOG_PARAM(STRING_ID, cipherType);                  // Encryption type
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Password to access the repo file if encrypted
        FUNCTION_LOG_PARAM(ENUM, pageSize);                         // Page size
        FUNCTION_LOG_PARAM(STRING, pgVersionForce);                 // Force pg version
        FUNCTION_LOG_PARAM(LIST, fileList);                         // List of files to backup
    FUNCTION_LOG_END();

    ASSERT(repoFile != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));
    ASSERT(fileList != NULL && !lstEmpty(fileList));
    ASSERT(pgPageSizeValid(pageSize));

    // Backup file results
    List *const result = lstNewP(sizeof(BackupFileResult));

    MEM_CONTEXT_TEMP_BEGIN()
    {
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
                    IoRead *const read = storageReadIo(
                        storageNewReadP(
                            storagePg(), file->pgFile, .ignoreMissing = file->pgFileIgnoreMissing,
                            .limit = file->pgFileCopyExactSize ? VARUINT64(file->pgFileSizeOriginal) : NULL));
                    ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(hashTypeSha1));
                    ioFilterGroupAdd(ioReadFilterGroup(read), ioSizeNew());

                    // If the pg file exists check the checksum/size
                    if (ioReadDrain(read))
                    {
                        const Buffer *const pgTestChecksum = pckReadBinP(
                            ioFilterGroupResultP(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE));
                        const uint64_t pgTestSize = pckReadU64P(ioFilterGroupResultP(ioReadFilterGroup(read), SIZE_FILTER_TYPE));

                        // Does the pg file match?
                        if (file->pgFileSize == pgTestSize && bufEq(file->pgFileChecksum, pgTestChecksum))
                        {
                            pgFileMatch = true;

                            // If it matches then no need to copy the file
                            MEM_CONTEXT_BEGIN(lstMemContext(result))
                            {
                                fileResult->backupCopyResult = backupCopyResultNoOp;
                                fileResult->copySize = file->pgFileSize;
                                fileResult->copyChecksum = file->pgFileChecksum;
                            }
                            MEM_CONTEXT_END();
                        }
                    }
                    // Else the source file is missing from the database so skip this file
                    else
                        fileResult->backupCopyResult = backupCopyResultSkip;
                }

                // On resume check the manifest file if it still exists in pg
                if (file->manifestFileResume && fileResult->backupCopyResult != backupCopyResultSkip)
                {
                    // Resumed files should never have a reference to a prior backup
                    ASSERT(!file->manifestFileHasReference);

                    // If the pg file matches or is unknown because delta was not performed then check the repo file
                    if (!file->pgFileDelta || pgFileMatch)
                    {
                        // Generate checksum/size for the repo file
                        IoRead *const read = storageReadIo(storageNewReadP(storageRepo(), repoFile));
                        ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(hashTypeSha1));
                        ioFilterGroupAdd(ioReadFilterGroup(read), ioSizeNew());
                        ioReadDrain(read);

                        // Test checksum/size
                        const Buffer *const pgTestChecksum = pckReadBinP(
                            ioFilterGroupResultP(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE));
                        const uint64_t pgTestSize = pckReadU64P(ioFilterGroupResultP(ioReadFilterGroup(read), SIZE_FILTER_TYPE));

                        // No need to recopy if checksum/size match. When the repo checksum is missing still compare to repo size
                        // since the repo checksum should only be missing when the repo file was not compressed/encrypted, i.e. the
                        // repo size should match the original size. There is no need to worry about old manifests here since resume
                        // does not work across versions.
                        if (file->repoFileSize == pgTestSize &&
                            bufEq(file->repoFileChecksum != NULL ? file->repoFileChecksum : file->pgFileChecksum, pgTestChecksum))
                        {
                            MEM_CONTEXT_BEGIN(lstMemContext(result))
                            {
                                fileResult->backupCopyResult = backupCopyResultChecksum;
                                fileResult->copySize = file->pgFileSize;
                                fileResult->copyChecksum = file->pgFileChecksum;
                            }
                            MEM_CONTEXT_END();
                        }
                        // Else copy when repo file is invalid
                        else
                        {
                            // Delta may have changed the result so set it back to copy
                            fileResult->backupCopyResult = backupCopyResultCopy;
                            fileResult->repoInvalid = true;
                        }
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

                if (fileResult->backupCopyResult == backupCopyResultCopy)
                {
                    // Setup pg file for read. Only read as many bytes as passed in pgFileSize. If the file is growing it does no
                    // good to copy data past the end of the size recorded in the manifest since those blocks will need to be
                    // replayed from WAL during recovery. pg_control requires special handling since it needs to be retried on crc
                    // validation failure.
                    bool repoChecksum = false;
                    IoRead *readIo;

                    if (strEqZ(file->pgFile, PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL))
                        readIo = ioBufferReadNew(pgControlBufferFromFile(storagePg(), pgVersionForce));
                    else
                    {
                        readIo = storageReadIo(
                            storageNewReadP(
                                storagePg(), file->pgFile, .ignoreMissing = file->pgFileIgnoreMissing, .compressible = compressible,
                                .limit = file->pgFileCopyExactSize ? VARUINT64(file->pgFileSizeOriginal) : NULL));
                    }

                    ioFilterGroupAdd(ioReadFilterGroup(readIo), cryptoHashNew(hashTypeSha1));
                    ioFilterGroupAdd(ioReadFilterGroup(readIo), ioSizeNew());

                    // Add page checksum filter
                    if (file->pgFileChecksumPage)
                    {
                        ioFilterGroupAdd(
                            ioReadFilterGroup(readIo),
                            pageChecksumNew(
                                segmentNumber(file->pgFile), PG_SEGMENT_SIZE_DEFAULT / pageSize, pageSize,
                                file->pgFilePageHeaderCheck, storagePathP(storagePg(), file->pgFile)));
                    }

                    // Compress filter
                    IoFilter *const compress =
                        repoFileCompressType != compressTypeNone ?
                            compressFilterP(
                                repoFileCompressType, repoFileCompressLevel, .raw = bundleRaw || file->blockIncrSize != 0) :
                            NULL;

                    // Encrypt filter
                    IoFilter *const encrypt =
                        cipherType != cipherTypeNone ?
                            cipherBlockNewP(
                                cipherModeEncrypt, cipherType, BUFSTR(cipherPass), .raw = bundleRaw || file->blockIncrSize != 0) :
                            NULL;

                    // If block incremental then add the filter and pass compress/encrypt filters to it since each block is
                    // compressed/encrypted separately
                    if (file->blockIncrSize != 0)
                    {
                        // Read prior block map
                        const Buffer *blockMap = NULL;

                        if (file->blockIncrMapPriorFile != NULL)
                        {
                            StorageRead *const blockMapRead = storageNewReadP(
                                storageRepo(), file->blockIncrMapPriorFile, .offset = file->blockIncrMapPriorOffset,
                                .limit = VARUINT64(file->blockIncrMapPriorSize));

                            if (cipherType != cipherTypeNone)
                            {
                                ioFilterGroupAdd(
                                    ioReadFilterGroup(storageReadIo(blockMapRead)),
                                    cipherBlockNewP(cipherModeDecrypt, cipherType, BUFSTR(cipherPass), .raw = true));
                            }

                            blockMap = storageGetP(blockMapRead);
                        }

                        // Add block incremental filter
                        ioFilterGroupAdd(
                            ioReadFilterGroup(readIo),
                            blockIncrNew(
                                file->blockIncrSuperSize, file->blockIncrSize, file->blockIncrChecksumSize, blockIncrReference,
                                bundleId, bundleOffset, blockMap, compress, encrypt));

                        repoChecksum = true;
                    }
                    // Else apply compress/encrypt filters to the entire file
                    else
                    {
                        // Add compress filter
                        if (compress != NULL)
                        {
                            ioFilterGroupAdd(ioReadFilterGroup(readIo), compress);
                            repoChecksum = true;
                        }

                        // Add encrypt filter
                        if (encrypt != NULL)
                        {
                            ioFilterGroupAdd(ioReadFilterGroup(readIo), encrypt);
                            repoChecksum = true;
                        }
                    }

                    // Capture checksum of file stored in the repo if filters that modify the output have been applied
                    if (repoChecksum)
                        ioFilterGroupAdd(ioReadFilterGroup(readIo), cryptoHashNew(hashTypeSha1));

                    // Add size filter last to calculate repo size
                    ioFilterGroupAdd(ioReadFilterGroup(readIo), ioSizeNew());

                    // Open the source
                    if (ioReadOpen(readIo))
                    {
                        Buffer *const buffer = bufNew(ioBufferSize());
                        bool readEof = false;

                        // Read the first buffer to determine if the file was truncated or was not changed. Detecting truncation
                        // matters only when bundling is enabled as otherwise the file will be stored anyway.
                        ioRead(readIo, buffer);

                        if (ioReadEof(readIo))
                        {
                            // Close the source and set eof
                            ioReadClose(readIo);
                            readEof = true;

                            // Get file size
                            fileResult->copySize = pckReadU64P(
                                ioFilterGroupResultP(ioReadFilterGroup(readIo), SIZE_FILTER_TYPE, .idx = 0));

                            // If file is zero-length then it was truncated during the backup. When bundling we can simply mark it
                            // as truncated since no file needs to be stored.
                            if (bundleId != 0 && fileResult->copySize == 0)
                            {
                                fileResult->backupCopyResult = backupCopyResultTruncate;
                                fileResult->copyChecksum = HASH_TYPE_SHA1_ZERO_BUF;

                                ASSERT(
                                    bufEq(
                                        fileResult->copyChecksum,
                                        pckReadBinP(
                                            ioFilterGroupResultP(ioReadFilterGroup(readIo), CRYPTO_HASH_FILTER_TYPE, .idx = 0))));
                            }
                            // Else check if size is equal to prior size
                            else if (file->manifestFileHasReference && fileResult->copySize == file->pgFileSize)
                            {
                                const Buffer *const copyChecksum = pckReadBinP(
                                    ioFilterGroupResultP(ioReadFilterGroup(readIo), CRYPTO_HASH_FILTER_TYPE));

                                // If checksum is also equal then no need to copy the file
                                if (bufEq(file->pgFileChecksum, copyChecksum))
                                {
                                    // If block incremental make sure no map was returned but a prior map was provided
                                    ASSERT(
                                        file->blockIncrSize == 0 ||
                                        (pckReadU64P(
                                             ioFilterGroupResultP(ioReadFilterGroup(readIo), BLOCK_INCR_FILTER_TYPE)) == 0 &&
                                         file->blockIncrMapPriorFile != NULL));

                                    fileResult->backupCopyResult = backupCopyResultNoOp;
                                    fileResult->copyChecksum = file->pgFileChecksum;
                                }
                            }
                        }

                        // Copy the file
                        if (fileResult->backupCopyResult == backupCopyResultCopy)
                        {
                            // Setup the repo file for write. There is no need to write the file atomically (e.g. via a temp file on
                            // Posix) because checksums are tested on resume after a failed backup. The path does not need to be
                            // synced for each file because all paths are synced at the end of the backup. It needs to be created in
                            // the prior context because it will live longer than a single loop when more than one file is being
                            // written.
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

                            // Write the first buffer
                            ioWrite(storageWriteIo(write), buffer);
                            bufFree(buffer);

                            // Copy remainder of the file if not eof
                            if (!readEof)
                            {
                                ioCopyP(readIo, storageWriteIo(write));

                                // Close the source
                                ioReadClose(readIo);
                            }

                            // Get copy results
                            MEM_CONTEXT_BEGIN(lstMemContext(result))
                            {
                                // Get size and checksum
                                fileResult->copySize = pckReadU64P(
                                    ioFilterGroupResultP(ioReadFilterGroup(readIo), SIZE_FILTER_TYPE, .idx = 0));
                                fileResult->copyChecksum = pckReadBinP(
                                    ioFilterGroupResultP(ioReadFilterGroup(readIo), CRYPTO_HASH_FILTER_TYPE, .idx = 0));

                                // Get bundle offset
                                fileResult->bundleOffset = bundleOffset;

                                // Get repo size
                                fileResult->repoSize = pckReadU64P(
                                    ioFilterGroupResultP(ioReadFilterGroup(readIo), SIZE_FILTER_TYPE, .idx = 1));

                                // Get results of page checksum validation
                                if (file->pgFileChecksumPage)
                                {
                                    fileResult->pageChecksumResult = pckDup(
                                        ioFilterGroupResultPackP(ioReadFilterGroup(readIo), PAGE_CHECKSUM_FILTER_TYPE));
                                }

                                // Get results of block incremental
                                if (file->blockIncrSize != 0)
                                {
                                    fileResult->blockIncrMapSize = pckReadU64P(
                                        ioFilterGroupResultP(ioReadFilterGroup(readIo), BLOCK_INCR_FILTER_TYPE));

                                    // There must be a map because the file should have changed or shrunk
                                    ASSERT(fileResult->blockIncrMapSize > 0);
                                }

                                // Get repo checksum
                                if (repoChecksum)
                                {
                                    fileResult->repoChecksum = pckReadBinP(
                                        ioFilterGroupResultP(ioReadFilterGroup(readIo), CRYPTO_HASH_FILTER_TYPE, .idx = 1));
                                }
                            }
                            MEM_CONTEXT_END();

                            bundleOffset += fileResult->repoSize;
                        }
                    }
                    // Else if source file is missing and the read setup indicated ignore a missing file, the database removed it so
                    // skip it
                    else
                        fileResult->backupCopyResult = backupCopyResultSkip;
                }

                // Remove the file if it was skipped and not bundled. The file will not always exist, but does need to be removed in
                // the case where the file existed before a resume or in the preliminary phase of a full/incr backup.
                if (fileResult->backupCopyResult == backupCopyResultSkip && bundleId == 0)
                    storageRemoveP(storageRepoWrite(), repoFile);
            }
            MEM_CONTEXT_TEMP_END();
        }

        // Close the repository file if it was opened
        if (write != NULL)
            ioWriteClose(storageWriteIo(write));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(LIST, result);
}
