/***********************************************************************************************************************************
Verify Command

Verify contents of the repository.
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "command/archive/common.h"
#include "command/check/common.h"
#include "command/verify/file.h"
#include "command/verify/protocol.h"
#include "command/verify/verify.h"
#include "common/compress/helper.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/fdWrite.h"
#include "common/io/io.h"
#include "common/log.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "protocol/parallel.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define VERIFY_STATUS_OK                                            "ok"
#define VERIFY_STATUS_ERROR                                         "error"

/***********************************************************************************************************************************
Data Types and Structures
***********************************************************************************************************************************/
#define FUNCTION_LOG_VERIFY_ARCHIVE_RESULT_TYPE                                                                                    \
    VerifyArchiveResult
#define FUNCTION_LOG_VERIFY_ARCHIVE_RESULT_FORMAT(value, buffer, bufferSize)                                                       \
    objNameToLog(&value, "VerifyArchiveResult", buffer, bufferSize)

#define FUNCTION_LOG_VERIFY_BACKUP_RESULT_TYPE                                                                                     \
    VerifyBackupResult
#define FUNCTION_LOG_VERIFY_BACKUP_RESULT_FORMAT(value, buffer, bufferSize)                                                        \
    objNameToLog(&value, "VerifyBackupResult", buffer, bufferSize)

// Structure for verifying repository info files
typedef struct VerifyInfoFile
{
    InfoBackup *backup;                                             // Backup.info file contents
    InfoArchive *archive;                                           // Archive.info file contents
    Manifest *manifest;                                             // Manifest file contents
    const String *checksum;                                         // File checksum
    int errorCode;                                                  // Error code else 0 for no error
} VerifyInfoFile;

// Job data results structures for archive and backup
typedef struct VerifyArchiveResult
{
    String *archiveId;                                              // Archive Id (e.g. 9.6-1, 10-2)
    unsigned int totalWalFile;                                      // Total number of WAL files listed in directory on first read
    unsigned int totalValidWal;                                     // Total number of WAL that were verified and valid
    PgWal pgWalInfo;                                                // PG version, WAL size, system id
    List *walRangeList;                                             // List of WAL file ranges - new item is when WAL is missing
} VerifyArchiveResult;

// WAL range includes the start/stop of sequential WAL and start/stop includes the timeline (e.g. 000000020000000100000005)
typedef struct VerifyWalRange
{
    String *stop;                                                   // Last WAL segment in this sequential range
    String *start;                                                  // First WAL segment in this sequential range
    List *invalidFileList;                                          // After all jobs complete, list of VerifyInvalidFile
} VerifyWalRange;

// Invalid file information (not missing but files failing verification) - for archive and backup
typedef struct VerifyInvalidFile
{
    String *fileName;                                               // Name of the file (includes path within the stanza)
    VerifyResult reason;                                            // Reason file is invalid (e.g. incorrect checksum)
} VerifyInvalidFile;

// Status result of a backup
typedef enum
{
    backupValid,                                                    // Default: All files in backup label repo passed verification
    backupInvalid,                                                  // One of more files in backup label repo failed verification
    backupMissingManifest,                                          // Backup manifest missing (backup may have expired)
    backupInProgress,                                               // Backup appeared to be in progress (so was skipped)
} VerifyBackupResultStatus;

typedef struct VerifyBackupResult
{
    String *backupLabel;                                            // Label assigned to the backup
    VerifyBackupResultStatus status;                                // Final status of the backup
    bool fileVerifyComplete;                                        // Have all the files of the backup completed verification?
    unsigned int totalFileManifest;                                 // Total number of backup files in the manifest
    unsigned int totalFileVerify;                                   // Total number of backup files being verified
    unsigned int totalFileValid;                                    // Total number of backup files that were verified and valid
    String *backupPrior;                                            // Prior backup that this backup depends on, if any
    unsigned int pgId;                                              // PG id will be used to find WAL for the backup in the repo
    unsigned int pgVersion;                                         // PG version will be used with PG id to find WAL in the repo
    String *archiveStart;                                           // First WAL segment in the backup
    String *archiveStop;                                            // Last WAL segment in the backup
    List *invalidFileList;                                          // List of invalid files found in the backup
} VerifyBackupResult;

// Job data structure for processing and results collection
typedef struct VerifyJobData
{
    MemContext *memContext;                                         // Context for memory allocations in this struct
    StringList *archiveIdList;                                      // List of archive ids to verify
    StringList *walPathList;                                        // WAL path list for a single archive id
    StringList *walFileList;                                        // WAL file list for a single WAL path
    StringList *backupList;                                         // List of backups to verify
    Manifest *manifest;                                             // Manifest contents with list of files to verify
    unsigned int manifestFileIdx;                                   // Index of the file within the manifest file list to process
    String *currentBackup;                                          // In progress backup, if any
    const InfoPg *pgHistory;                                        // Database history list
    bool backupProcessing;                                          // Are we processing WAL or are we processing backups
    const String *manifestCipherPass;                               // Cipher pass for reading backup manifests
    const String *walCipherPass;                                    // Cipher pass for reading WAL files
    const String *backupCipherPass;                                 // Cipher pass for reading backup files referenced in a manifest
    unsigned int jobErrorTotal;                                     // Total errors that occurred during the job execution
    List *archiveIdResultList;                                      // Archive results
    List *backupResultList;                                         // Backup results
} VerifyJobData;

/***********************************************************************************************************************************
Helper function to add a file to an invalid file list
***********************************************************************************************************************************/
static void
verifyInvalidFileAdd(List *const invalidFileList, const VerifyResult reason, const String *const fileName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, invalidFileList);                 // Invalid file list to add the filename to
        FUNCTION_TEST_PARAM(ENUM, reason);                          // Reason for invalid file
        FUNCTION_TEST_PARAM(STRING, fileName);                      // Name of invalid file
    FUNCTION_TEST_END();

    ASSERT(invalidFileList != NULL);
    ASSERT(fileName != NULL);

    MEM_CONTEXT_BEGIN(lstMemContext(invalidFileList))
    {
        const VerifyInvalidFile invalidFile =
        {
            .fileName = strDup(fileName),
            .reason = reason,
        };

        lstAdd(invalidFileList, &invalidFile);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Load a file into memory
***********************************************************************************************************************************/
static StorageRead *
verifyFileLoad(const String *const pathFileName, const String *const cipherPass)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, pathFileName);                  // Fully qualified path/file name
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Password to open file if encrypted
    FUNCTION_TEST_END();

    ASSERT(pathFileName != NULL);

    // Read the file and error if missing
    StorageRead *const result = storageNewReadP(storageRepo(), pathFileName);

    // *read points to a location within result so update result with contents based on necessary filters
    IoRead *const read = storageReadIo(result);

    cipherBlockFilterGroupAdd(
        ioReadFilterGroup(read), cfgOptionStrId(cfgOptRepoCipherType), cipherModeDecrypt, cipherPass);
    ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(hashTypeSha1));

    // If the file is compressed, add a decompression filter
    if (compressTypeFromName(pathFileName) != compressTypeNone)
        ioFilterGroupAdd(ioReadFilterGroup(read), decompressFilterP(compressTypeFromName(pathFileName)));

    FUNCTION_TEST_RETURN(STORAGE_READ, result);
}

