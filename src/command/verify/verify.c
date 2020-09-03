/***********************************************************************************************************************************
Verify Command to verify the contents of the repository
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>
#include <stdlib.h>

#include "command/archive/common.h"
#include "command/check/common.h"
#include "command/verify/file.h"
#include "command/verify/protocol.h"
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

#include <stdio.h> // CSHANG Remove

/***********************************************************************************************************************************
Data Types and Structures
***********************************************************************************************************************************/
#define FUNCTION_LOG_ARCHIVE_RESULT_TYPE                                                                                           \
    ArchiveResult
#define FUNCTION_LOG_ARCHIVE_RESULT_FORMAT(value, buffer, bufferSize)                                                              \
    objToLog(&value, "ArchiveResult", buffer, bufferSize)

#define FUNCTION_LOG_VERIFY_INFO_FILE_TYPE                                                                                         \
    VerifyInfoFile
#define FUNCTION_LOG_VERIFY_INFO_FILE_FORMAT(value, buffer, bufferSize)                                                            \
    objToLog(&value, "VerifyInfoFile", buffer, bufferSize)

#define FUNCTION_LOG_WAL_RANGE_TYPE                                                                                                \
    WalRange
#define FUNCTION_LOG_WAL_RANGE_FORMAT(value, buffer, bufferSize)                                                                   \
    objToLog(&value, "WalRange", buffer, bufferSize)

// Structure for verifying archive, backup and manifest info files
typedef struct VerifyInfoFile
{
    InfoBackup *backup;                                             // Backup.info file contents
    InfoArchive *archive;                                           // Archive.info file contents
    Manifest *manifest;                                             // Manifest file contents
    const String *checksum;                                         // File checksum
    int errorCode;                                                  // Error code else 0 for no error
} VerifyInfoFile;

// Job data results structures for archive and backup
typedef struct ArchiveResult
{
    String *archiveId;                                              // Archive Id (e.g. 9.6-1, 10-2)
    unsigned int totalWalFile;                                      // Total number of WAL files listed in directory on first read
    unsigned int totalValidWal;                                     // Total number of WAL that were verified and valid
    PgWal pgWalInfo;                                                // PG version, WAL size, system id
    List *walRangeList;                                             // List of WAL file ranges - new item is when WAL is missing
} ArchiveResult;

// WAL range includes the start/stop of sequential WAL and start/stop includes the timeline (e.g. 000000020000000100000005)
typedef struct WalRange
{
    String *stop;                                                   // Last WAL segment in this sequential range
    String *start;                                                  // First WAL segment in this sequential range
    List *invalidFileList;                                          // After all jobs complete, list of InvalidFile
} WalRange;

// Invalid file information (not missing but files failing verification) - for archive and backup
typedef struct InvalidFile
{
    String *fileName;                                               // Filename  - CSHANG Sshould maybe be filePathName
    VerifyResult reason;                                            // Reason file is invalid (e.g. incorrect checksum)
} InvalidFile;

// Status result of a backup
typedef enum
{
    backupConsistent,
    backupConsistentWithPITR,
    backupMissingManifest,
    backupInProgress,
} BackupResultStatus;

typedef struct BackupResult
{
    String *backupLabel;
    String *backupPrior;
    String *archiveStart;                                           // First WAL segment in the backup
    String *archiveStop;                                            // Last WAL segment in the backup
    BackupResultStatus status;
    List *invalidFileList;
} BackupResult;

// Job data stucture for processing and results collection
typedef struct VerifyJobData
{
    StringList *archiveIdList;                                      // List of archive ids to verify
    StringList *walPathList;                                        // WAL path list for a single archive id
    StringList *walFileList;                                        // WAL file list for a single WAL path
    StringList *backupList;                                         // List of backups to verify
    StringList *backupFileList;                                     // List of files in a single backup directory
    String *currentBackup;                                          // In progress backup, if any
    const InfoPg *pgHistory;                                        // Database history list
    bool backupProcessing;                                          // Are we processing WAL or are we processing backups
    const String *manifestCipherPass;                               // Cipher pass for reading backup manifests
    const String *walCipherPass;                                    // Cipher pass for reading WAL files
    const String *backupCipherPass;                                 // Cipher pass for reading backup files referenced in a manifest
    unsigned int jobErrorTotal;                                     // Total errors that occurred during the job execution
    List *archiveIdResultList;                                      // Archive results
    List *backupResultList;                                         // Backup results
    Manifest *manifest;                                             // Manifest currently being processed
} VerifyJobData;

/***********************************************************************************************************************************
Load a file into memory
***********************************************************************************************************************************/
static StorageRead *
verifyFileLoad(const String *pathFileName, const String *cipherPass)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, pathFileName);                  // Fully qualified path/file name
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Password to open file if encrypted
    FUNCTION_TEST_END();

    ASSERT(pathFileName != NULL);

