/***********************************************************************************************************************************
Verify Command

Verify the contents of the repository.
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

/***********************************************************************************************************************************
Data Types and Structures
***********************************************************************************************************************************/
#define FUNCTION_LOG_VERIFY_ARCHIVE_RESULT_TYPE                                                                                    \
    VerifyArchiveResult
#define FUNCTION_LOG_VERIFY_ARCHIVE_RESULT_FORMAT(value, buffer, bufferSize)                                                       \
    objToLog(&value, "VerifyArchiveResult", buffer, bufferSize)

#define FUNCTION_LOG_VERIFY_INFO_FILE_TYPE                                                                                         \
    VerifyInfoFile
#define FUNCTION_LOG_VERIFY_INFO_FILE_FORMAT(value, buffer, bufferSize)                                                            \
    objToLog(&value, "VerifyInfoFile", buffer, bufferSize)

#define FUNCTION_LOG_VERIFY_WAL_RANGE_TYPE                                                                                         \
    VerifyWalRange
#define FUNCTION_LOG_VERIFY_WAL_RANGE_FORMAT(value, buffer, bufferSize)                                                            \
    objToLog(&value, "VerifyWalRange", buffer, bufferSize)

// Structure for verifying archive, backup, and manifest info files
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
    backupConsistent,
    backupConsistentWithPITR,
    backupMissingManifest,
    backupInProgress,
} VerifyBackupResultStatus;