/***********************************************************************************************************************************
Get status of info files in the repository
***********************************************************************************************************************************/
static VerifyInfoFile
verifyInfoFile(const String *const pathFileName, const bool keepFile, const String *const cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pathFileName);                   // Fully qualified path/file name
        FUNCTION_LOG_PARAM(BOOL, keepFile);                         // Should the file be kept in memory?
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Password to open file if encrypted
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(pathFileName != NULL);

    VerifyInfoFile result = {.errorCode = 0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        TRY_BEGIN()
        {
            IoRead *const infoRead = storageReadIo(verifyFileLoad(pathFileName, cipherPass));

            // If directed to keep the loaded file in memory, then move the file into the result, else drain the io and close it
            if (keepFile)
            {
                if (strBeginsWith(pathFileName, INFO_BACKUP_PATH_FILE_STR))
                    result.backup = infoBackupMove(infoBackupNewLoad(infoRead), memContextPrior());
                else if (strBeginsWith(pathFileName, INFO_ARCHIVE_PATH_FILE_STR))
                    result.archive = infoArchiveMove(infoArchiveNewLoad(infoRead), memContextPrior());
                else
                    result.manifest = manifestMove(manifestNewLoad(infoRead), memContextPrior());
            }
            else
                ioReadDrain(infoRead);

            const Buffer *const filterResult = pckReadBinP(
                ioFilterGroupResultP(ioReadFilterGroup(infoRead), CRYPTO_HASH_FILTER_TYPE));

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.checksum = strNewEncode(encodingHex, filterResult);
            }
            MEM_CONTEXT_PRIOR_END();
        }
        CATCH_ANY()
        {
            result.errorCode = errorCode();
            String *const errorMsg = strCatZ(strNew(), errorMessage());

            if (result.errorCode == errorTypeCode(&ChecksumError))
                strCat(errorMsg, strNewFmt(" %s", strZ(pathFileName)));

            LOG_DETAIL(strZ(errorMsg));
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/***********************************************************************************************************************************
Get the archive.info file
***********************************************************************************************************************************/
static InfoArchive *
verifyArchiveInfoFile(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    InfoArchive *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the main info file
        const VerifyInfoFile verifyArchiveInfo = verifyInfoFile(
            INFO_ARCHIVE_PATH_FILE_STR, true, cfgOptionStrNull(cfgOptRepoCipherPass));

        // If the main file did not error, then report on the copy's status and check checksums
        if (verifyArchiveInfo.errorCode == 0)
        {
            result = verifyArchiveInfo.archive;
            infoArchiveMove(result, memContextPrior());

            // Attempt to load the copy and report on it's status but don't keep it in memory
            const VerifyInfoFile verifyArchiveInfoCopy = verifyInfoFile(
                INFO_ARCHIVE_PATH_FILE_COPY_STR, false, cfgOptionStrNull(cfgOptRepoCipherPass));

            // If the copy loaded successfully, then check the checksums
            if (verifyArchiveInfoCopy.errorCode == 0)
            {
                // If the info and info.copy checksums don't match each other than one (or both) of the files could be corrupt so
                // log a warning but must trust main
                if (!strEq(verifyArchiveInfo.checksum, verifyArchiveInfoCopy.checksum))
                    LOG_DETAIL("archive.info.copy does not match archive.info");
            }
        }
        else
        {
            // Attempt to load the copy
            const VerifyInfoFile verifyArchiveInfoCopy = verifyInfoFile(
                INFO_ARCHIVE_PATH_FILE_COPY_STR, true, cfgOptionStrNull(cfgOptRepoCipherPass));

            // If loaded successfully, then return the copy as usable
            if (verifyArchiveInfoCopy.errorCode == 0)
            {
                result = verifyArchiveInfoCopy.archive;
                infoArchiveMove(result, memContextPrior());
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INFO_ARCHIVE, result);
}

/***********************************************************************************************************************************
Get the backup.info file
***********************************************************************************************************************************/
static InfoBackup *
verifyBackupInfoFile(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    InfoBackup *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the main info file
        const VerifyInfoFile verifyBackupInfo = verifyInfoFile(
            INFO_BACKUP_PATH_FILE_STR, true, cfgOptionStrNull(cfgOptRepoCipherPass));

        // If the main file did not error, then report on the copy's status and check checksums
        if (verifyBackupInfo.errorCode == 0)
        {
            result = verifyBackupInfo.backup;
            infoBackupMove(result, memContextPrior());

            // Attempt to load the copy and report on it's status but don't keep it in memory
            const VerifyInfoFile verifyBackupInfoCopy = verifyInfoFile(
                INFO_BACKUP_PATH_FILE_COPY_STR, false, cfgOptionStrNull(cfgOptRepoCipherPass));

            // If the copy loaded successfully, then check the checksums
            if (verifyBackupInfoCopy.errorCode == 0)
            {
                // If the info and info.copy checksums don't match each other than one (or both) of the files could be corrupt so
                // log a warning but must trust main
                if (!strEq(verifyBackupInfo.checksum, verifyBackupInfoCopy.checksum))
                    LOG_DETAIL("backup.info.copy does not match backup.info");
            }
        }
        else
        {
            // Attempt to load the copy
            const VerifyInfoFile verifyBackupInfoCopy = verifyInfoFile(
                INFO_BACKUP_PATH_FILE_COPY_STR, true, cfgOptionStrNull(cfgOptRepoCipherPass));

            // If loaded successfully, then return the copy as usable
            if (verifyBackupInfoCopy.errorCode == 0)
            {
                result = verifyBackupInfoCopy.backup;
                infoBackupMove(result, memContextPrior());
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INFO_BACKUP, result);
}

/***********************************************************************************************************************************
Get the manifest file
***********************************************************************************************************************************/
static Manifest *
verifyManifestFile(
    VerifyBackupResult *const backupResult, const String *const cipherPass, bool currentBackup, const InfoPg *const pgHistory,
    unsigned int *const jobErrorTotal)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM_P(VERIFY_BACKUP_RESULT, backupResult);   // The result set for the backup being processed
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Passphrase to access the manifest file
        FUNCTION_LOG_PARAM(BOOL, currentBackup);                    // Is this possibly a backup currently in progress?
        FUNCTION_LOG_PARAM(INFO_PG, pgHistory);                     // Database history
        FUNCTION_LOG_PARAM_P(UINT, jobErrorTotal);                  // Pointer to the overall job error total
    FUNCTION_LOG_END();

    Manifest *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const fileName = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupResult->backupLabel));

        // Get the main manifest file
        const VerifyInfoFile verifyManifestInfo = verifyInfoFile(fileName, true, cipherPass);

        // If the main file did not error, then report on the copy's status and check checksums
        if (verifyManifestInfo.errorCode == 0)
        {
            result = verifyManifestInfo.manifest;

            // The current in-progress backup is only notional until the main file is checked because the backup may have
            // completed by the time the main manifest is checked here. So having a main manifest file means this backup is not
            // (or is no longer) the currentBackup.
            currentBackup = false;

            // Attempt to load the copy and report on it's status but don't keep it in memory
            const VerifyInfoFile verifyManifestInfoCopy = verifyInfoFile(
                strNewFmt("%s%s", strZ(fileName), INFO_COPY_EXT), false, cipherPass);

            // If the copy loaded successfully, then check the checksums
            if (verifyManifestInfoCopy.errorCode == 0)
            {
                // If the manifest and manifest.copy checksums don't match each other than one (or both) of the files could be
                // corrupt so log a warning but trust main
                if (!strEq(verifyManifestInfo.checksum, verifyManifestInfoCopy.checksum))
                    LOG_DETAIL_FMT("backup '%s' manifest.copy does not match manifest", strZ(backupResult->backupLabel));
            }
        }
        else
        {
            // If this might be an in-progress backup and the main manifest is simply missing, it is assumed the backup is an
            // actual in-progress backup and verification is skipped, otherwise, if the main is not simply missing, or this is not
            // an in-progress backup then attempt to load the copy.
            if (!(currentBackup && verifyManifestInfo.errorCode == errorTypeCode(&FileMissingError)))
            {
                currentBackup = false;

                const VerifyInfoFile verifyManifestInfoCopy = verifyInfoFile(
                    strNewFmt("%s%s", strZ(fileName), INFO_COPY_EXT), true, cipherPass);

                // If loaded successfully, then return the copy as usable
                if (verifyManifestInfoCopy.errorCode == 0)
                {
                    LOG_DETAIL_FMT("%s/backup.manifest is missing or unusable, using copy", strZ(backupResult->backupLabel));

                    result = verifyManifestInfoCopy.manifest;
                }
                else if (verifyManifestInfo.errorCode == errorTypeCode(&FileMissingError) &&
                         verifyManifestInfoCopy.errorCode == errorTypeCode(&FileMissingError))
                {
                    backupResult->status = backupMissingManifest;

                    LOG_DETAIL_FMT("manifest missing for '%s' - backup may have expired", strZ(backupResult->backupLabel));
                }
            }
            else
            {
                backupResult->status = backupInProgress;

                LOG_INFO_FMT("backup '%s' appears to be in progress, skipping", strZ(backupResult->backupLabel));
            }
        }

        // If found a usable manifest then check that the database it was based on is in the history
        if (result != NULL)
        {
            bool found = false;
            const ManifestData *const manData = manifestData(result);

            // Confirm the PG database information from the manifest is in the history list
            for (unsigned int infoPgIdx = 0; infoPgIdx < infoPgDataTotal(pgHistory); infoPgIdx++)
            {
                const InfoPgData pgHistoryData = infoPgData(pgHistory, infoPgIdx);

                if (pgHistoryData.id == manData->pgId && pgHistoryData.systemId == manData->pgSystemId &&
                    pgHistoryData.version == manData->pgVersion)
                {
                    found = true;
                    break;
                }
            }

            // If the PG data is not found in the backup.info history, then error and reset the result
            if (!found)
            {
                LOG_INFO_FMT(
                    "'%s' may not be recoverable - PG data (id %u, version %s, system-id %" PRIu64 ") is not in the backup.info"
                    " history, skipping",
                    strZ(backupResult->backupLabel), manData->pgId, strZ(pgVersionToStr(manData->pgVersion)), manData->pgSystemId);

                manifestFree(result);
                result = NULL;
            }
            else
                manifestMove(result, memContextPrior());
        }

        // If the result is NULL and the backup status has not yet been set, then the backup is unusable (invalid)
        if (result == NULL && backupResult->status == backupValid)
        {
            backupResult->status = backupInvalid;
            (*jobErrorTotal)++;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, result);
}