// CSHANG But how does this type of reading help with manifest? Won't we still be pulling in the entire file into memory to get the checksum or will I need to chunk it up and add all the checksums together?

    // Read the file and error if missing
    StorageRead *result = storageNewReadP(storageRepo(), pathFileName);

    // *read points to a location within result so update result with contents based on necessary filters
    IoRead *read = storageReadIo(result);
    cipherBlockFilterGroupAdd(
        ioReadFilterGroup(read), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cipherModeDecrypt, cipherPass);
    ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(HASH_TYPE_SHA1_STR));

    // If the file is compressed, add a decompression filter
    if (compressTypeFromName(pathFileName) != compressTypeNone)
        ioFilterGroupAdd(ioReadFilterGroup(read), decompressFilter(compressTypeFromName(pathFileName)));

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get status of info files in the repository
***********************************************************************************************************************************/
static VerifyInfoFile
verifyInfoFile(const String *pathFileName, bool keepFile, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pathFileName);                   // Fully qualified path/file name
        FUNCTION_LOG_PARAM(BOOL, keepFile);                         // Should the file be kept in memory?
        FUNCTION_TEST_PARAM(STRING, cipherPass);                    // Password to open file if encrypted
    FUNCTION_LOG_END();

    ASSERT(pathFileName != NULL);

    VerifyInfoFile result = {.errorCode = 0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        TRY_BEGIN()
        {
            IoRead *infoRead = storageReadIo(verifyFileLoad(pathFileName, cipherPass));

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

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.checksum = strDup(varStr(ioFilterGroupResult(ioReadFilterGroup(infoRead), CRYPTO_HASH_FILTER_TYPE_STR)));
            }
            MEM_CONTEXT_PRIOR_END();
        }
        CATCH_ANY()
        {
            result.errorCode = errorCode();
            String *errorMsg = strNew(errorMessage());

            if (result.errorCode == errorTypeCode(&ChecksumError))
                strCat(errorMsg, strNewFmt(" %s", strZ(pathFileName)));

            LOG_WARN(strZ(errorMsg));
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(VERIFY_INFO_FILE, result);
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
        VerifyInfoFile verifyArchiveInfo = verifyInfoFile(INFO_ARCHIVE_PATH_FILE_STR, true, cfgOptionStrNull(cfgOptRepoCipherPass));

        // If the main file did not error, then report on the copy's status and check checksums
        if (verifyArchiveInfo.errorCode == 0)
        {
            result = verifyArchiveInfo.archive;
            infoArchiveMove(result, memContextPrior());

            // Attempt to load the copy and report on it's status but don't keep it in memory
            VerifyInfoFile verifyArchiveInfoCopy = verifyInfoFile(
                INFO_ARCHIVE_PATH_FILE_COPY_STR, false, cfgOptionStrNull(cfgOptRepoCipherPass));

            // If the copy loaded successfuly, then check the checksums
            if (verifyArchiveInfoCopy.errorCode == 0)
            {
                // If the info and info.copy checksums don't match each other than one (or both) of the files could be corrupt so
                // log a warning but must trust main
                if (!strEq(verifyArchiveInfo.checksum, verifyArchiveInfoCopy.checksum))
                    LOG_WARN("archive.info.copy does not match archive.info");
            }
        }
        else
        {
            // Attempt to load the copy
            VerifyInfoFile verifyArchiveInfoCopy = verifyInfoFile(
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
        VerifyInfoFile verifyBackupInfo = verifyInfoFile(INFO_BACKUP_PATH_FILE_STR, true, cfgOptionStrNull(cfgOptRepoCipherPass));

        // If the main file did not error, then report on the copy's status and check checksums
        if (verifyBackupInfo.errorCode == 0)
        {
            result = verifyBackupInfo.backup;
            infoBackupMove(result, memContextPrior());

            // Attempt to load the copy and report on it's status but don't keep it in memory
            VerifyInfoFile verifyBackupInfoCopy = verifyInfoFile(
                INFO_BACKUP_PATH_FILE_COPY_STR, false, cfgOptionStrNull(cfgOptRepoCipherPass));

            // If the copy loaded successfuly, then check the checksums
            if (verifyBackupInfoCopy.errorCode == 0)
            {
                // If the info and info.copy checksums don't match each other than one (or both) of the files could be corrupt so
                // log a warning but must trust main
                if (!strEq(verifyBackupInfo.checksum, verifyBackupInfoCopy.checksum))
                    LOG_WARN("backup.info.copy does not match backup.info");
            }
        }
        else
        {
            // Attempt to load the copy
            VerifyInfoFile verifyBackupInfoCopy = verifyInfoFile(
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

// /***********************************************************************************************************************************
// Get the manifest file
// ***********************************************************************************************************************************/
// static Manifest *
// verifyManifestFile(const String *backupLabel, const String *cipherPass, bool *currentBackup, const InfoPg *pgHistory)
// {
//     FUNCTION_LOG_BEGIN(logLevelDebug);
//         FUNCTION_LOG_PARAM(STRING, backupLabel);
//         FUNCTION_TEST_PARAM(STRING, cipherPass);
//         FUNCTION_LOG_PARAM_P(BOOL, currentBackup);
//         FUNCTION_LOG_PARAM(INFO_PG, pgHistory);
//     FUNCTION_LOG_END();
//
//     Manifest *result = NULL;
//
//     MEM_CONTEXT_TEMP_BEGIN()
//     {
//         String *fileName = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel));
//
//         // Get the main manifest file
//         VerifyInfoFile verifyManifestInfo = verifyInfoFile(fileName, true, cipherPass);
//
//         // If the main file did not error, then report on the copy's status and check checksums
//         if (verifyManifestInfo.errorCode == 0)
//         {
//             result = verifyManifestInfo.manifest;
//             manifestMove(result, memContextPrior());
//
//             // Attempt to load the copy and report on it's status but don't keep it in memory
//             VerifyInfoFile verifyManifestInfoCopy = verifyInfoFile(
//                 strNewFmt("%s%s", strZ(fileName), INFO_COPY_EXT), false, cipherPass);
//
//             // If the copy loaded successfuly, then check the checksums
//             if (verifyManifestInfoCopy.errorCode == 0)
//             {
//                 // If the manifest and manifest.copy checksums don't match each other than one (or both) of the files could be
//                 // corrupt so log a warning but trust main
//                 if (!strEq(verifyManifestInfo.checksum, verifyManifestInfoCopy.checksum))
//                     LOG_WARN("backup.manifest.copy does not match backup.manifest");
//             }
//         }
//         else
//         {
//             // Attempt to load the copy if this is not the current backup - no attempt is made to check an in-progress backup.
//             // currentBackup is only notional until the main file is checked because the backup.info file may not have existed or
//             // the backup may have completed by the time we get here. If the main manifest is simply missing, it is assumed
//             // the backup is an in-progress backup and verification is skipped, otherwise, it is no longer considered an in-progress
//             // backup and an attempt will be made to load the manifest copy.
//             if (!(*currentBackup && verifyManifestInfo.errorCode == errorTypeCode(&FileMissingError)))
//             {
//                 *currentBackup = false;
//
//                 VerifyInfoFile verifyManifestInfoCopy = verifyInfoFile(
//                     strNewFmt("%s%s", strZ(fileName), INFO_COPY_EXT), true, cipherPass);
//
//                 // If loaded successfully, then return the copy as usable
//                 if (verifyManifestInfoCopy.errorCode == 0)
//                 {
//                     LOG_WARN_FMT("%s/backup.manifest is missing or unusable, using copy", strZ(backupLabel));
//
//                     result = verifyManifestInfoCopy.manifest;
//                     manifestMove(result, memContextPrior());
//                 }
//             }
//         }
//
//         // If found a usable manifest then check that the database it was based on is in the history
//         if (result != NULL)
//         {
//             bool found = false;
//             const ManifestData *manData = manifestData(result);
//
//             // Confirm the PG database information from the manifest is in the history list
//             for (unsigned int infoPgIdx = 0; infoPgIdx < infoPgDataTotal(pgHistory); infoPgIdx++)
//             {
//                 InfoPgData pgHistoryData = infoPgData(pgHistory, infoPgIdx);
//
//                 if (pgHistoryData.id == manData->pgId && pgHistoryData.systemId == manData->pgSystemId &&
//                     pgHistoryData.version == manData->pgVersion)
//                 {
//                     found = true;
//                     break;
//                 }
//             }
//
//             // If the PG data is not found in the backup.info history, then warn but check all the files anyway
//             if (!found)
//                 LOG_WARN_FMT("'%s' may not be recoverable - PG data is not in the backup.info history", strZ(backupLabel));
//         }
//
//     }
//     MEM_CONTEXT_TEMP_END();
//
//     FUNCTION_LOG_RETURN(MANIFEST, result);
// }

/***********************************************************************************************************************************
Check the history in the info files
***********************************************************************************************************************************/
void
verifyPgHistory(const InfoPg *archiveInfoPg, const InfoPg *backupInfoPg)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_PG, archiveInfoPg);                // Postgres information from the archive.info file
        FUNCTION_TEST_PARAM(INFO_PG, backupInfoPg);                 // Postgres information from the backup.info file
    FUNCTION_TEST_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check archive.info and backup.info current PG data matches. If there is a mismatch, verify cannot continue since
        // the database is not considered accessible during the verify command so no way to tell which would be valid.
        InfoPgData archiveInfoPgData = infoPgData(archiveInfoPg, infoPgDataCurrentId(archiveInfoPg));
        InfoPgData backupInfoPgData = infoPgData(backupInfoPg, infoPgDataCurrentId(backupInfoPg));
        checkStanzaInfo(&archiveInfoPgData, &backupInfoPgData);

        unsigned int archiveInfoHistoryTotal = infoPgDataTotal(archiveInfoPg);
        unsigned int backupInfoHistoryTotal = infoPgDataTotal(backupInfoPg);

        String *errMsg = strNew("archive and backup history lists do not match");

        if (archiveInfoHistoryTotal != backupInfoHistoryTotal)
            THROW(FormatError, strZ(errMsg));

        // Confirm the lists are the same
        for (unsigned int infoPgIdx = 0; infoPgIdx < archiveInfoHistoryTotal; infoPgIdx++)
        {
            InfoPgData archiveInfoPgHistory = infoPgData(archiveInfoPg, infoPgIdx);
            InfoPgData backupInfoPgHistory = infoPgData(backupInfoPg, infoPgIdx);

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
Populate the wal ranges from the provided, sorted, wal files list for a given archiveId
***********************************************************************************************************************************/
static void
createArchiveIdRange(
    ArchiveResult *archiveIdResult, StringList *walFileList, List *archiveIdResultList, unsigned int *jobErrorTotal)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(ARCHIVE_RESULT, archiveIdResult);     // The result set for the archive Id being processed
        FUNCTION_TEST_PARAM(STRING_LIST, walFileList);              // Sorted (ascending) list of WAL files in a timeline
        FUNCTION_TEST_PARAM(LIST, archiveIdResultList);             // The list of archive Id results to add archiveIdResult to
        FUNCTION_TEST_PARAM_P(UINT, jobErrorTotal);                 // Pointer to the overall job error total
    FUNCTION_TEST_END();

    ASSERT(archiveIdResult != NULL);
    ASSERT(walFileList != NULL);
    ASSERT(archiveIdResultList != NULL);

    // Initialize the WAL range
    WalRange walRange =
    {
        .start = NULL,
        .stop = NULL,
        .invalidFileList = lstNewP(sizeof(InvalidFile), .comparator =  lstComparatorStr),
    };

    unsigned int walFileIdx = 0;

    do
    {
        String *walSegment = strSubN(strLstGet(walFileList, walFileIdx), 0, WAL_SEGMENT_NAME_SIZE);

        // If walSegment found ends in FF for PG versions 9.2 or less then skip it but log error because it should not exist and
        // PostgreSQL will ignore it
        if (archiveIdResult->pgWalInfo.version <= PG_VERSION_92 && strEndsWithZ(walSegment, "FF"))
        {
            LOG_ERROR_FMT(
                errorTypeCode(&FileInvalidError), "invalid WAL '%s' for '%s' exists, skipping", strZ(walSegment),
                strZ(archiveIdResult->archiveId));

            (*jobErrorTotal)++;

            // Remove the file from the original list so no attempt is made to verify it
            strLstRemoveIdx(walFileList, walFileIdx);
            continue;
        }

        // The lists are sorted so look ahead to see if this is a duplicate of the next one in the list
        if (walFileIdx + 1 < strLstSize(walFileList))
        {
            if (strEq(walSegment, strSubN(strLstGet(walFileList, walFileIdx + 1), 0, WAL_SEGMENT_NAME_SIZE)))
            {
                LOG_ERROR_FMT(
                    errorTypeCode(&FileInvalidError), "duplicate WAL '%s' for '%s' exists, skipping", strZ(walSegment),
                    strZ(archiveIdResult->archiveId));

                (*jobErrorTotal)++;

                bool foundDup = true;
// CSHANG In archiveIdResult have a duplicate error count
                // Remove all duplicates of this WAL, including this WAL, from the list
                while (strLstSize(walFileList) > 0 && foundDup)
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
        if (walRange.start == NULL)
        {
            walRange.start = walSegment;
            walRange.stop = walSegment;
            walFileIdx++;
            continue;
        }

        // If the next WAL is the appropriate distance away, then there is no gap. For versions less than or equal to 9.2,
        // the WAL size is static at 16MB but (for some unknown reason) WAL ending in FF is skipped so it should never exist, so
        // the next WAL is 2 times the distance (WAL segment size) away, not one.
        if (pgLsnFromWalSegment(walSegment, archiveIdResult->pgWalInfo.size) -
            pgLsnFromWalSegment(walRange.stop, archiveIdResult->pgWalInfo.size) ==
                ((archiveIdResult->pgWalInfo.version <= PG_VERSION_92 && strEndsWithZ(walRange.stop, "FE"))
                ? archiveIdResult->pgWalInfo.size * 2 : archiveIdResult->pgWalInfo.size))
        {
            walRange.stop = walSegment;
        }
        else
        {
            // A gap was found so add the current range to the list and start a new range
            lstAdd(archiveIdResult->walRangeList, &walRange);
            walRange = (WalRange)
            {
                .start = strDup(walSegment),
                .stop = strDup(walSegment),
                .invalidFileList = lstNewP(sizeof(InvalidFile), .comparator =  lstComparatorStr),
            };
        }

        walFileIdx++;
    }
    while (walFileIdx < strLstSize(walFileList));

    // If walRange containes a range, then add the last walRange to the list
    if (walRange.start != NULL)
        lstAdd(archiveIdResult->walRangeList, &walRange);

    // Now if there are ranges for this archiveId then add them
    if (lstSize(archiveIdResult->walRangeList) > 0)
        lstAdd(archiveIdResultList, archiveIdResult);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Verify the job data for the archives
***********************************************************************************************************************************/
static ProtocolParallelJob *
verifyArchive(void *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);                          // Pointer to the job data
    FUNCTION_TEST_END();

    ProtocolParallelJob *result = NULL;

    VerifyJobData *jobData = data;

    // Process archive files, if any
    while (strLstSize(jobData->archiveIdList) > 0)
    {
        result = NULL;

        ArchiveResult archiveIdResult =
        {
            .archiveId = strDup(strLstGet(jobData->archiveIdList, 0)),
            .walRangeList = lstNewP(sizeof(WalRange), .comparator =  lstComparatorStr),
        };

        if (strLstSize(jobData->walPathList) == 0)
        {
            // Get the WAL paths for the first item in the archive Id list
            jobData->walPathList =
                strLstSort(
                    storageListP(
                        storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strZ(archiveIdResult.archiveId)),
                        .expression = WAL_SEGMENT_DIR_REGEXP_STR),
                    sortOrderAsc);
        }

        // If there are WAL paths then get the file lists
        if (strLstSize(jobData->walPathList) > 0)
        {
            do
            {
                String *walPath = strLstGet(jobData->walPathList, 0);

                // Get the WAL files for the first item in the WAL paths list and initialize WAL info and ranges
                if (strLstSize(jobData->walFileList) == 0)
                {
                    jobData->walFileList =
                        strLstSort(
                            storageListP(
                                storageRepo(),
                                strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(archiveIdResult.archiveId), strZ(walPath)),
                                .expression = WAL_SEGMENT_FILE_REGEXP_STR),
                            sortOrderAsc);

                    if (strLstSize(jobData->walFileList) > 0)
                    {
                        // Initialize the WAL segment size from the first WAL
                        StorageRead *walRead = verifyFileLoad(
                            strNewFmt(
                                STORAGE_REPO_ARCHIVE "/%s/%s/%s", strZ(archiveIdResult.archiveId), strZ(walPath),
                                strZ(strLstGet(jobData->walFileList, 0))),
                            jobData->walCipherPass);

                        PgWal walInfo = pgWalFromBuffer(storageGetP(walRead, .exactSize = PG_WAL_HEADER_SIZE));

                        archiveIdResult.pgWalInfo.size = walInfo.size;
                        archiveIdResult.pgWalInfo.version = walInfo.version;

                        // Add total number of WAL file in the directory to the total WAL - this number will include duplicates,
                        // if any, that will be filtered out and not checked but will be reported as errors in the log
                        archiveIdResult.totalWalFile += strLstSize(jobData->walFileList);

                        createArchiveIdRange(
                            &archiveIdResult, jobData->walFileList, jobData->archiveIdResultList, &jobData->jobErrorTotal);
                    }
                }

                // If there are WAL files, then verify them
                if (strLstSize(jobData->walFileList) > 0)
                {
                    do
                    {
                        // Get the archive id info for the current (last) archive id being processed
                        ArchiveResult *archiveResult = lstGetLast(jobData->archiveIdResultList);

                        // Get the fully qualified file name and checksum
                        const String *fileName = strLstGet(jobData->walFileList, 0);
                        const String *filePathName = strNewFmt(
                            STORAGE_REPO_ARCHIVE "/%s/%s/%s", strZ(archiveResult->archiveId), strZ(walPath), strZ(fileName));
                        String *checksum = strSubN(fileName, WAL_SEGMENT_NAME_SIZE + 1, HASH_TYPE_SHA1_SIZE_HEX);

                        // Set up the job
                        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_VERIFY_FILE_STR);
                        protocolCommandParamAdd(command, VARSTR(filePathName));
                        protocolCommandParamAdd(command, VARSTR(checksum));
                        protocolCommandParamAdd(command, VARBOOL(true));
                        protocolCommandParamAdd(command, VARUINT64(archiveResult->pgWalInfo.size));
                        protocolCommandParamAdd(command, VARSTR(jobData->walCipherPass));

                        // Assign job to result
                        result = protocolParallelJobNew(VARSTR(filePathName), command);
                        // CSHANG for now, since no temp context then no move
                        // result = protocolParallelJobMove(
                        //     protocolParallelJobNew(VARSTR(filePathName), command), memContextPrior());

                        // Remove the file to process from the list
                        strLstRemoveIdx(jobData->walFileList, 0);

                        // If this is the last file to process for this timeline, then remove the path
                        if (strLstSize(jobData->walFileList) == 0)
                            strLstRemoveIdx(jobData->walPathList, 0);

                        // Return to process the job found
                        break;
                    }
                    while (strLstSize(jobData->walFileList) > 0);
                }
                else
                {
                    // No valid WAL to process (may be only duplicates or nothing in WAL path) - remove the WAL path from the list
                    LOG_WARN_FMT(
                        "path '%s/%s' does not contain any valid WAL to be processed", strZ(archiveIdResult.archiveId),
                        strZ(walPath));
                    strLstRemoveIdx(jobData->walPathList, 0);
                }

                // If a job was found to be processed then break out to process it
                if (result != NULL)
                    break;
            }
            while (strLstSize(jobData->walPathList) > 0);

            // If this is the last timeline to process for this archiveId, then remove the archiveId
            if (strLstSize(jobData->walPathList) == 0)
                strLstRemoveIdx(jobData->archiveIdList, 0);

            // If a file was sent to be processed then break so can process it
            if (result != NULL)
                break;
        }
        else
        {
            // Log that no WAL paths exist in the archive Id dir - remove the archive Id from the list (nothing to process)
            LOG_WARN_FMT("archive path '%s' is empty", strZ(archiveIdResult.archiveId));
            strLstRemoveIdx(jobData->archiveIdList, 0);

            // Add to the results for completeness
            lstAdd(jobData->archiveIdResultList, &archiveIdResult);
        }
    }

    FUNCTION_TEST_RETURN(result);
}

// /***********************************************************************************************************************************
// Verify the job data backups
// ***********************************************************************************************************************************/
// static ProtocolParallelJob *
// verifyBackup(void *data)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM_P(VOID, data);
//     FUNCTION_TEST_END();
//
//     ProtocolParallelJob *result = NULL;
//
//     VerifyJobData *jobData = data;
//
//     // Process backup files, if any
//     while (strLstSize(jobData->backupList) > 0)
//     {
//         result = NULL;
//
//         BackupResult backupResult =
//         {
//             .backupLabel = strDup(strLstGet(jobData->backupList, 0)),
//         };
//
//         bool inProgressBackup = strEq(jobData->currentBackup, backupResult.backupLabel);
//
//         // Get a usable backup manifest file
//         const Manifest *manifest = verifyManifestFile(
//             backupResult.backupLabel, jobData->manifestCipherPass, &inProgressBackup, jobData->pgHistory);
//
//         // If a usable backup.manifest file is not found
//         if (manifest == NULL)
//         {
//             // Warn if it is not actually the current in-progress backup
//             if (!inProgressBackup)
//             {
//                 backupResult.status = backupMissingManifest;
//
//                 LOG_WARN_FMT("Manifest files missing for '%s' - backup may have expired", strZ(backupResult.backupLabel));
//             }
//             else
//             {
//                 backupResult.status = backupInProgress;
//
//                 LOG_INFO_FMT("backup '%s' appears to be in progress, skipping", strZ(backupResult.backupLabel));
//             }
//
//             // Update the result status and skip
//             lstAdd(jobData->backupResultList, &backupResult);
//
//             // Remove this backup from the processing list
//             strLstRemoveIdx(jobData->backupList, 0);
//         }
//         // Else process the files in the manifest
//         else
//         {
// // CSHANG Problem here because the manifest poiinter is declared const - but I want to change it each time: jobData->manifest = manifest; but do I really need to store it? Or just the manData (which is also a const) - so maybe I neeed to MOVE it into the the jobData?
//
//             const ManifestData *manData = manifestData(manifest);
//             backupResult.archiveStart = strDup(manData->archiveStart);
//             backupResult.archiveStop = strDup(manData->archiveStop); // CSHANG May not have this?
//
//             // Get the cipher subpass used to decrypt files in the backup
//             jobData->backupCipherPass = manifestCipherSubPass(manifest);
// // CSHANG Need compress-type so can create the name of the file (LOOK at restore for how it constructs the name and reds the file off disk)
// // CSHANG It is possible to have a backup without all the WAL if option-archive-check=false is not set but in this is not on then all bets are off
//
// // CSHANG Should free the manifest after complete here in order to get it out of memory and start on a new one
//         }
//     }
//
//     FUNCTION_TEST_RETURN(result);
// }

/***********************************************************************************************************************************
Process the job data
***********************************************************************************************************************************/
static ProtocolParallelJob *
verifyJobCallback(void *data, unsigned int clientIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);                          // Pointer to the job data
        FUNCTION_TEST_PARAM(UINT, clientIdx);                       // Client index (not used for this process)
    FUNCTION_TEST_END();

    ASSERT(data != NULL);

    // No special logic based on the client, since only operating against the repo, so just get the next job
    (void)clientIdx;

    // Initialize the result
    ProtocolParallelJob *result = NULL;

    // MEM_CONTEXT_TEMP_BEGIN() // cshang remove temp block FOR NOW - will later have a memContext management
    // {
    // Get a new job if there are any left
    VerifyJobData *jobData = data;

    if (!jobData->backupProcessing)
    {
        result = verifyArchive(data);
        jobData->backupProcessing = strLstSize(jobData->archiveIdList) == 0;

        // If there is a result from archiving, then return it
        if (result != NULL)
            FUNCTION_TEST_RETURN(result);  // CSHANG can only do if don't have a temp mem context
    }
    //
    // // Process backups - get manifest and verify it first thru function here vs sending verifyFile, log errors and incr job error
    // if (jobData->backupProcessing)
    // {
    //     result = verifyBackup(data);
    //
    //     // If there is a result from backups, then return it
    //     if (result != NULL)
    //         FUNCTION_TEST_RETURN(result);
    // }

    // }
    // MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Helper function for returning a string corresponding to the result code
***********************************************************************************************************************************/
static String *
verifyErrorMsg(VerifyResult verifyResult)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, verifyResult);                    // Result code from the verifyFile() function
    FUNCTION_TEST_END();

    String *result = strNew("");

    if (verifyResult == verifyFileMissing)
        result = strCatZ(result, "file missing");
    else if (verifyResult == verifyChecksumMismatch)
        result = strCatZ(result, "invalid checksum");
    else
        result = strCatZ(result, "invalid size");

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Helper function to set the currently processing backup label, if any, and check that the archiveIds are in the db history
***********************************************************************************************************************************/
static String *
setBackupCheckArchive(
    const StringList *backupList, const InfoBackup *backupInfo, const StringList *archiveIdList, const InfoPg *pgHistory,
    unsigned int *jobErrorTotal)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, backupList);               // List of backup labels in the backup directory
        FUNCTION_TEST_PARAM(INFO_BACKUP, backupInfo);               // Contents of the backup.info file
        FUNCTION_TEST_PARAM(STRING_LIST, archiveIdList);            // List of archiveIds in the archive directory
        FUNCTION_TEST_PARAM(INFO_PG, pgHistory);                    // Pointer to InfoPg of archive.info for accesing PG history
        FUNCTION_TEST_PARAM_P(UINT, jobErrorTotal);                 // Pointer to overall job error total
    FUNCTION_TEST_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If there are backups, set the last backup as current if it is not in backup.info - if it is, then it is complete, else
        // it will be checked later
        if (strLstSize(backupList) > 0)
        {
            // Get the last backup as current if it is not in backup.info current list
            String *backupLabel = strLstGet(backupList, strLstSize(backupList) - 1);

            if (infoBackupDataByLabel(backupInfo, backupLabel) == NULL)
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
        if (strLstSize(archiveIdList) > 0)
        {
            StringList *archiveIdHistoryList = strLstNew();

            for (unsigned int histIdx = 0; histIdx < infoPgDataTotal(pgHistory); histIdx++)
            {
                strLstAdd(archiveIdHistoryList, infoPgArchiveId(pgHistory, histIdx));
            }

            // Sort the history list
            strLstSort(strLstComparatorSet(archiveIdHistoryList, archiveIdComparator), sortOrderAsc);

            String *missingFromHistory = strNew("");

            // Check if the archiveId on disk exists in the archive.info history list and report it if not
            for (unsigned int archiveIdx = 0; archiveIdx < strLstSize(archiveIdList); archiveIdx++)
            {
                String *archiveId = strLstGet(archiveIdList, archiveIdx);
                if (!strLstExists(archiveIdHistoryList, archiveId))
                    strCat(missingFromHistory, (strEmpty(missingFromHistory) ? archiveId : strNewFmt(", %s", strZ(archiveId))));
            }

            if (!strEmpty(missingFromHistory))
            {
                LOG_ERROR_FMT(
                    errorTypeCode(&ArchiveMismatchError), "archiveIds '%s' are not in the archive.info history list",
                    strZ(missingFromHistory));

                (*jobErrorTotal)++;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
 Add the file to the invalid file list for the range in which it exists
***********************************************************************************************************************************/
static void
addInvalidWalFile(List *walRangeList, VerifyResult fileResult, String *fileName, String *walSegment)
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

    for (unsigned int walIdx = 0; walIdx < lstSize(walRangeList); walIdx++)
    {
        WalRange *walRange = lstGet(walRangeList, walIdx);

        // If the WAL segment is less/equal to the stop file then it falls in this range since ranges
        // are sorted by stop file in ascending order, therefore first one found is the range.
        if (strCmp(walRange->stop, walSegment) >= 0)
        {
            InvalidFile invalidFile =
            {
                .fileName = strDup(fileName),
                .reason = fileResult,
            };

            // Add the file to the range where it was found and exit the loop
            lstAdd(walRange->invalidFileList, &invalidFile);
            break;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Render the results of the verify command
***********************************************************************************************************************************/
static String *
verifyRender(List *archiveIdResultList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, archiveIdResultList);             // Result list for all archive Ids in the repo
    FUNCTION_TEST_END();

    ASSERT(archiveIdResultList != NULL);

    String *result = strNew("Results:\n");

    for (unsigned int archiveIdx = 0; archiveIdx < lstSize(archiveIdResultList); archiveIdx++)
    {
        ArchiveResult *archiveIdResult = lstGet(archiveIdResultList, archiveIdx);
        strCatFmt(
            result, "  archiveId: %s, total WAL checked: %u, total valid WAL: %u\n", strZ(archiveIdResult->archiveId),
            archiveIdResult->totalWalFile, archiveIdResult->totalValidWal);

        if (archiveIdResult->totalWalFile > 0)
        {
            unsigned int errMissing = 0;
            unsigned int errChecksum = 0;
            unsigned int errSize = 0;
            unsigned int errOther = 0;

            for (unsigned int walIdx = 0; walIdx < lstSize(archiveIdResult->walRangeList); walIdx++)
            {
                WalRange *walRange = lstGet(archiveIdResult->walRangeList, walIdx);

                LOG_DETAIL_FMT(
                    "archiveId: %s, wal start: %s, wal stop: %s", strZ(archiveIdResult->archiveId), strZ(walRange->start),
                    strZ(walRange->stop));

                unsigned int invalidIdx = 0;

                while (invalidIdx < lstSize(walRange->invalidFileList))
                {
                    InvalidFile *invalidFile = lstGet(walRange->invalidFileList, invalidIdx);

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

            strCatFmt(
                result,
                "    missing: %u, checksum invalid: %u, size invalid: %u, other: %u\n",
                errMissing, errChecksum, errSize, errOther);
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Process the verify command
***********************************************************************************************************************************/
static String *
verifyProcess(unsigned int *errorTotal)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_TEST_PARAM_P(UINT, errorTotal);                    // Pointer to overall job error total
    FUNCTION_LOG_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *resultStr = strNew("");

        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        const Storage *storage = storageRepo();

        // Get a usable backup info file
        InfoBackup *backupInfo = verifyBackupInfoFile();

        // If a usable backup.info file is not found, then report an error in the log
        if (backupInfo == NULL)
        {
            LOG_ERROR(errorTypeCode(&FormatError), "No usable backup.info file");
            (*errorTotal)++;
        }

        // Get a usable archive info file
        InfoArchive *archiveInfo = verifyArchiveInfoFile();

        // If a usable archive.info file is not found, then report an error in the log
        if (archiveInfo == NULL)
        {
            LOG_ERROR(errorTypeCode(&FormatError), "No usable archive.info file");
            (*errorTotal)++;
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
                LOG_ERROR(errorTypeCode(&FormatError), errorMessage());
                (*errorTotal)++;
            }
            TRY_END();
        }

        // If valid info files, then begin process of checking backups and archives in the repo
        if ((*errorTotal) == 0)
        {
            // Initialize the job data
            VerifyJobData jobData =
            {
                .walPathList = strLstNew(),  // cshang need to create memcontex and later after processing loop, memContextDiscard(); see manifest.c line 1793
                .walFileList = strLstNew(),
                .backupFileList = strLstNew(),
                .pgHistory = infoArchivePg(archiveInfo),
                .manifestCipherPass = infoPgCipherPass(infoBackupPg(backupInfo)),
                .walCipherPass = infoPgCipherPass(infoArchivePg(archiveInfo)),
                .archiveIdResultList = lstNewP(sizeof(ArchiveResult), .comparator =  archiveIdComparator),
            };

            // Get a list of backups in the repo
            jobData.backupList = strLstSort(
                storageListP(
                    storage, STORAGE_REPO_BACKUP_STR,
                    .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
                sortOrderAsc);

            // Get a list of archive Ids in the repo (e.g. 9.4-1, 10-2, etc) sorted by the db-id (number after the dash)
            jobData.archiveIdList = strLstSort(
                strLstComparatorSet(
                    storageListP(storage, STORAGE_REPO_ARCHIVE_STR, .expression = STRDEF(REGEX_ARCHIVE_DIR_DB_VERSION)),
                    archiveIdComparator),
                sortOrderAsc);

            // Only begin processing if there are some archives or backups in the repo
            if (strLstSize(jobData.archiveIdList) > 0 || strLstSize(jobData.backupList) > 0)
            {
                // WARN if there are no archives or there are no backups in the repo so that the callback need not try to
                // distinguish between having processed all of the list or if the list was missing in the first place
                if (strLstSize(jobData.archiveIdList) == 0 || strLstSize(jobData.backupList) == 0)
                    LOG_WARN_FMT("no %s exist in the repo", strLstSize(jobData.archiveIdList) == 0 ? "archives" : "backups");

                // Set current backup if there is one and verify the archive history on disk is in the database history
                jobData.currentBackup = setBackupCheckArchive(
                    jobData.backupList, backupInfo, jobData.archiveIdList, jobData.pgHistory, &jobData.jobErrorTotal);

                // Create the parallel executor
                ProtocolParallel *parallelExec = protocolParallelNew(
                    (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * MSEC_PER_SEC) / 2, verifyJobCallback, &jobData);

                // CSHANG Add this option
                // // If a fast option has been requested, then only create one process to handle, else create as many as process-max
                // unsigned int numProcesses = cfgOptionTest(cfgOptFast) ? 1 : cfgOptionUInt(cfgOptProcessMax);

                for (unsigned int processIdx = 1; processIdx <= cfgOptionUInt(cfgOptProcessMax); processIdx++)
                    protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypeRepo, 1, processIdx));

                // Process jobs
                do
                {
                    unsigned int completed = protocolParallelProcess(parallelExec);

                    // Process completed jobs
                    for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
                    {
                        // Get the job and job key
                        ProtocolParallelJob *job = protocolParallelResult(parallelExec);
                        unsigned int processId = protocolParallelJobProcessId(job);
                        StringList *filePathLst = strLstNewSplit(varStr(protocolParallelJobKey(job)), FSLASH_STR);
                        String *fileType = strLstGet(filePathLst, 0);
                        strLstRemoveIdx(filePathLst, 0);
                        String *filePathName = strLstJoin(filePathLst, "/");

                        ArchiveResult *archiveIdResult;

                        // Get archiveId result data
                        if (strEq(fileType, STORAGE_REPO_ARCHIVE_STR))
                        {
                            // Find the archiveId in the list - ASSERT if not found since this should never happen
                            String *archiveId = strLstGet(filePathLst, 0);
                            unsigned int index = lstFindIdx(jobData.archiveIdResultList, &archiveId);
                            ASSERT(index != LIST_NOT_FOUND);

                            archiveIdResult = lstGet(jobData.archiveIdResultList, index);
                        }

                        // The job was successful
                        if (protocolParallelJobErrorCode(job) == 0)
                        {
                            const VerifyResult verifyResult = (VerifyResult)varUIntForce(protocolParallelJobResult(job));

                            if (strEq(fileType, STORAGE_REPO_ARCHIVE_STR))
                            {
                                if (verifyResult != verifyOk)
                                {
                                    // Log a warning because the WAL may have gone missing if expire came through and removed it
                                    // legitimately so it is not necessarily an error so the jobErrorTotal should not be incremented
                                    // !!! Maybe filter on missing and report an error and increment error if something else?
                                    // meaning shouldn't a checksum or invalid size be reported as an ERROR in the log and not WARN?
                                    LOG_WARN_PID_FMT(processId, "%s: %s", strZ(verifyErrorMsg(verifyResult)), strZ(filePathName));

                                    // If this is a WAL file
                                    if (strEq(fileType, STORAGE_REPO_ARCHIVE_STR))
                                    {
                                        // Add invalid file with reason from result of verifyFile to range list
                                        addInvalidWalFile(
                                            archiveIdResult->walRangeList, verifyResult, filePathName,
                                            strSubN(strLstGet(filePathLst, strLstSize(filePathLst) - 1), 0, WAL_SEGMENT_NAME_SIZE));
                                    }
                                }
                                else
                                    archiveIdResult->totalValidWal++;

                            }
                        }
                        // Else the job errored
                        else
                        {
                            // Log a protocol error and increment the jobErrorTotal
                            LOG_ERROR_PID_FMT(
                                processId, errorTypeCode(&ProtocolError),
                                "could not verify %s: [%d] %s", strZ(filePathName),
                                protocolParallelJobErrorCode(job), strZ(protocolParallelJobErrorMessage(job)));

                            jobData.jobErrorTotal++;

                            // If this is a WAL file, then add the file to the invalid file list for WAL range of the archiveId
                            if (strEq(fileType, STORAGE_REPO_ARCHIVE_STR))
                            {
                                // Add invalid file with "OtherError" reason to range list
                                addInvalidWalFile(
                                    archiveIdResult->walRangeList, verifyOtherError, filePathName,
                                    strSubN(strLstGet(filePathLst, strLstSize(filePathLst) - 1), 0, WAL_SEGMENT_NAME_SIZE));
                            }

                        }
                        // Free the job
                        protocolParallelJobFree(job);
                    }
                }
                while (!protocolParallelDone(parallelExec));

                // HERE we will need to do the final reconciliation - checking backup required WAL against, valid WAL

                // Report results
                if (lstSize(jobData.archiveIdResultList) > 0)
                    resultStr = verifyRender(jobData.archiveIdResultList);
            }
            else
                LOG_WARN("no archives or backups exist in the repo");

            (*errorTotal) += jobData.jobErrorTotal;
        }

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strDup(resultStr);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
void
cmdVerify(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        unsigned int errorTotal = 0;
        String *result = verifyProcess(&errorTotal);

        // !!! Output results but need to make conditional based on an option not whether there are errors or not
        if (errorTotal == 0)
            ioFdWriteOneStr(STDOUT_FILENO, result);

        // Throw an error if any encountered
        if (errorTotal > 0)
            THROW_FMT(RuntimeError, "%u fatal errors encountered, see log for details", errorTotal);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