typedef struct VerifyBackupResult
{
    String *backupLabel;
    String *backupPrior;
    String *archiveStart;                                           // First WAL segment in the backup
    String *archiveStop;                                            // Last WAL segment in the backup
    VerifyBackupResultStatus status;
    List *invalidFileList;
} VerifyBackupResult;

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
                else
                    result.archive = infoArchiveMove(infoArchiveNewLoad(infoRead), memContextPrior());
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
Populate the WAL ranges from the provided, sorted, WAL files list for a given archiveId
***********************************************************************************************************************************/
static void
verifyCreateArchiveIdRange(VerifyArchiveResult *archiveIdResult, StringList *walFileList, unsigned int *jobErrorTotal)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VERIFY_ARCHIVE_RESULT, archiveIdResult);  // The result set for the archive Id being processed
        FUNCTION_TEST_PARAM(STRING_LIST, walFileList);                  // Sorted (ascending) list of WAL files in a timeline
        FUNCTION_TEST_PARAM_P(UINT, jobErrorTotal);                     // Pointer to the overall job error total
    FUNCTION_TEST_END();

    ASSERT(archiveIdResult != NULL);
    ASSERT(walFileList != NULL);

    unsigned int walFileIdx = 0;
    bool addWal = true;

    // Initialize the WAL range
    VerifyWalRange walRange =
    {
        .start = NULL,
        .stop = NULL,
        .invalidFileList = lstNewP(sizeof(VerifyInvalidFile), .comparator =  lstComparatorStr),
    };

    // If there is a WAL range for this archiveID, get the last one. If there is no timeline change then continue updating the last
    // WAL range.
    if (lstSize(archiveIdResult->walRangeList) != 0 &&
        strEq(
            strSubN(((VerifyWalRange *)lstGetLast(archiveIdResult->walRangeList))->stop, 0, 8),
            strSubN(strSubN(strLstGet(walFileList, walFileIdx), 0, WAL_SEGMENT_NAME_SIZE), 0, 8)))
    {
        walRange = *(VerifyWalRange *)lstGetLast(archiveIdResult->walRangeList);
        addWal = false;
    }

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

        // Initialize the range if it has not yet been initialized with a segment and continue to next
        if (walRange.start == NULL)
        {
            walRange.start = walSegment;
            walRange.stop = walSegment;
            walFileIdx++;
            continue;
        }

        // If the next WAL is the appropriate distance away, then there is no gap
        if (strEq(
            walSegmentNext(walRange.stop, (size_t)archiveIdResult->pgWalInfo.size, archiveIdResult->pgWalInfo.version), walSegment))
        {
            walRange.stop = walSegment;

            // Update the archiveId range if WAL range is already added to the archiveId
            if (!addWal)
            {
                MEM_CONTEXT_BEGIN(lstMemContext(archiveIdResult->walRangeList))
                {
                    ((VerifyWalRange *)lstGetLast(archiveIdResult->walRangeList))->stop = strDup(walRange.stop);
                }
                MEM_CONTEXT_END();
            }
        }
        else
        {
            // A gap was found and if not updating the current WAL range then add the current range to the list
            // else update the current range.
            if (addWal)
                lstAdd(archiveIdResult->walRangeList, &walRange);
            else
            {
                MEM_CONTEXT_BEGIN(lstMemContext(archiveIdResult->walRangeList))
                {
                    ((VerifyWalRange *)lstGetLast(archiveIdResult->walRangeList))->stop = strDup(walRange.stop);
                }
                MEM_CONTEXT_END();
            }

            // Start a new range
            walRange = (VerifyWalRange)
            {
                .start = strDup(walSegment),
                .stop = strDup(walSegment),
                .invalidFileList = lstNewP(sizeof(VerifyInvalidFile), .comparator =  lstComparatorStr),
            };
            addWal = true;
        }

        walFileIdx++;
    }
    while (walFileIdx < strLstSize(walFileList));

    // If walRange containes a range, then add the last walRange to the list
    if (addWal && walRange.start != NULL)
        lstAdd(archiveIdResult->walRangeList, &walRange);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Return verify jobs for the archive
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

        // Add archiveId to the result list if the list is empty or the last processed is not equal to the current archiveId
        if (lstSize(jobData->archiveIdResultList) == 0 ||
            !strEq(
                ((VerifyArchiveResult *)lstGetLast(jobData->archiveIdResultList))->archiveId, strLstGet(jobData->archiveIdList, 0)))
        {
            VerifyArchiveResult archiveIdResult =
            {
                .archiveId = strDup(strLstGet(jobData->archiveIdList, 0)),
                .walRangeList = lstNewP(sizeof(VerifyWalRange), .comparator =  lstComparatorStr),
            };

            lstAdd(jobData->archiveIdResultList, &archiveIdResult);

            // Get the WAL paths for the archive Id
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

                // Get the archive id info for the current (last) archive id being processed
                VerifyArchiveResult *archiveResult = lstGetLast(jobData->archiveIdResultList);

                // Get the WAL files for the first item in the WAL paths list and initialize WAL info and ranges
                if (strLstSize(jobData->walFileList) == 0)
                {
                    jobData->walFileList =
                        strLstSort(
                            storageListP(
                                storageRepo(),
                                strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(archiveResult->archiveId), strZ(walPath)),
                                .expression = WAL_SEGMENT_FILE_REGEXP_STR),
                            sortOrderAsc);

                    if (strLstSize(jobData->walFileList) > 0)
                    {
                        if (archiveResult->pgWalInfo.size == 0)
                        {
                            // Initialize the WAL segment size from the first WAL
                            StorageRead *walRead = verifyFileLoad(
                                strNewFmt(
                                    STORAGE_REPO_ARCHIVE "/%s/%s/%s", strZ(archiveResult->archiveId), strZ(walPath),
                                    strZ(strLstGet(jobData->walFileList, 0))),
                                jobData->walCipherPass);

                            PgWal walInfo = pgWalFromBuffer(storageGetP(walRead, .exactSize = PG_WAL_HEADER_SIZE));

                            archiveResult->pgWalInfo.size = walInfo.size;
                            archiveResult->pgWalInfo.version = walInfo.version;
                        }

                        // Add total number of WAL files in the directory to the total WAL - this number will include duplicates,
                        // if any, that will be filtered out and not checked but will be reported as errors in the log
                        archiveResult->totalWalFile += strLstSize(jobData->walFileList);

                        verifyCreateArchiveIdRange(archiveResult, jobData->walFileList, &jobData->jobErrorTotal);
                    }
                }

                // If there are WAL files, then verify them
                if (strLstSize(jobData->walFileList) > 0)
                {
                    do
                    {
                        // Get the fully qualified file name and checksum
                        const String *fileName = strLstGet(jobData->walFileList, 0);
                        const String *filePathName = strNewFmt(
                            STORAGE_REPO_ARCHIVE "/%s/%s/%s", strZ(archiveResult->archiveId), strZ(walPath), strZ(fileName));
                        String *checksum = strSubN(fileName, WAL_SEGMENT_NAME_SIZE + 1, HASH_TYPE_SHA1_SIZE_HEX);

                        // Set up the job
                        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_VERIFY_FILE_STR);
                        protocolCommandParamAdd(command, VARSTR(filePathName));
                        protocolCommandParamAdd(command, VARSTR(checksum));
                        protocolCommandParamAdd(command, VARUINT64(archiveResult->pgWalInfo.size));
                        protocolCommandParamAdd(command, VARSTR(jobData->walCipherPass));

                        // Assign job to result
                        result = protocolParallelJobNew(VARSTR(filePathName), command);

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
                        "path '%s/%s' does not contain any valid WAL to be processed", strZ(archiveResult->archiveId),
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
            LOG_WARN_FMT("archive path '%s' is empty", strZ(strLstGet(jobData->archiveIdList, 0)));
            strLstRemoveIdx(jobData->archiveIdList, 0);
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Process the job data
***********************************************************************************************************************************/
static ProtocolParallelJob *
verifyJobCallback(void *data, unsigned int clientIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);                          // Pointer to the job data
        (void)clientIdx;                                            // Client index (not used for this process)
    FUNCTION_TEST_END();

    ASSERT(data != NULL);

    // Initialize the result
    ProtocolParallelJob *result = NULL;

    // Get a new job if there are any left
    VerifyJobData *jobData = data;

    if (!jobData->backupProcessing)
    {
        result = verifyArchive(data);
        jobData->backupProcessing = strLstSize(jobData->archiveIdList) == 0;

        // If there is a result from archiving, then return it
        if (result != NULL)
            FUNCTION_TEST_RETURN(result);
    }

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
    else if (verifyResult == verifySizeInvalid)
        result = strCatZ(result, "invalid size");
    else
        result = strCatZ(result, "invalid verify");

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Helper function to set the currently processing backup label, if any, and check that the archiveIds are in the db history
***********************************************************************************************************************************/
static String *
verifySetBackupCheckArchive(
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
                strLstAdd(archiveIdHistoryList, infoPgArchiveId(pgHistory, histIdx));

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
verifyAddInvalidWalFile(List *walRangeList, VerifyResult fileResult, String *fileName, String *walSegment)
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
        VerifyWalRange *walRange = lstGet(walRangeList, walIdx);

        // If the WAL segment is less/equal to the stop file then it falls in this range since ranges are sorted by stop file in
        // ascending order, therefore first one found is the range
        if (strCmp(walRange->stop, walSegment) >= 0)
        {
            VerifyInvalidFile invalidFile =
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
        VerifyArchiveResult *archiveIdResult = lstGet(archiveIdResultList, archiveIdx);
        strCatFmt(
            result, "%s  archiveId: %s, total WAL checked: %u, total valid WAL: %u", (archiveIdx > 0 ? "\n" : ""),
            strZ(archiveIdResult->archiveId), archiveIdResult->totalWalFile, archiveIdResult->totalValidWal);

        if (archiveIdResult->totalWalFile > 0)
        {
            unsigned int errMissing = 0;
            unsigned int errChecksum = 0;
            unsigned int errSize = 0;
            unsigned int errOther = 0;

            for (unsigned int walIdx = 0; walIdx < lstSize(archiveIdResult->walRangeList); walIdx++)
            {
                VerifyWalRange *walRange = lstGet(archiveIdResult->walRangeList, walIdx);

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

            strCatFmt(
                result,
                "\n    missing: %u, checksum invalid: %u, size invalid: %u, other: %u",
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
                .walPathList = strLstNew(),
                .walFileList = strLstNew(),
                .backupFileList = strLstNew(),
                .pgHistory = infoArchivePg(archiveInfo),
                .manifestCipherPass = infoPgCipherPass(infoBackupPg(backupInfo)),
                .walCipherPass = infoPgCipherPass(infoArchivePg(archiveInfo)),
                .archiveIdResultList = lstNewP(sizeof(VerifyArchiveResult), .comparator =  archiveIdComparator),
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
                // Warn if there are no archives or there are no backups in the repo so that the callback need not try to
                // distinguish between having processed all of the list or if the list was missing in the first place
                if (strLstSize(jobData.archiveIdList) == 0 || strLstSize(jobData.backupList) == 0)
                    LOG_WARN_FMT("no %s exist in the repo", strLstSize(jobData.archiveIdList) == 0 ? "archives" : "backups");

                // Set current backup if there is one and verify the archive history on disk is in the database history
                jobData.currentBackup = verifySetBackupCheckArchive(
                    jobData.backupList, backupInfo, jobData.archiveIdList, jobData.pgHistory, &jobData.jobErrorTotal);

                // Create the parallel executor
                ProtocolParallel *parallelExec = protocolParallelNew(
                    (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * MSEC_PER_SEC) / 2, verifyJobCallback, &jobData);

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
                        // String *fileType = strLstGet(filePathLst, 0);
                        strLstRemoveIdx(filePathLst, 0);
                        String *filePathName = strLstJoin(filePathLst, "/");

                        VerifyArchiveResult *archiveIdResult = NULL;

                        // Find the archiveId in the list - assert if not found since this should never happen
                        String *archiveId = strLstGet(filePathLst, 0);
                        unsigned int index = lstFindIdx(jobData.archiveIdResultList, &archiveId);
                        ASSERT(index != LIST_NOT_FOUND);

                        archiveIdResult = lstGet(jobData.archiveIdResultList, index);

                        // The job was successful
                        if (protocolParallelJobErrorCode(job) == 0)
                        {
                            const VerifyResult verifyResult = (VerifyResult)varUIntForce(protocolParallelJobResult(job));

                            if (verifyResult == verifyOk)
                                archiveIdResult->totalValidWal++;
                            else
                            {
                                // Log a warning because the WAL may have gone missing if expire came through and removed it
                                // legitimately so it is not necessarily an error so the jobErrorTotal should not be incremented
                                if (verifyResult == verifyFileMissing)  // {uncovered - unable to cover in test harness}
                                {
                                    LOG_WARN_PID_FMT(                   // {+uncovered}
                                        processId, "%s: %s", strZ(verifyErrorMsg(verifyResult)),
                                        strZ(filePathName));
                                }
                                else
                                {
                                    LOG_ERROR_PID_FMT(
                                        processId, errorTypeCode(&FileInvalidError),
                                        "%s %s", strZ(verifyErrorMsg(verifyResult)), strZ(filePathName));

                                    jobData.jobErrorTotal++;
                                }

                                // Add invalid file with reason from result of verifyFile to range list
                                verifyAddInvalidWalFile(
                                    archiveIdResult->walRangeList, verifyResult, filePathName,
                                    strSubN(strLstGet(filePathLst, strLstSize(filePathLst) - 1), 0, WAL_SEGMENT_NAME_SIZE));
                            }
                        }
                        // Else the job errored
                        else
                        {
                            // Log a protocol error and increment the jobErrorTotal
                            LOG_ERROR_PID_FMT(
                                processId, errorTypeCode(&ProtocolError),
                                "%s %s: [%d] %s", strZ(verifyErrorMsg(verifyOtherError)), strZ(filePathName),
                                protocolParallelJobErrorCode(job), strZ(protocolParallelJobErrorMessage(job)));

                            jobData.jobErrorTotal++;

                            // Add invalid file with "OtherError" reason to range list
                            verifyAddInvalidWalFile(
                                archiveIdResult->walRangeList, verifyOtherError, filePathName,
                                strSubN(strLstGet(filePathLst, strLstSize(filePathLst) - 1), 0, WAL_SEGMENT_NAME_SIZE));
                        }

                        // Free the job
                        protocolParallelJobFree(job);
                    }
                }
                while (!protocolParallelDone(parallelExec));

                // ??? Need to do the final reconciliation - checking backup required WAL against, valid WAL

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

        // ??? Output results but need to make conditional based on an option, not whether there are errors or not
        if (errorTotal == 0 && strSize(result) > 0)
            LOG_INFO_FMT("%s", strZ(result));

        // Throw an error if any encountered
        if (errorTotal > 0)
            THROW_FMT(RuntimeError, "%u fatal errors encountered, see log for details", errorTotal);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