/***********************************************************************************************************************************
Check the history in the info files
***********************************************************************************************************************************/
static void
verifyPgHistory(const InfoPg *const archiveInfoPg, const InfoPg *const backupInfoPg)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_PG, archiveInfoPg);                // Postgres information from the archive.info file
        FUNCTION_TEST_PARAM(INFO_PG, backupInfoPg);                 // Postgres information from the backup.info file
    FUNCTION_TEST_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check archive.info and backup.info current PG data matches. If there is a mismatch, verify cannot continue since
        // the database is not considered accessible during the verify command so no way to tell which would be valid.
        const InfoPgData archiveInfoPgData = infoPgData(archiveInfoPg, infoPgDataCurrentId(archiveInfoPg));
        const InfoPgData backupInfoPgData = infoPgData(backupInfoPg, infoPgDataCurrentId(backupInfoPg));
        checkStanzaInfo(&archiveInfoPgData, &backupInfoPgData);

        const unsigned int archiveInfoHistoryTotal = infoPgDataTotal(archiveInfoPg);
        const unsigned int backupInfoHistoryTotal = infoPgDataTotal(backupInfoPg);

        String *const errMsg = strNewZ("archive and backup history lists do not match");

        if (archiveInfoHistoryTotal != backupInfoHistoryTotal)
            THROW(FormatError, strZ(errMsg));

        // Confirm the lists are the same
        for (unsigned int infoPgIdx = 0; infoPgIdx < archiveInfoHistoryTotal; infoPgIdx++)
        {
            const InfoPgData archiveInfoPgHistory = infoPgData(archiveInfoPg, infoPgIdx);
            const InfoPgData backupInfoPgHistory = infoPgData(backupInfoPg, infoPgIdx);

            if (archiveInfoPgHistory.id != backupInfoPgHistory.id ||
                archiveInfoPgHistory.systemId != backupInfoPgHistory.systemId ||
                archiveInfoPgHistory.version != backupInfoPgHistory.version)
            {
                THROW(FormatError, strZ(errMsg));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Populate the WAL ranges from the provided, sorted, WAL files list for a given archiveId
***********************************************************************************************************************************/
static void
verifyCreateArchiveIdRange(
    const VerifyArchiveResult *const archiveIdResult, StringList *const walFileList, unsigned int *const jobErrorTotal)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VERIFY_ARCHIVE_RESULT, archiveIdResult);  // The result set for the archive Id being processed
        FUNCTION_TEST_PARAM(STRING_LIST, walFileList);                  // Sorted (ascending) list of WAL files in a timeline
        FUNCTION_TEST_PARAM_P(UINT, jobErrorTotal);                     // Pointer to the overall job error total
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(archiveIdResult != NULL);
    ASSERT(walFileList != NULL);

    unsigned int walFileIdx = 0;

    // Initialize the WAL range
    VerifyWalRange *walRange = NULL;

    // If there is a WAL range for this archiveID, get the last one. If there is no timeline change then continue updating the last
    // WAL range.
    if (!lstEmpty(archiveIdResult->walRangeList) &&
        strEq(
            strSubN(((VerifyWalRange *)lstGetLast(archiveIdResult->walRangeList))->stop, 0, 8),
            strSubN(strSubN(strLstGet(walFileList, walFileIdx), 0, WAL_SEGMENT_NAME_SIZE), 0, 8)))
    {
        walRange = lstGetLast(archiveIdResult->walRangeList);
    }

    do
    {
        const String *const walSegment = strSubN(strLstGet(walFileList, walFileIdx), 0, WAL_SEGMENT_NAME_SIZE);

        // The lists are sorted so look ahead to see if this is a duplicate of the next one in the list
        if (walFileIdx + 1 < strLstSize(walFileList))
        {
            if (strEq(walSegment, strSubN(strLstGet(walFileList, walFileIdx + 1), 0, WAL_SEGMENT_NAME_SIZE)))
            {
                LOG_INFO_FMT("duplicate WAL '%s' for '%s' exists, skipping", strZ(walSegment), strZ(archiveIdResult->archiveId));

                (*jobErrorTotal)++;

                bool foundDup = true;

                // Remove all duplicates of this WAL, including this WAL, from the list
                while (walFileIdx < strLstSize(walFileList) && foundDup)
                {
                    if (strEq(walSegment, strSubN(strLstGet(walFileList, walFileIdx), 0, WAL_SEGMENT_NAME_SIZE)))
                        strLstRemoveIdx(walFileList, walFileIdx);
                    else
                        foundDup = false;
                }

                continue;
            }
        }

        // Initialize the range if it has not yet been initialized and continue to next
        if (walRange == NULL ||
            !strEq(
                walSegmentNext(walRange->stop, (size_t)archiveIdResult->pgWalInfo.size, archiveIdResult->pgWalInfo.version),
                walSegment))
        {
            // Add the initialized wal range to the range list
            MEM_CONTEXT_BEGIN(lstMemContext(archiveIdResult->walRangeList))
            {
                const VerifyWalRange walRangeNew =
                {
                    .start = strDup(walSegment),
                    .stop = strDup(walSegment),
                    .invalidFileList = lstNewP(sizeof(VerifyInvalidFile), .comparator = lstComparatorStr),
                };

                lstAdd(archiveIdResult->walRangeList, &walRangeNew);
            }
            MEM_CONTEXT_END();

            // Set the current wal range being processed to what was just added
            walRange = lstGetLast(archiveIdResult->walRangeList);
        }
        // If the next WAL is the appropriate distance away, then there is no gap
        else
        {
            MEM_CONTEXT_BEGIN(lstMemContext(archiveIdResult->walRangeList))
            {
                strFree(walRange->stop);
                walRange->stop = strDup(walSegment);
            }
            MEM_CONTEXT_END();
        }

        walFileIdx++;
    }
    while (walFileIdx < strLstSize(walFileList));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Return verify jobs for the archive
***********************************************************************************************************************************/
static ProtocolParallelJob *
verifyArchive(VerifyJobData *const jobData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, jobData);
    FUNCTION_TEST_END();

    ProtocolParallelJob *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Process archive files, if any
        while (!strLstEmpty(jobData->archiveIdList))
        {
            // Add archiveId to the result list if the list is empty or the last processed is not equal to the current archiveId
            if (lstEmpty(jobData->archiveIdResultList) ||
                !strEq(
                    ((VerifyArchiveResult *)lstGetLast(jobData->archiveIdResultList))->archiveId,
                    strLstGet(jobData->archiveIdList, 0)))
            {
                const String *const archiveId = strLstGet(jobData->archiveIdList, 0);

                MEM_CONTEXT_BEGIN(lstMemContext(jobData->archiveIdResultList))
                {
                    const VerifyArchiveResult archiveIdResult =
                    {
                        .archiveId = strDup(archiveId),
                        .walRangeList = lstNewP(sizeof(VerifyWalRange), .comparator = lstComparatorStr),
                    };

                    lstAdd(jobData->archiveIdResultList, &archiveIdResult);
                }
                MEM_CONTEXT_END();

                // Free the old WAL path list
                strLstFree(jobData->walPathList);

                // Get the WAL paths for the archive Id
                const String *const archiveIdPath = strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strZ(archiveId));

                MEM_CONTEXT_BEGIN(jobData->memContext)
                {
                    jobData->walPathList = strLstSort(
                        storageListP(storageRepo(), archiveIdPath, .expression = WAL_SEGMENT_DIR_REGEXP_STR), sortOrderAsc);
                }
                MEM_CONTEXT_END();
            }

            // If there are WAL paths then get the file lists
            if (!strLstEmpty(jobData->walPathList))
            {
                // Get the archive id info for the current (last) archive id being processed
                VerifyArchiveResult *const archiveResult = lstGetLast(jobData->archiveIdResultList);

                do
                {
                    const String *const walPath = strLstGet(jobData->walPathList, 0);

                    // Get the WAL files for the first item in the WAL paths list and initialize WAL info and ranges
                    if (strLstEmpty(jobData->walFileList))
                    {
                        // Free the old WAL file list
                        strLstFree(jobData->walFileList);

                        // Get WAL file list
                        const String *const walFilePath = strNewFmt(
                            STORAGE_REPO_ARCHIVE "/%s/%s", strZ(archiveResult->archiveId), strZ(walPath));

                        MEM_CONTEXT_BEGIN(jobData->memContext)
                        {
                            jobData->walFileList = strLstSort(
                                storageListP(storageRepo(), walFilePath, .expression = WAL_SEGMENT_FILE_REGEXP_STR), sortOrderAsc);
                        }
                        MEM_CONTEXT_END();

                        if (!strLstEmpty(jobData->walFileList))
                        {
                            if (archiveResult->pgWalInfo.size == 0)
                            {
                                // Initialize the WAL segment size from the first WAL
                                StorageRead *const walRead = verifyFileLoad(
                                    strNewFmt(
                                        STORAGE_REPO_ARCHIVE "/%s/%s/%s", strZ(archiveResult->archiveId), strZ(walPath),
                                        strZ(strLstGet(jobData->walFileList, 0))),
                                    jobData->walCipherPass);

                                const PgWal walInfo = pgWalFromBuffer(
                                    storageGetP(walRead, .exactSize = PG_WAL_HEADER_SIZE), cfgOptionStrNull(cfgOptPgVersionForce));

                                archiveResult->pgWalInfo.size = walInfo.size;
                                archiveResult->pgWalInfo.version = walInfo.version;
                            }

                            // Add total number of WAL files in the directory to the total WAL - this number will include
                            // duplicates, if any, that will be filtered out and not checked but will be reported as errors in the
                            // log
                            archiveResult->totalWalFile += strLstSize(jobData->walFileList);

                            verifyCreateArchiveIdRange(archiveResult, jobData->walFileList, &jobData->jobErrorTotal);
                        }
                    }

                    // If there are WAL files, then verify them
                    if (!strLstEmpty(jobData->walFileList))
                    {
                        // Get the fully qualified file name and checksum
                        const String *const fileName = strLstGet(jobData->walFileList, 0);
                        const String *const filePathName = strNewFmt(
                            STORAGE_REPO_ARCHIVE "/%s/%s/%s", strZ(archiveResult->archiveId), strZ(walPath), strZ(fileName));
                        const Buffer *const checksum = bufNewDecode(
                            encodingHex, strSubN(fileName, WAL_SEGMENT_NAME_SIZE + 1, HASH_TYPE_SHA1_SIZE_HEX));

                        // Set up the job
                        PackWrite *const param = protocolPackNew();

                        pckWriteStrP(param, filePathName);
                        pckWriteBoolP(param, false);
                        pckWriteU32P(param, compressTypeFromName(filePathName));
                        pckWriteBinP(param, checksum);
                        pckWriteU64P(param, archiveResult->pgWalInfo.size);
                        pckWriteStrP(param, jobData->walCipherPass);

                        // Assign job to result, prepending the archiveId to the key for consistency with backup processing
                        const String *const jobKey = strNewFmt("%s/%s", strZ(archiveResult->archiveId), strZ(filePathName));

                        MEM_CONTEXT_PRIOR_BEGIN()
                        {
                            result = protocolParallelJobNew(VARSTR(jobKey), PROTOCOL_COMMAND_VERIFY_FILE, param);
                        }
                        MEM_CONTEXT_PRIOR_END();

                        // Remove the file to process from the list
                        strLstRemoveIdx(jobData->walFileList, 0);

                        // If this is the last file to process for this timeline, then remove the path
                        if (strLstEmpty(jobData->walFileList))
                            strLstRemoveIdx(jobData->walPathList, 0);
                    }
                    else
                    {
                        // No valid WAL to process (may be only duplicates or nothing in WAL path) - remove WAL path from the list
                        LOG_DETAIL_FMT(
                            "path '%s/%s' does not contain any valid WAL to be processed", strZ(archiveResult->archiveId),
                            strZ(walPath));
                        strLstRemoveIdx(jobData->walPathList, 0);
                    }

                    // If a job was found to be processed then break out to process it
                    if (result != NULL)
                        break;
                }
                while (!strLstEmpty(jobData->walPathList));

                // If this is the last timeline to process for this archiveId, then remove the archiveId
                if (strLstEmpty(jobData->walPathList))
                    strLstRemoveIdx(jobData->archiveIdList, 0);

                // If a file was sent to be processed then break so we can process it
                if (result != NULL)
                    break;
            }
            else
            {
                // Log that no WAL paths exist in the archive Id dir - remove the archive Id from the list (nothing to process)
                LOG_DETAIL_FMT("archive path '%s' is empty", strZ(strLstGet(jobData->archiveIdList, 0)));
                strLstRemoveIdx(jobData->archiveIdList, 0);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(PROTOCOL_PARALLEL_JOB, result);
}

/***********************************************************************************************************************************
Verify the job data backups
***********************************************************************************************************************************/
static ProtocolParallelJob *
verifyBackup(VerifyJobData *const jobData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, jobData);
    FUNCTION_TEST_END();

    ProtocolParallelJob *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Process backup files, if any
        while (!strLstEmpty(jobData->backupList))
        {
            // If result list is empty or the last processed is not equal to the backup being processed, then initialize the backup
            // data and results
            if (lstEmpty(jobData->backupResultList) ||
                !strEq(
                    ((VerifyBackupResult *)lstGetLast(jobData->backupResultList))->backupLabel, strLstGet(jobData->backupList, 0)))
            {
                MEM_CONTEXT_BEGIN(lstMemContext(jobData->backupResultList))
                {
                    const VerifyBackupResult backupResultNew =
                    {
                        .backupLabel = strDup(strLstGet(jobData->backupList, 0)),
                        .invalidFileList = lstNewP(sizeof(VerifyInvalidFile), .comparator = lstComparatorStr),
                    };

                    // Add the backup to the result list
                    lstAdd(jobData->backupResultList, &backupResultNew);
                }
                MEM_CONTEXT_END();

                // Get the result just added so it can be updated directly
                VerifyBackupResult *const backupResult = lstGetLast(jobData->backupResultList);

                // If currentBackup is set (meaning the newest backup label on disk was not in the db:current section when the
                // backup.info file was read) and this is the same label, then set inProgressBackup to true, else false.
                // inProgressBackup may be changed in verifyManifestFile if a main backup.manifest exists since that would indicate
                // the backup completed during the verify process.
                const bool inProgressBackup = strEq(jobData->currentBackup, backupResult->backupLabel);

                // Get a usable backup manifest file
                Manifest *const manifest = verifyManifestFile(
                    backupResult, jobData->manifestCipherPass, inProgressBackup, jobData->pgHistory, &jobData->jobErrorTotal);

                // If a usable backup.manifest file is not found
                if (manifest == NULL)
                {
                    // Remove this backup from the processing list
                    strLstRemoveIdx(jobData->backupList, 0);

                    // No files to process so continue to the next backup in the list
                    continue;
                }
                // Initialize the backup results and manifest for processing
                else
                {
                    // Move the manifest to the jobData for processing
                    jobData->manifest = manifestMove(manifest, jobData->memContext);

                    // Initialize the jobData
                    MEM_CONTEXT_BEGIN(jobData->memContext)
                    {
                        // Get the cipher subpass used to decrypt files in the backup and initialize the file list index
                        jobData->backupCipherPass = strDup(manifestCipherSubPass(jobData->manifest));
                        jobData->manifestFileIdx = 0;
                    }
                    MEM_CONTEXT_END();

                    const ManifestData *const manData = manifestData(jobData->manifest);

                    MEM_CONTEXT_BEGIN(lstMemContext(jobData->backupResultList))
                    {
                        backupResult->totalFileManifest = manifestFileTotal(jobData->manifest);
                        backupResult->backupPrior = strDup(manData->backupLabelPrior);
                        backupResult->pgId = manData->pgId;
                        backupResult->pgVersion = manData->pgVersion;
                        backupResult->archiveStart = strDup(manData->archiveStart);
                        backupResult->archiveStop = strDup(manData->archiveStop);
                    }
                    MEM_CONTEXT_END();
                }
            }

            VerifyBackupResult *const backupResult = lstGetLast(jobData->backupResultList);

            // Process any files in the manifest
            if (jobData->manifestFileIdx < manifestFileTotal(jobData->manifest))
            {
                do
                {
                    const ManifestFile fileData = manifestFile(jobData->manifest, jobData->manifestFileIdx);

                    // Track the files verified in order to determine when the processing of the backup is complete
                    backupResult->totalFileVerify++;

                    // Check the file if it is not zero-length or not bundled
                    if (fileData.size != 0 || !manifestData(jobData->manifest)->bundle)
                    {
                        // Check if the file is referenced in a prior backup
                        const String *fileBackupLabel = NULL;

                        if (fileData.reference != NULL)
                        {
                            // If the prior backup is not in the result list, then that backup was never processed (likely due to
                            // the --set option) so verify the file
                            const unsigned int backupPriorIdx = lstFindIdx(jobData->backupResultList, &fileData.reference);

                            if (backupPriorIdx == LIST_NOT_FOUND)
                            {
                                fileBackupLabel = fileData.reference;
                            }
                            // Else the backup this file references has a result so check the processing state for the referenced
                            // backup
                            else
                            {
                                const VerifyBackupResult *const backupResultPrior = lstGet(
                                    jobData->backupResultList, backupPriorIdx);

                                // If the verify-state of the backup is not complete then verify the file
                                if (!backupResultPrior->fileVerifyComplete)
                                {
                                    fileBackupLabel = fileData.reference;
                                }
                                // Else skip verification
                                else
                                {
                                    const String *const priorFile = strNewFmt(
                                        "%s/%s%s", strZ(fileData.reference), strZ(fileData.name),
                                        strZ(compressExtStr((manifestData(jobData->manifest))->backupOptionCompressType)));
                                    const unsigned int backupPriorInvalidIdx = lstFindIdx(
                                        backupResultPrior->invalidFileList, &priorFile);

                                    // If the file is in the invalid file list of the prior backup where it is referenced then add
                                    // the file as invalid to this backup result and set the backup result status; since already
                                    // logged an error on this file, don't log again
                                    if (backupPriorInvalidIdx != LIST_NOT_FOUND)
                                    {
                                        VerifyInvalidFile *invalidFile = lstGet(
                                            backupResultPrior->invalidFileList, backupPriorInvalidIdx);
                                        verifyInvalidFileAdd(
                                            backupResult->invalidFileList, invalidFile->reason, invalidFile->fileName);
                                        backupResult->status = backupInvalid;
                                    }
                                    // Else file in prior backup was valid so increment total valid files for this backup
                                    else
                                    {
                                        backupResult->totalFileValid++;
                                    }
                                }
                            }
                        }
                        // Else file is not referenced in a prior backup
                        else
                            fileBackupLabel = backupResult->backupLabel;

                        // If backup label is not null then send it off for processing
                        if (fileBackupLabel != NULL)
                        {
                            // Set up the job
                            PackWrite *const param = protocolPackNew();

                            const String *const filePathName = backupFileRepoPathP(
                                fileBackupLabel, .manifestName = fileData.name, .bundleId = fileData.bundleId,
                                .compressType = manifestData(jobData->manifest)->backupOptionCompressType,
                                .blockIncr = fileData.blockIncrMapSize != 0);

                            pckWriteStrP(param, filePathName);

                            if (fileData.bundleId != 0)
                            {
                                pckWriteBoolP(param, true);
                                pckWriteU64P(param, fileData.bundleOffset);
                                pckWriteU64P(param, fileData.sizeRepo);
                            }
                            else
                                pckWriteBoolP(param, false);

                            // Use the repo checksum when present
                            if (fileData.checksumRepoSha1 != NULL)
                            {
                                pckWriteU32P(param, compressTypeNone);
                                pckWriteBinP(param, BUF(fileData.checksumRepoSha1, HASH_TYPE_SHA1_SIZE));
                                pckWriteU64P(param, fileData.sizeRepo);
                                pckWriteStrP(param, NULL);
                            }
                            // Else use the file checksum, which may require additional filters, e.g. decompression
                            else
                            {
                                pckWriteU32P(param, manifestData(jobData->manifest)->backupOptionCompressType);
                                pckWriteBinP(param, BUF(fileData.checksumSha1, HASH_TYPE_SHA1_SIZE));
                                pckWriteU64P(param, fileData.size);
                                pckWriteStrP(param, jobData->backupCipherPass);
                            }

                            // Assign job to result (prepend backup label being processed to the key since some files are in a prior
                            // backup)
                            const String *const jobKey = strNewFmt("%s/%s", strZ(backupResult->backupLabel), strZ(filePathName));

                            MEM_CONTEXT_PRIOR_BEGIN()
                            {
                                result = protocolParallelJobNew(VARSTR(jobKey), PROTOCOL_COMMAND_VERIFY_FILE, param);
                            }
                            MEM_CONTEXT_PRIOR_END();
                        }
                    }
                    // Else mark the zero-length file as valid
                    else
                        backupResult->totalFileValid++;

                    // Increment the index to point to the next file
                    jobData->manifestFileIdx++;

                    // If this was the last file to process for this backup, then free the manifest and remove this backup from the
                    // processing list
                    if (jobData->manifestFileIdx == backupResult->totalFileManifest)
                    {
                        manifestFree(jobData->manifest);
                        jobData->manifest = NULL;
                        strLstRemoveIdx(jobData->backupList, 0);
                    }

                    // If a job was found to be processed then break out to process it
                    if (result != NULL)
                        break;
                }
                while (jobData->manifestFileIdx < backupResult->totalFileManifest);
            }
            else
            {
                // Nothing to process so report an error, free the manifest, set the status, and remove the backup from processing
                // list
                LOG_INFO_FMT("backup '%s' manifest does not contain any target files to verify", strZ(backupResult->backupLabel));

                jobData->jobErrorTotal++;

                manifestFree(jobData->manifest);
                jobData->manifest = NULL;

                backupResult->status = backupInvalid;

                strLstRemoveIdx(jobData->backupList, 0);
            }

            // If a job was found to be processed then break out to process it
            if (result != NULL)
                break;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(PROTOCOL_PARALLEL_JOB, result);
}

/***********************************************************************************************************************************
Process the job data
***********************************************************************************************************************************/
static ProtocolParallelJob *
verifyJobCallback(void *const data, const unsigned int clientIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);                          // Pointer to the job data
        (void)clientIdx;                                            // Client index (not used for this process)
    FUNCTION_TEST_END();

    ASSERT(data != NULL);

    // Initialize the result
    ProtocolParallelJob *result = NULL;
    VerifyJobData *const jobData = data;

    if (!jobData->backupProcessing)
    {
        result = verifyArchive(jobData);

        // Set the backupProcessing flag if the archive processing is finished so backup processing can begin immediately after
        jobData->backupProcessing = strLstEmpty(jobData->archiveIdList);
    }

    if (jobData->backupProcessing)
    {
        // Only begin backup verification if the last archive result was processed
        if (result == NULL)
            result = verifyBackup(jobData);
    }

    FUNCTION_TEST_RETURN(PROTOCOL_PARALLEL_JOB, result);
}

