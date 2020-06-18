/***********************************************************************************************************************************
Verify File
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/verify/file.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/filter/group.h"
#include "common/io/filter/sink.h"
#include "common/io/filter/size.h"
#include "common/io/io.h"
#include "common/log.h"
#include "storage/helper.h"

VerifyFileResult
verifyFile(
    const String *filePathName, const String *fileChecksum, bool sizeCheck, uint64_t fileSize, CompressType fileCompressType,
    CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, filePathName);                   // Fully qualified file name
        FUNCTION_LOG_PARAM(STRING, fileChecksum);                   // Checksum that the file should be
        FUNCTION_LOG_PARAM(BOOL, sizeCheck);                        // Can the size be verified?
        FUNCTION_LOG_PARAM(UINT64, fileSize);                       // Size of file (if checkable, else 0)
        FUNCTION_LOG_PARAM(ENUM, fileCompressType);                 // Compress type for file
        FUNCTION_LOG_PARAM(ENUM, cipherType);                       // Encryption type
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Password to access the repo file if encrypted
    FUNCTION_LOG_END();

    ASSERT(filePathName != NULL);
    ASSERT(fileChecksum != NULL);

    // Is the file valid?
    VerifyFileResult result = {.filePathName = strDup(filePathName)};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Prepare the file for reading
        IoRead *read = storageReadIo(storageNewReadP(storageRepo(), filePathName));
        IoFilterGroup *filterGroup = ioReadFilterGroup(read);

        // Add decryption filter
        if (cipherPass != NULL)
            ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));
// CSHANG How do I tell if file could not decompress? And what about errors that could be thrown - do I need to catch all those here and try to return something?
        // Add decompression filter
        if (fileCompressType != compressTypeNone)
            ioFilterGroupAdd(filterGroup, decompressFilter(fileCompressType));

        // Add sha1 filter
        ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));

        // Add size filter
        ioFilterGroupAdd(filterGroup, ioSizeNew());

        // Add IoSink so the file data is not transmitted from the remote
        ioFilterGroupAdd(filterGroup, ioSinkNew());
// CSHANG So if it is a WAL file and we get here, it is because at the time the verify command started, we read the archive DIR and found the file - but an expire could run and delete it out from under us, so it's not a problem but we need to indicate it is missing
        // If the file exists check the checksum/size
        if (ioReadDrain(read))
        {
            // Validate checksum
            if (!strEq(fileChecksum, varStr(ioFilterGroupResult(filterGroup, CRYPTO_HASH_FILTER_TYPE_STR))))
            {
                result.fileResult = verifyChecksumMismatch;
            }
            // CSHANG does size filter return 0 if file size is 0? Assume it does...but just in case...
            // If the size can be checked, do so
            else if (sizeCheck && fileSize != varUInt64Force(ioFilterGroupResult(ioReadFilterGroup(read), SIZE_FILTER_TYPE_STR)))
            {
                result.fileResult = verifySizeInvalid;
            }
        }
        else
            result.fileResult = verifyFileMissing;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(VERIFY_FILE_RESULT, result);
}

// -------------------------- RESTORE reference
// #include <fcntl.h>
// #include <unistd.h>
// #include <utime.h>
//
// #include "command/restore/file.h"
// #include "common/crypto/cipherBlock.h"
// #include "common/crypto/hash.h"
// #include "common/debug.h"
// #include "common/io/filter/group.h"
// #include "common/io/filter/size.h"
// #include "common/io/io.h"
// #include "common/log.h"
// #include "config/config.h"
// #include "storage/helper.h"
//
// /**********************************************************************************************************************************/
// bool
// restoreFile(
//     const String *repoFile, const String *repoFileReference, CompressType repoFileCompressType, const String *pgFile,
//     const String *pgFileChecksum, bool pgFileZero, uint64_t pgFileSize, time_t pgFileModified, mode_t pgFileMode,
//     const String *pgFileUser, const String *pgFileGroup, time_t copyTimeBegin, bool delta, bool deltaForce,
//     const String *cipherPass)
// {
//     FUNCTION_LOG_BEGIN(logLevelDebug);
//         FUNCTION_LOG_PARAM(STRING, repoFile);
//         FUNCTION_LOG_PARAM(STRING, repoFileReference);
//         FUNCTION_LOG_PARAM(ENUM, repoFileCompressType);
//         FUNCTION_LOG_PARAM(STRING, pgFile);
//         FUNCTION_LOG_PARAM(STRING, pgFileChecksum);
//         FUNCTION_LOG_PARAM(BOOL, pgFileZero);
//         FUNCTION_LOG_PARAM(UINT64, pgFileSize);
//         FUNCTION_LOG_PARAM(TIME, pgFileModified);
//         FUNCTION_LOG_PARAM(MODE, pgFileMode);
//         FUNCTION_LOG_PARAM(STRING, pgFileUser);
//         FUNCTION_LOG_PARAM(STRING, pgFileGroup);
//         FUNCTION_LOG_PARAM(TIME, copyTimeBegin);
//         FUNCTION_LOG_PARAM(BOOL, delta);
//         FUNCTION_LOG_PARAM(BOOL, deltaForce);
//         FUNCTION_TEST_PARAM(STRING, cipherPass);
//     FUNCTION_LOG_END();
//
//     ASSERT(repoFile != NULL);
//     ASSERT(repoFileReference != NULL);
//     ASSERT(pgFile != NULL);
//
//     // Was the file copied?
//     bool result = true;
//
//     // Is the file compressible during the copy?
//     bool compressible = true;
//
//     MEM_CONTEXT_TEMP_BEGIN()
//     {
//         // Perform delta if requested.  Delta zero-length files to avoid overwriting the file if the timestamp is correct.
//         if (delta && !pgFileZero)
//         {
//             // Perform delta if the file exists
//             StorageInfo info = storageInfoP(storagePg(), pgFile, .ignoreMissing = true, .followLink = true);
//
//             if (info.exists)
//             {
//                 // If force then use size/timestamp delta
//                 if (deltaForce)
//                 {
//                     // Make sure that timestamp/size are equal and that timestamp is before the copy start time of the backup
//                     if (info.size == pgFileSize && info.timeModified == pgFileModified && info.timeModified < copyTimeBegin)
//                         result = false;
//                 }
//                 // Else use size and checksum
//                 else
//                 {
//                     // Only continue delta if the file size is as expected
//                     if (info.size == pgFileSize)
//                     {
//                         // Generate checksum for the file if size is not zero
//                         IoRead *read = NULL;
//
//                         if (info.size != 0)
//                         {
//                             read = storageReadIo(storageNewReadP(storagePgWrite(), pgFile));
//                             ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(HASH_TYPE_SHA1_STR));
//                             ioReadDrain(read);
//                         }
//
//                         // If size and checksum are equal then no need to copy the file
//                         if (pgFileSize == 0 ||
//                             strEq(
//                                 pgFileChecksum, varStr(ioFilterGroupResult(ioReadFilterGroup(read), CRYPTO_HASH_FILTER_TYPE_STR))))
//                         {
//                             // Even if hash/size are the same set the time back to backup time.  This helps with unit testing, but
//                             // also presents a pristine version of the database after restore.
//                             if (info.timeModified != pgFileModified)
//                             {
//                                 THROW_ON_SYS_ERROR_FMT(
//                                     utime(
//                                         strPtr(storagePathP(storagePg(), pgFile)),
//                                         &((struct utimbuf){.actime = pgFileModified, .modtime = pgFileModified})) == -1,
//                                     FileInfoError, "unable to set time for '%s'", strPtr(storagePathP(storagePg(), pgFile)));
//                             }
//
//                             result = false;
//                         }
//                     }
//                 }
//             }
//         }
//
//         // Copy file from repository to database or create zero-length/sparse file
//         if (result)
//         {
//             // Create destination file
//             StorageWrite *pgFileWrite = storageNewWriteP(
//                 storagePgWrite(), pgFile, .modeFile = pgFileMode, .user = pgFileUser, .group = pgFileGroup,
//                 .timeModified = pgFileModified, .noAtomic = true, .noCreatePath = true, .noSyncPath = true);
//
//             // If size is zero/sparse no need to actually copy
//             if (pgFileSize == 0 || pgFileZero)
//             {
//                 ioWriteOpen(storageWriteIo(pgFileWrite));
//
//                 // Truncate the file to specified length (note in this case the file with grow, not shrink)
//                 if (pgFileZero)
//                 {
//                     THROW_ON_SYS_ERROR_FMT(
//                         ftruncate(ioWriteHandle(storageWriteIo(pgFileWrite)), (off_t)pgFileSize) == -1, FileWriteError,
//                         "unable to truncate '%s'", strPtr(pgFile));
//
//                     // Report the file as not copied
//                     result = false;
//                 }
//
//                 ioWriteClose(storageWriteIo(pgFileWrite));
//             }
//             // Else perform the copy
//             else
//             {
//                 IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(pgFileWrite));
//
//                 // Add decryption filter
//                 if (cipherPass != NULL)
//                 {
//                     ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));
//                     compressible = false;
//                 }
//
//                 // Add decompression filter
//                 if (repoFileCompressType != compressTypeNone)
//                 {
//                     ioFilterGroupAdd(filterGroup, decompressFilter(repoFileCompressType));
//                     compressible = false;
//                 }
//
//                 // Add sha1 filter
//                 ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));
//
//                 // Add size filter
//                 ioFilterGroupAdd(filterGroup, ioSizeNew());
//
//                 // Copy file
//                 storageCopyP(
//                     storageNewReadP(
//                         storageRepo(),
//                         strNewFmt(
//                             STORAGE_REPO_BACKUP "/%s/%s%s", strPtr(repoFileReference), strPtr(repoFile),
//                             strPtr(compressExtStr(repoFileCompressType))),
//                         .compressible = compressible),
//                     pgFileWrite);
//
//                 // Validate checksum
//                 if (!strEq(pgFileChecksum, varStr(ioFilterGroupResult(filterGroup, CRYPTO_HASH_FILTER_TYPE_STR))))
//                 {
//                     THROW_FMT(
//                         ChecksumError,
//                         "error restoring '%s': actual checksum '%s' does not match expected checksum '%s'", strPtr(pgFile),
//                         strPtr(varStr(ioFilterGroupResult(filterGroup, CRYPTO_HASH_FILTER_TYPE_STR))), strPtr(pgFileChecksum));
//                 }
//             }
//         }
//     }
//     MEM_CONTEXT_TEMP_END();
//
//     FUNCTION_LOG_RETURN(BOOL, result);
// }
