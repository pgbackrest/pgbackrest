/***********************************************************************************************************************************
Backup File
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "command/backup/file.h"
#include "command/backup/pageChecksum.h"
#include "common/compress/gzip/common.h"
#include "common/compress/gzip/compress.h"
#include "common/compress/gzip/decompress.h"
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
Copy a file from the PostgreSQL data directory to the repository
***********************************************************************************************************************************/
#define FUNCTION_LOG_BACKUP_FILE_RESULT_TYPE                                                                                       \
    BackupFileResult
#define FUNCTION_LOG_BACKUP_FILE_RESULT_FORMAT(value, buffer, bufferSize)                                                          \
    objToLog(&value, "BackupFileResult", buffer, bufferSize)

BackupFileResult
backupFile(
    const String *pgFile, bool pgFileIgnoreMissing, uint64_t pgFileSize, const String *pgFileChecksum, bool pgFileChecksumPage,
    uint64_t pgFileChecksumPageLsnLimit, const String *repoFile, bool repoFileHasReference, bool repoFileCompress,
    unsigned int repoFileCompressLevel, const String *backupLabel, bool delta, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pgFile);
        FUNCTION_LOG_PARAM(BOOL, pgFileIgnoreMissing);
        FUNCTION_LOG_PARAM(UINT64, pgFileSize);
        FUNCTION_LOG_PARAM(STRING, pgFileChecksum);
        FUNCTION_LOG_PARAM(BOOL, pgFileChecksumPage);
        FUNCTION_LOG_PARAM(UINT64, pgFileChecksumPageLsnLimit);
        FUNCTION_LOG_PARAM(STRING, repoFile);
        FUNCTION_LOG_PARAM(BOOL, repoFileHasReference);
        FUNCTION_LOG_PARAM(BOOL, repoFileCompress);
        FUNCTION_LOG_PARAM(UINT, repoFileCompressLevel);
        FUNCTION_LOG_PARAM(STRING, backupLabel);
        FUNCTION_LOG_PARAM(BOOL, delta);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(pgFile != NULL);
    ASSERT(repoFile != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    // Backup file results
    BackupFileResult result = {.backupCopyResult = backupCopyResultCopy};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Generate complete repo path and add compression extension if needed
        const String *repoPathFile = strNewFmt(
            STORAGE_REPO_BACKUP "/%s/%s%s", strPtr(backupLabel), strPtr(repoFile), repoFileCompress ? "." GZIP_EXT : "");

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
                // Generate checksum/size for the pg file
                IoRead *read = storageReadIo(storageNewReadP(storagePg(), pgFile, .ignoreMissing = true));
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
                            memContextSwitch(MEM_CONTEXT_OLD());
                            result.backupCopyResult = backupCopyResultNoOp;
                            result.copySize = pgTestSize;
                            result.copyChecksum = strDup(pgTestChecksum);
                            memContextSwitch(MEM_CONTEXT_TEMP());
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
                    storageRemoveNP(
                        storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strPtr(backupLabel), strPtr(repoPathFile)));
                }
                else if (!delta || pgFileMatch)
                {
                    // Generate checksum/size for the repo file
                    IoRead *read = storageReadIo(storageNewReadNP(storageRepo(), repoPathFile));

                    if (cipherPass != NULL)
                    {
                        ioFilterGroupAdd(
                            ioReadFilterGroup(read), cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass),
                            NULL));
                    }

                    if (repoFileCompress)
                        ioFilterGroupAdd(ioReadFilterGroup(read), gzipDecompressNew(false));

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
                        memContextSwitch(MEM_CONTEXT_OLD());
                        result.backupCopyResult = backupCopyResultChecksum;
                        result.copySize = pgTestSize;
                        result.copyChecksum = strDup(pgTestChecksum);
                        memContextSwitch(MEM_CONTEXT_TEMP());
                    }
                    // Else recopy when repo file is not as expected
                    else
                        result.backupCopyResult = backupCopyResultReCopy;
                }
            }
        }

        // Copy the file
        if (result.backupCopyResult == backupCopyResultCopy || result.backupCopyResult == backupCopyResultReCopy)
        {
            // Is the file compressible during the copy?
            bool compressible = !repoFileCompress && cipherType == cipherTypeNone;

            // Open pg file for read
            StorageRead *read = storageNewReadP(storagePg(), pgFile, .ignoreMissing = true, .compressible = compressible);
            ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), cryptoHashNew(HASH_TYPE_SHA1_STR));
            ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), ioSizeNew());

            // Add page checksum filter
            if (pgFileChecksumPage)
            {
                // Determine which segment no this is by checking for a numeric extension.  No extension means segment 0.
                unsigned int segmentNo = regExpMatchOne(STRDEF("\\.[0-9]+$"), pgFile) ?
                    cvtZToUInt(strrchr(strPtr(pgFile), '.') + 1) : 0;

                ioFilterGroupAdd(
                    ioReadFilterGroup(storageReadIo(read)),
                    pageChecksumNew(segmentNo, PG_SEGMENT_PAGE_DEFAULT, PG_PAGE_SIZE_DEFAULT, pgFileChecksumPageLsnLimit));
            }

            // Add compression
            if (repoFileCompress)
                ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), gzipCompressNew((int)repoFileCompressLevel, false));

            // If there is a cipher then add the encrypt filter
            if (cipherType != cipherTypeNone)
            {
                ioFilterGroupAdd(
                    ioReadFilterGroup(
                        storageReadIo(read)), cipherBlockNew(cipherModeEncrypt, cipherType, BUFSTR(cipherPass), NULL));
            }

            // Open the repo file for write
            StorageWrite *write = storageNewWriteP(storageRepoWrite(), repoPathFile, .compressible = compressible);
            ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(write)), ioSizeNew());

            // Copy the file
            if (storageCopy(read, write))
            {
                memContextSwitch(MEM_CONTEXT_OLD());

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

                memContextSwitch(MEM_CONTEXT_TEMP());
            }
            // Else if source file is missing the database removed it
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
            result.repoSize = storageInfoNP(storageRepo(), repoPathFile).size;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BACKUP_FILE_RESULT, result);
}