/***********************************************************************************************************************************
Helper function for returning a string corresponding to the result code
***********************************************************************************************************************************/
static const char *
verifyErrorMsg(const VerifyResult verifyResult)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, verifyResult);                    // Result code from the verifyFile() function
    FUNCTION_TEST_END();

    ASSERT(verifyResult != verifyOk);

    switch (verifyResult)
    {
        case verifyFileMissing:
            FUNCTION_TEST_RETURN_CONST(STRINGZ, "file missing");

        case verifyChecksumMismatch:
            FUNCTION_TEST_RETURN_CONST(STRINGZ, "invalid checksum");

        case verifySizeInvalid:
            FUNCTION_TEST_RETURN_CONST(STRINGZ, "invalid size");

        default:
            ASSERT(verifyResult == verifyOtherError);
            FUNCTION_TEST_RETURN_CONST(STRINGZ, "invalid result");
    }
}

/***********************************************************************************************************************************
Helper function to output a log message based on job result that is not verifyOk and return an error count
***********************************************************************************************************************************/
static unsigned int
verifyLogInvalidResult(
    const String *const fileType, const VerifyResult verifyResult, const unsigned int processId, const String *const filePathName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, fileType);                      // Indicates archive or backup file
        FUNCTION_TEST_PARAM(ENUM, verifyResult);                    // Result code from the verifyFile() function
        FUNCTION_TEST_PARAM(UINT, processId);                       // Process Id reporting the result
        FUNCTION_TEST_PARAM(STRING, filePathName);                  // File for which results are being reported
    FUNCTION_TEST_END();

    ASSERT(fileType != NULL);
    ASSERT(filePathName != NULL);

    // Log a warning because the WAL may have gone missing if expire came through and removed it
    // legitimately so it is not necessarily an error so the jobErrorTotal should not be incremented
    if (strEq(fileType, STORAGE_REPO_ARCHIVE_STR) && verifyResult == verifyFileMissing)
    {
        LOG_WARN_PID_FMT(processId, "%s '%s'", verifyErrorMsg(verifyResult), strZ(filePathName));
        FUNCTION_TEST_RETURN(UINT, 0);
    }

    LOG_INFO_PID_FMT(processId, "%s '%s'", verifyErrorMsg(verifyResult), strZ(filePathName));
    FUNCTION_TEST_RETURN(UINT, 1);
}

/***********************************************************************************************************************************
Helper function to set the currently processing backup label, if any, and check that the archiveIds are in the db history
***********************************************************************************************************************************/
static String *
verifySetBackupCheckArchive(
    const StringList *const backupList, const InfoBackup *const backupInfo, const StringList *const archiveIdList,
    const InfoPg *const pgHistory, unsigned int *const jobErrorTotal)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, backupList);               // List of backup labels in the backup directory
        FUNCTION_TEST_PARAM(INFO_BACKUP, backupInfo);               // Contents of the backup.info file
        FUNCTION_TEST_PARAM(STRING_LIST, archiveIdList);            // List of archiveIds in the archive directory
        FUNCTION_TEST_PARAM(INFO_PG, pgHistory);                    // Pointer to InfoPg of archive.info for accessing PG history
        FUNCTION_TEST_PARAM_P(UINT, jobErrorTotal);                 // Pointer to overall job error total
    FUNCTION_TEST_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If there are backups, set the last backup as current if it is not in backup.info - if it is, then it is complete, else
        // it will be checked later
        if (!strLstEmpty(backupList))
        {
            // Get the last backup as current if it is not in backup.info current list
            const String *const backupLabel = strLstGet(backupList, strLstSize(backupList) - 1);

            if (!infoBackupLabelExists(backupInfo, backupLabel))
            {
                // Duplicate the string into the prior context
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = strDup(backupLabel);
                }
                MEM_CONTEXT_PRIOR_END();
            }
        }

        // If there are archive directories on disk, make sure they are in the database history list
        if (!strLstEmpty(archiveIdList))
        {
            StringList *const archiveIdHistoryList = strLstNew();

            for (unsigned int histIdx = 0; histIdx < infoPgDataTotal(pgHistory); histIdx++)
                strLstAdd(archiveIdHistoryList, infoPgArchiveId(pgHistory, histIdx));

            // Sort the history list
            strLstSort(strLstComparatorSet(archiveIdHistoryList, archiveIdComparator), sortOrderAsc);

            String *const missingFromHistory = strNew();

            // Check if the archiveId on disk exists in the archive.info history list and report it if not
            for (unsigned int archiveIdx = 0; archiveIdx < strLstSize(archiveIdList); archiveIdx++)
            {
                const String *const archiveId = strLstGet(archiveIdList, archiveIdx);

                if (!strLstExists(archiveIdHistoryList, archiveId))
                    strCat(missingFromHistory, (strEmpty(missingFromHistory) ? archiveId : strNewFmt(", %s", strZ(archiveId))));
            }

            if (!strEmpty(missingFromHistory))
            {
                LOG_INFO_FMT("archiveIds '%s' are not in the archive.info history list", strZ(missingFromHistory));

                (*jobErrorTotal)++;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Add the file to the invalid file list for the range in which it exists
***********************************************************************************************************************************/
static void
verifyAddInvalidWalFile(
    const List *const walRangeList, const VerifyResult fileResult, const String *const fileName, const String *const walSegment)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, walRangeList);                    // List of WAL ranges for an archive Id
        FUNCTION_TEST_PARAM(UINT, fileResult);                      // Result of verifyFile()
        FUNCTION_TEST_PARAM(STRING, fileName);                      // File name (without the REPO prefix)
        FUNCTION_TEST_PARAM(STRING, walSegment);                    // WAL segment, i.e. 000000010000000000000005
    FUNCTION_TEST_END();

    ASSERT(walRangeList != NULL);
    ASSERT(fileName != NULL);
    ASSERT(walSegment != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        for (unsigned int walIdx = 0; walIdx < lstSize(walRangeList); walIdx++)
        {
            const VerifyWalRange *const walRange = lstGet(walRangeList, walIdx);

            // If the WAL segment is less/equal to the stop file then it falls in this range since ranges are sorted by stop file in
            // ascending order, therefore first one found is the range
            if (strCmp(walRange->stop, walSegment) >= 0)
            {
                // Add the file to the range where it was found and exit the loop
                verifyInvalidFileAdd(walRange->invalidFileList, fileResult, fileName);
                break;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Create file errors string
***********************************************************************************************************************************/
static String *
verifyCreateFileErrorsStr(
    const unsigned int errMissing, const unsigned int errChecksum, const unsigned int errSize, const unsigned int errOther,
    const bool verboseText)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, errMissing);                      // Number of files missing
        FUNCTION_TEST_PARAM(UINT, errChecksum);                     // Number of files with checksum errors
        FUNCTION_TEST_PARAM(UINT, errSize);                         // Number of files with invalid size
        FUNCTION_TEST_PARAM(UINT, errOther);                        // Number of files with other errors
        FUNCTION_TEST_PARAM(BOOL, verboseText);                     // Is verbose output requested
    FUNCTION_TEST_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // List all if verbose text, otherwise only list if type has errors
        strCatFmt(
            result, "\n    %s%s%s%s",
            verboseText || errMissing ? zNewFmt("missing: %u, ", errMissing) : "",
            verboseText || errChecksum ? zNewFmt("checksum invalid: %u, ", errChecksum) : "",
            verboseText || errSize ? zNewFmt("size invalid: %u, ", errSize) : "",
            verboseText || errOther ? zNewFmt("other: %u", errOther) : "");

        // Clean up trailing comma when necessary
        if (strEndsWithZ(result, ", "))
            strTruncIdx(result, (int)strSize(result) - 2);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Render the results of the verify command
***********************************************************************************************************************************/
static String *
verifyRender(const List *const archiveIdResultList, const List *const backupResultList, const bool verboseText)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, archiveIdResultList);             // Result list for all archive Ids in the repo
        FUNCTION_TEST_PARAM(LIST, backupResultList);                // Result list for all backups in the repo
        FUNCTION_TEST_PARAM(BOOL, verboseText);                     // Is verbose output requested?
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(archiveIdResultList != NULL);
    ASSERT(backupResultList != NULL);

    String *const result = strNew();

    // Render archive results
    if (verboseText && lstEmpty(archiveIdResultList))
        strCatZ(result, "\n  archiveId: none found");
    else
    {
        for (unsigned int archiveIdx = 0; archiveIdx < lstSize(archiveIdResultList); archiveIdx++)
        {
            const VerifyArchiveResult *const archiveIdResult = lstGet(archiveIdResultList, archiveIdx);

            if (verboseText || archiveIdResult->totalWalFile - archiveIdResult->totalValidWal != 0)
            {
                strCatFmt(
                    result, "\n  archiveId: %s, total WAL checked: %u, total valid WAL: %u", strZ(archiveIdResult->archiveId),
                    archiveIdResult->totalWalFile, archiveIdResult->totalValidWal);
            }

            if (archiveIdResult->totalWalFile > 0)
            {
                unsigned int errMissing = 0;
                unsigned int errChecksum = 0;
                unsigned int errSize = 0;
                unsigned int errOther = 0;

                for (unsigned int walIdx = 0; walIdx < lstSize(archiveIdResult->walRangeList); walIdx++)
                {
                    const VerifyWalRange *const walRange = lstGet(archiveIdResult->walRangeList, walIdx);

                    LOG_DETAIL_FMT(
                        "archiveId: %s, wal start: %s, wal stop: %s", strZ(archiveIdResult->archiveId), strZ(walRange->start),
                        strZ(walRange->stop));

                    unsigned int invalidIdx = 0;

                    while (invalidIdx < lstSize(walRange->invalidFileList))
                    {
                        VerifyInvalidFile *invalidFile = lstGet(walRange->invalidFileList, invalidIdx);

                        if (invalidFile->reason == verifyFileMissing)
                            errMissing++;
                        else if (invalidFile->reason == verifyChecksumMismatch)
                            errChecksum++;
                        else if (invalidFile->reason == verifySizeInvalid)
                            errSize++;
                        else
                            errOther++;

                        invalidIdx++;
                    }
                }

                // Create/append file errors string
                if (verboseText || errMissing + errChecksum + errSize + errOther > 0)
                    strCat(result, verifyCreateFileErrorsStr(errMissing, errChecksum, errSize, errOther, verboseText));
            }
        }
    }

    // Render backup results
    if (verboseText && lstEmpty(backupResultList))
        strCatZ(result, "\n  backup: none found");
    else
    {
        for (unsigned int backupIdx = 0; backupIdx < lstSize(backupResultList); backupIdx++)
        {
            const VerifyBackupResult *const backupResult = lstGet(backupResultList, backupIdx);
            const char *status;

            switch (backupResult->status)
            {
                case backupInvalid:
                    status = "invalid";
                    break;

                case backupMissingManifest:
                    status = "manifest missing";
                    break;

                case backupInProgress:
                    status = "in-progress";
                    break;

                default:
                {
                    ASSERT(backupResult->status == backupValid);

                    status = "valid";
                    break;
                }
            }

            if (verboseText || (strcmp(status, "valid") != 0 && strcmp(status, "in-progress") != 0))
            {
                strCatFmt(
                    result, "\n  backup: %s, status: %s, total files checked: %u, total valid files: %u",
                    strZ(backupResult->backupLabel), status, backupResult->totalFileVerify, backupResult->totalFileValid);
            }

            if (backupResult->totalFileVerify > 0)
            {
                unsigned int errMissing = 0;
                unsigned int errChecksum = 0;
                unsigned int errSize = 0;
                unsigned int errOther = 0;

                for (unsigned int invalidIdx = 0; invalidIdx < lstSize(backupResult->invalidFileList); invalidIdx++)
                {
                    const VerifyInvalidFile *const invalidFile = lstGet(backupResult->invalidFileList, invalidIdx);

                    if (invalidFile->reason == verifyFileMissing)
                        errMissing++;
                    else if (invalidFile->reason == verifyChecksumMismatch)
                        errChecksum++;
                    else if (invalidFile->reason == verifySizeInvalid)
                        errSize++;
                    else
                        errOther++;
                }

                // Create/append file errors string
                if (verboseText || errMissing + errChecksum + errSize + errOther > 0)
                    strCat(result, verifyCreateFileErrorsStr(errMissing, errChecksum, errSize, errOther, verboseText));
            }
        }
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Process the verify command
***********************************************************************************************************************************/
static String *
verifyProcess(const bool verboseText)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, verboseText);                      // Is verbose output requested?
    FUNCTION_LOG_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        unsigned int errorTotal = 0;
        String *resultStr = strNew();

        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        const Storage *const storage = storageRepo();

        // Get a usable backup info file
        const InfoBackup *const backupInfo = verifyBackupInfoFile();

        // If a usable backup.info file is not found, then report an error in the log
        if (backupInfo == NULL)
        {
            strCatZ(resultStr, "\n  No usable backup.info file");
            errorTotal++;
        }

        // Get a usable archive info file
        const InfoArchive *const archiveInfo = verifyArchiveInfoFile();

        // If a usable archive.info file is not found, then report an error in the log
        if (archiveInfo == NULL)
        {
            strCatZ(resultStr, "\n  No usable archive.info file");
            errorTotal++;
        }

        // If both a usable archive info and backup info file were found, then proceed with verification
        if (archiveInfo != NULL && backupInfo != NULL)
        {
            TRY_BEGIN()
            {
                // Verify that the archive.info and backup.info current database info and history lists are the same
                verifyPgHistory(infoArchivePg(archiveInfo), infoBackupPg(backupInfo));
            }
            CATCH_ANY()
            {
                strCatFmt(resultStr, "\n%s", errorMessage());
                errorTotal++;
            }
            TRY_END();
        }

        // If valid info files, then begin process of checking backups and archives in the repo
        if (errorTotal == 0)
        {
            // Initialize the job data
            VerifyJobData jobData =
            {
                .memContext = memContextCurrent(),
                .walPathList = NULL,
                .walFileList = strLstNew(),
                .pgHistory = infoArchivePg(archiveInfo),
                .manifestCipherPass = infoPgCipherPass(infoBackupPg(backupInfo)),
                .walCipherPass = infoPgCipherPass(infoArchivePg(archiveInfo)),
                .archiveIdResultList = lstNewP(sizeof(VerifyArchiveResult), .comparator = archiveIdComparator),
                .backupResultList = lstNewP(sizeof(VerifyBackupResult), .comparator = lstComparatorStr),
            };

            // Get a list of backups in the repo sorted ascending
            jobData.backupList = strLstSort(
                storageListP(
                    storage, STORAGE_REPO_BACKUP_STR,
                    .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
                sortOrderAsc);

            // Get a list of archive Ids in the repo (e.g. 9.6-1, 10-2, etc) sorted ascending by the db-id (number after the dash)
            jobData.archiveIdList = strLstSort(
                strLstComparatorSet(
                    storageListP(storage, STORAGE_REPO_ARCHIVE_STR, .expression = STRDEF(REGEX_ARCHIVE_DIR_DB_VERSION)),
                    archiveIdComparator),
                sortOrderAsc);

            // Only begin processing if there are some archives or backups in the repo
            if (!strLstEmpty(jobData.archiveIdList) || !strLstEmpty(jobData.backupList))
            {
                // Warn if there are no archives or there are no backups in the repo so that the callback need not try to
                // distinguish between having processed all of the list or if the list was missing in the first place
                if (strLstEmpty(jobData.archiveIdList) || strLstEmpty(jobData.backupList))
                    LOG_DETAIL_FMT("no %s exist in the repo", strLstEmpty(jobData.archiveIdList) ? "archives" : "backups");

                // If there are no archives to process, then set the processing flag to skip to processing the backups
                if (strLstEmpty(jobData.archiveIdList))
                    jobData.backupProcessing = true;

                // Set current backup if there is one and verify the archive history on disk is in the database history
                jobData.currentBackup = verifySetBackupCheckArchive(
                    jobData.backupList, backupInfo, jobData.archiveIdList, jobData.pgHistory, &jobData.jobErrorTotal);

                // Create the parallel executor
                ProtocolParallel *const parallelExec = protocolParallelNew(
                    cfgOptionUInt64(cfgOptProtocolTimeout) / 2, verifyJobCallback, &jobData);

                for (unsigned int processIdx = 1; processIdx <= cfgOptionUInt(cfgOptProcessMax); processIdx++)
                    protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypeRepo, 0, processIdx));

                // Process jobs
                MEM_CONTEXT_TEMP_RESET_BEGIN()
                {
                    do
                    {
                        unsigned int completed = protocolParallelProcess(parallelExec);

                        // Process completed jobs
                        for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
                        {
                            // Get the job and job key
                            ProtocolParallelJob *const job = protocolParallelResult(parallelExec);
                            const unsigned int processId = protocolParallelJobProcessId(job);
                            StringList *const filePathLst = strLstNewSplit(varStr(protocolParallelJobKey(job)), FSLASH_STR);

                            // Remove the result and file type identifier and recreate the path file name
                            const String *const resultId = strLstGet(filePathLst, 0);
                            strLstRemoveIdx(filePathLst, 0);
                            const String *const fileType = strLstGet(filePathLst, 0);
                            strLstRemoveIdx(filePathLst, 0);
                            const String *const filePathName = strLstJoin(filePathLst, "/");

                            // Initialize the result sets
                            VerifyArchiveResult *archiveIdResult = NULL;
                            VerifyBackupResult *backupResult = NULL;

                            // Get archiveId result data
                            if (strEq(fileType, STORAGE_REPO_ARCHIVE_STR))
                            {
                                // Find the archiveId in the list - assert if not found since this should never happen
                                const unsigned int index = lstFindIdx(jobData.archiveIdResultList, &resultId);
                                ASSERT(index != LIST_NOT_FOUND);

                                archiveIdResult = lstGet(jobData.archiveIdResultList, index);
                            }
                            // Else get the backup result data
                            else
                            {
                                const unsigned int index = lstFindIdx(jobData.backupResultList, &resultId);
                                ASSERT(index != LIST_NOT_FOUND);

                                backupResult = lstGet(jobData.backupResultList, index);
                            }

                            // The job was successful
                            if (protocolParallelJobErrorCode(job) == 0)
                            {
                                const VerifyResult verifyResult = (VerifyResult)pckReadU32P(protocolParallelJobResult(job));

                                // Update the result set for the type of file being processed
                                if (strEq(fileType, STORAGE_REPO_ARCHIVE_STR))
                                {
                                    if (verifyResult == verifyOk)
                                        archiveIdResult->totalValidWal++;
                                    else
                                    {
                                        jobData.jobErrorTotal += verifyLogInvalidResult(
                                            fileType, verifyResult, processId, filePathName);

                                        // Add invalid file to the WAL range
                                        verifyAddInvalidWalFile(
                                            archiveIdResult->walRangeList, verifyResult, filePathName,
                                            strSubN(
                                                strLstGet(filePathLst, strLstSize(filePathLst) - 1), 0, WAL_SEGMENT_NAME_SIZE));
                                    }
                                }
                                else
                                {
                                    if (verifyResult == verifyOk)
                                        backupResult->totalFileValid++;
                                    else
                                    {
                                        jobData.jobErrorTotal += verifyLogInvalidResult(
                                            fileType, verifyResult, processId, filePathName);
                                        backupResult->status = backupInvalid;
                                        verifyInvalidFileAdd(backupResult->invalidFileList, verifyResult, filePathName);
                                    }
                                }
                            }
                            // Else the job errored
                            else
                            {
                                // Log a protocol error and increment the jobErrorTotal
                                LOG_INFO_PID_FMT(
                                    processId,
                                    "%s %s: [%d] %s", verifyErrorMsg(verifyOtherError), strZ(filePathName),
                                    protocolParallelJobErrorCode(job), strZ(protocolParallelJobErrorMessage(job)));

                                jobData.jobErrorTotal++;

                                // Add invalid file with "OtherError" reason to invalid file list
                                if (strEq(fileType, STORAGE_REPO_ARCHIVE_STR))
                                {
                                    // Add invalid file to the WAL range
                                    verifyAddInvalidWalFile(
                                        archiveIdResult->walRangeList, verifyOtherError, filePathName,
                                        strSubN(strLstGet(filePathLst, strLstSize(filePathLst) - 1), 0, WAL_SEGMENT_NAME_SIZE));
                                }
                                else
                                {
                                    backupResult->status = backupInvalid;
                                    verifyInvalidFileAdd(backupResult->invalidFileList, verifyOtherError, filePathName);
                                }
                            }

                            // Set backup verification complete for a backup if all files have run through verification
                            if (strEq(fileType, STORAGE_REPO_BACKUP_STR) &&
                                backupResult->totalFileVerify == backupResult->totalFileManifest)
                            {
                                backupResult->fileVerifyComplete = true;
                            }

                            // Free the job
                            protocolParallelJobFree(job);
                        }

                        // Reset the memory context occasionally so we don't use too much memory or slow down processing
                        MEM_CONTEXT_TEMP_RESET(1000);
                    }
                    while (!protocolParallelDone(parallelExec));
                }
                MEM_CONTEXT_TEMP_END();

                // ??? Need to do the final reconciliation - checking backup required WAL against, valid WAL

                // Report results
                resultStr = verifyRender(jobData.archiveIdResultList, jobData.backupResultList, verboseText);
            }
            else
                strCatZ(resultStr, "\n    no archives or backups exist in the repo");

            errorTotal += jobData.jobErrorTotal;
        }

        // If verbose output or errors then output results
        if (verboseText || errorTotal > 0)
        {
            strCatFmt(
                result, "stanza: %s\nstatus: %s%s", strZ(cfgOptionStr(cfgOptStanza)),
                errorTotal > 0 ? VERIFY_STATUS_ERROR : VERIFY_STATUS_OK, strZ(resultStr));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdVerify(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const result = verifyProcess(cfgOptionBool(cfgOptVerbose));

        // Output results if any
        if (!strEmpty(result))
        {
            // Log results
            LOG_INFO_FMT("%s", strZ(result));

            // Output to console when requested
            if (cfgOptionStrId(cfgOptOutput) == CFGOPTVAL_OUTPUT_TEXT)
            {
                ioFdWriteOneStr(STDOUT_FILENO, result);
                ioFdWriteOneStr(STDOUT_FILENO, LF_STR);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
