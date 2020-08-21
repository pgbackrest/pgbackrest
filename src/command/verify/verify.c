/***********************************************************************************************************************************
Verify Command
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

/* https://github.com/pgbackrest/pgbackrest/issues/1032
Add a command to verify the contents of the repository. By the default the command will:

    * Verify that backup/archive.info and their copies are valid
        - May have to expose infoBackupNewLoad so can load the backup.info and then the backup.info.copy and compare the 2 - need to know if one is corrupt and always failing over to the other.
        - want to WARN if only one exists
        - If backup.info/copy AND/OR archive.info/copy is missing then do not abort. ONLY error and abort if encrypted since we can't read the archive dir or backup dirs
        - checkStanzaInfoPg() makes sure the archive and backup info files (not copies just that one or the other) exist and are valid for the database version - BUT the db being available is not allowed because restore doesn't require it - so maybe just check archive and backup info DB sections match each other. BUT this throws an error if can't open one or the other - so would need to catch this and do LOG_ERROR() - maybe prefix with VERIFY-ERROR - or just take out the conditions and check for what we need OR maybe call checkStanzaInfo
        - We should probably check archive and backup history lists match (expire checks that the backup.info history contains at least the same history as archive.info (it can have more)) since we need to get backup db-id needs to be able to translate exactly to the archiveId (i.e. db-id=2, db-version=9.6 in backu.info must translate to archiveId 9.6-2.
        - If archive-copy set the WAL can be in the backup dirs so just because archive.info is missing, don't want to error

    * Reconstruct backup.info and WARN on mismatches - NO because this doesn't really help us since we do not want to take a lock which means things can be added/removed as we're verifying. So instead:
        - Get a list of backup on disk and a list of archive ids and pass that to job data
        - When checking backups, if the label is missing then skip (probably expired from underneath us) and then if it is there but only the manifest.copy exists, then skip (could be aborted or in progress so can't really check anything)
        - It is possible to have a backup without all the WAL if archive-check is not set but in this is not on then all bets are off

    * Check for missing WAL and backup files
        - This would be where we'd need to start the parallel executor, right? YES
        - At this point, we would have valid info files, so, starting with the WAL, get start/stop list for each archiveId so maybe a structure with archiveID and then a start/stop list for gaps? e.g.
            [0] 9.4-1
                [0] start, stop
                                <--- gap
                [1] start, stop
            [1] 10-2
                [0] start, stop

            BUT what does "missing WAL" mean - there can be gaps so "missing" is only if it is expected to be there for a backup to be consistent
        - The filename always include the WAL segment (timeline?) so all I need is the filename. BUT if I have to verify the checksums, so should I do it in the verifyFile function

    * Verify all manifests/copies are valid
        - Both the manifest and manifest.copy exist, loadable and are identical. WARN if any condition is false (this should be in the jobCallback).
         If most recent has only copy, then move on since it could be the latest backup in progress. If missing both, then expired so skip. But if only copy and not the most recent then the backup still needs to be checked since restore will just try to read the manifest BUT it checks the manifest against the backup.info current section so if not in there (than what does restore do? WARN? ERROR?). If main is not there and copy is but it is not the latest then warn that main is missing
         BUT should we skip because backup reconstruct would remove it from current backup.info or should we use it to verify the backups? Meaning go with the copy if not the latest backup and warn? cmdRestore does not reconstruct the backup.info when it loads it.

    * Verify the checksum of all WAL/backup files
        - Pass the checksum to verifyFile - this needs to be stripped off from file in WAL but for backup it must be read from the manifest (which we need to read in the jobCallback
        - Skip checking size for WAL file but should check for size equality with the backup files - if one or the other doesn't match, then corrupt
        - in verifyFile() function be sure to add IoSync filter and then do IoReadDrain and then look at the results of the filters. See restore/file.c lines 85 though 87 (although need more filters e.g. decrypt, decompress, size, sha, sync).

            // Generate checksum for the file if size is not zero
            IoRead *read = NULL;

                read = storageReadIo(storageNewReadP(storageRepo(), filename));
                IoFilterGroup *filterGroup = ioReadFilterGroup(read);

                // Add decryption filter
                if (cipherPass != NULL)
                    ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));


                // Add decompression filter
                if (repoFileCompressType != compressTypeNone)
                    ioFilterGroupAdd(filterGroup, decompressFilter(repoFileCompressType));

                // Add sha1 filter
                ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));

                // Add size filter
                ioFilterGroupAdd(filterGroup, ioSizeNew());

                // Add IoSink - this just throws away data so we don't want to move the data from the remote. The data is thrown away after the file is read, checksummed, etc
                ioFilterGroupAdd(filterGroup, ioSinkNew());

                ioReadDrain(read); // this will throw away if ioSink was not implemented

                // Then filtergroup get result to validate checksum
                if (!strEq(pgFileChecksum, varStr(ioFilterGroupResult(filterGroup, CRYPTO_HASH_FILTER_TYPE_STR))))

        - For the first pass we only check that all the files in the manifest are on disk. Future we may also WARN if there are files that are on disk but not in the manifest.
        - verifyJobResult() would check the status of the file (bad) and can either add to a corrupt list or add gaps in the WAL (probaby former) - I need to know what result I was coming from so that is the jobKey - maybe we use the filename as the key - does it begin with Archive then it is WAL, else it's a backup (wait, what about manifest) - maybe we can make the key a variant instead of a string. Need list of backups start/stop to be put into jobData and use this for final reconciliation.
        - Would this be doing mostly what restoreFile() does without the actualy copy? But we have to "sort-of" copy it (you mentioned a technique where we can just throw it away and not write to disk) to get the correct size and checksum...
        - If a corrupt WAL file then warn/log that we have a "corrupt/missing" file and remove it from the range list so it will cause a "gap" then when checking backup it will log another error that the backup that relies on that particular WAL is not valid

    * Ignore files with references when every backup will be validated, otherwise, check each file referenced to another file in the backup set. For example, if ask for an incremental backup verify, then the manifest may have references to files in another (prior) backup so go check them.

The command can be modified with the following options:

    --set - only verify the specified backup and associated WAL (including PITR)
    --no-pitr only verify the WAL required to make the backup consistent.
    --fast - check only that the file is present and has the correct size.

We would like these additional features, but are willing to release the first version without them:

    * WARN on extra files
    * Check that history files exist for timelines after the first (the first timeline will not have a history file if it is the oldest timeline ever written to the repo)
    * Check that copies of the manifests exist in backup.history

Questions/Concerns
- How much crossover with the Check command are we anticipating?
- The command can be run locally or remotely, so from any pgbackrest host, right? YES
- Data.pm: ALL OF THESE ARE NO
    * Will we be verifying links? Does the cmd_verify need to be in Data.pm CFGOPT_LINK_ALL, CFGOPT_LINK_MAP
    * Same question for tablespaces
- How to check WAL for PITR (e.g. after end of last backup?)? ==> If doing async archive and process max = 8 then could be 8 missing. But we don't have access to process-max and we don't know if they had asynch archiving, so if we see gaps in the last 5 minutes of the WAL stream and the last backup stop WAL is there, then we'll just ignore that PITR has gaps. MUST always assume is we don't have access to the configuration.
*/
/**********************************************************************************************************************************/
// typedef enum // CSHANG Don't think I will need
// {
//     verifyWal,
//     verifyBackup,
// } VerifyState;
//
// typedef struct VerifyData
// {
//     bool fast;                                                      // Has the fast option been requested
//     VerifyState state; // CSHANG Don't think I need this as I will just be checking the Lists and removing(?) each item verified
//         // CSHANG list of WAL / WAL directories / archiveId to check
//         StringList *archiveIdList;
//         StringList *walPathList;
//         StringList *walFileList;
//
//         // CSHANG list of Backup directories/files to check
//         StringList *backupList;
//         StringList *backupFileList;
// // } VerifyData;

// static ProtocolParallelJob *
// verifyJobCallback(void *data, unsigned int clientIdx)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM_P(VOID, data);
//         FUNCTION_TEST_PARAM(UINT, clientIdx);
//     FUNCTION_TEST_END();
//
//     ASSERT(data != NULL); -- or can it be - like when there is no more data
//
//     // No special logic based on the client, we'll just get the next job
//     (void)clientIdx;
//
//     // Get a new job if there are any left
//     VerifyData *jobData = data;
// if archiveIDlist ==Null
//     go get a list of archiveid.
//     done.
//
// if (walPathList size == 0)
//     go read the walpath
//     add all walPathlist
//     remove archiveid from list
//
// if walPathlist > 0
//     go read the walPathlist[i]
//     add all walFilelist
//     Here we check for gaps? Then when check for backup consistency, we warn if a specific backup can't do pitr or can do pitr
//     remove walPathlist[i] from list
//
// maybe have some info log (like "checking archinve id 9.4-1" and then detail level logging could in addition list the archive files being checked).
//
//     if (walFilelist)
//     {
//         process the 1 walFile from the list
//         remove that from the list
//
//         below are the guts from the PROTOCOL_COMMAND_ARCHIVE_GET_STR
//
//         const String *walSegment = strLstGet(jobData->walSegmentList, jobData->walSegmentIdx);
//         jobData->walSegmentIdx++;
//
//         ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_ARCHIVE_GET_STR); // CSHANG Replace with PROTOCOL_COMMAND_VERIFY_FILE_STR
//         protocolCommandParamAdd(command, VARSTR(walSegment));
//
//         FUNCTION_TEST_RETURN(protocolParallelJobNew(VARSTR(walSegment), command));
//     }
// else // OR maybe just have a verifyFile - may be that it just takes a file name and it doesn't know what type of file it is - like pass it the compress type, encryption, etc - whatever it needs to read the file. Make the verifyFile dumb. There is a funtion to get the compression type. BUT can't test compression on the files in backup because need to read the manifest to determine that.
//     if (backupList)
//         {
//             next backup thing to process
//
//             FUNCTION_TEST_RETURN(protocolParallelJobNew(VARSTR(walSegment), command));
//         }
//
//     FUNCTION_TEST_RETURN(NULL);
// }
//
// verifyJobResult(...)
// {
//     // Check that the job was successful
//     if (protocolParallelJobErrorCode(job) == 0)
//     {
//         MEM_CONTEXT_TEMP_BEGIN()
//         {
//             THISMAYBEASTRUCT result = varSOMETHING(protocolParallelJobResult(job));
// // CSHANG This is just an example - we need to do logging based on INFO for what we're checking, DETAIL for each files checked, then BACKUP success, WAL success would also be INFO, WARN logLevelWarn or ERROR logLevelError
//             LOG_PID(logLevelInfo, protocolParallelJobProcessId(job), 0, strZ(log));
//         }
//         MEM_CONTEXT_TEMP_END();
//
//         // Free the job
//         protocolParallelJobFree(job);
//     }
//     else
//         THROW_CODE(protocolParallelJobErrorCode(job), strZ(protocolParallelJobErrorMessage(job)));
// }
//***********************************************************************************************************************************
/* CSHANG NOTES for PITR, For example:
postgres@pg-primary:~$ more /var/lib/pgbackrest/archive/demo/12-1/00000002.history
1	0/501C4D0	before 2020-08-10 17:14:51.275288+00

postgres@pg-primary:~$ more /var/lib/pgbackrest/archive/demo/12-1/00000003.history
1	0/501C4D0	before 2020-08-10 17:14:51.275288+00


2	0/7000000	before 2000-01-01 00:00:00+00

And archive has:
var/lib/pgbackrest/archive/demo/12-1/0000000100000000:
000000010000000000000001-da5d050e95663fe95f52dd5059db341b296ae1fa.gz
000000010000000000000002.00000028.backup
000000010000000000000002-498acf8c1dc48233f305bdd24cbb7bdc970d1268.gz
000000010000000000000003.00000028.backup
000000010000000000000003-b323c5739356590e18aa75c8079ec9ff06cb32b7.gz
000000010000000000000004.00000028.backup
000000010000000000000004-0e54893dcff383538d3f6fd93f59b62e2bb42432.gz
000000010000000000000005-b1cc92d58afd5d6bf3a4a530f72bb9e3d3f2e8f6.gz  <-- timeline switch
000000010000000000000006-a1cc92d58afd5d6bf3a4a530f72bb9e3d3f2e8f6.gz

/var/lib/pgbackrest/archive/demo/12-1/0000000200000000:
000000020000000000000005-74bd3036721ccfdfec648fe1b6fd2cc7b60fe310.gz
000000020000000000000006.00000028.backup
000000020000000000000006-c6d2580ccd6fbd2ee83bb4bf5cb445f673eb17ff.gz <-- timeline switch at 2 0/7 but nothing after this so it won't
                                                            be found but that's OK -or- can't play on timeline 1 to the WAL switch?

/var/lib/pgbackrest/archive/demo/12-1/0000000300000000:
000000030000000000000007-fb920b357b0bccc168b572196dccd42fcca05f53.gz

So if we had a list of timelineWalSwitch:
000000010000000000000005
000000020000000000000007

And created another list of expectedTimelineWal (each switch file but timeline incremented)
000000020000000000000005
000000030000000000000007

So we can check each backup for consistency and when we say that we mean the backup and all of the prior backups it relies on. For
example, if full backup has
start 000000010000000000000001, stop 000000010000000000000004   FULL
start 000000020000000000000005, stop 000000020000000000000006   INCR depends on FULL
The if 000000010000000000000003 is missing/corrupt then FULL and then everything that depends on it must be marked invalid.

Once we know a backup is "consistent" then we need to check how far it can play forward. So, technically, the FULL could play through
the timeline 1 to 000000010000000000000006 (although it is not clear to me how this can be since PG would skip it because PG will be
following the progression through the history file) or through to the current 000000030000000000000007 is we follow the timeline
switches and there are no gaps.

    // Check if there was a timeline change
    if (strCmp(strSubN(range[x]->stop, 0, 8), strSubN(range[x+1], 0, 8)) != 0)
    {
        uint32_t stopTimeline = (uint32_t)strtol(strZ(strSubN(range[x]stop, 0, 8)), NULL, 16);
        uint32_t currentTimeline = (uint32_t)strtol(strZ(strSubN(range[x+1], 0, 8)), NULL, 16);

But the timeline+1 true? Could we skip to timeline 3 if there was a problem with timeline 2? But shouldn't that be in the history file?
        if (currentTimeline == stopTimeline + 1)
        {
*/

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
} VerifyJobData;

/***********************************************************************************************************************************
Load a file into memory
***********************************************************************************************************************************/
static StorageRead *
verifyFileLoad(const String *fileName, const String *cipherPass)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, fileName);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_TEST_END();

// CSHANG But how does this type of reading help with manifest? Won't we still be pulling in the entire file into memory to get the checksum or will I need to chunk it up and add all the checksums together?

    // Read the file and error if missing
    StorageRead *result = storageNewReadP(storageRepo(), fileName);

    // *read points to a location within result so update result with contents based on necessary filters
    IoRead *read = storageReadIo(result);
    cipherBlockFilterGroupAdd(
        ioReadFilterGroup(read), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cipherModeDecrypt, cipherPass);
    ioFilterGroupAdd(ioReadFilterGroup(read), cryptoHashNew(HASH_TYPE_SHA1_STR));

    // If the file is compressed, add a decompression filter
    if (compressTypeFromName(fileName) != compressTypeNone)
        ioFilterGroupAdd(ioReadFilterGroup(read), decompressFilter(compressTypeFromName(fileName)));

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get status of info files in repository
***********************************************************************************************************************************/
static VerifyInfoFile
verifyInfoFile(const String *pathFileName, bool loadFile, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pathFileName);
        FUNCTION_LOG_PARAM(BOOL, loadFile);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(pathFileName != NULL);

    VerifyInfoFile result = {.errorCode = 0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        TRY_BEGIN()
        {
            IoRead *infoRead = storageReadIo(verifyFileLoad(pathFileName, cipherPass));

            if (loadFile)
            {
                if (strBeginsWith(pathFileName, INFO_BACKUP_PATH_FILE_STR))
                {
                    result.backup = infoBackupMove(infoBackupNewLoad(infoRead), memContextPrior());
                }
                else if (strBeginsWith(pathFileName, INFO_ARCHIVE_PATH_FILE_STR))
                {
                    result.archive = infoArchiveMove(infoArchiveNewLoad(infoRead), memContextPrior());
                }
                else
                {
                    result.manifest = manifestMove(manifestNewLoad(infoRead), memContextPrior());
                }
            }
            else
                ioReadDrain(infoRead);      // Drain the io and close it

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
Get the manifest file
***********************************************************************************************************************************/
static Manifest *
verifyManifestFile(const String *backupLabel, const String *cipherPass, bool *currentBackup, const InfoPg *pgHistory)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, backupLabel);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_LOG_PARAM_P(BOOL, currentBackup);
        FUNCTION_LOG_PARAM(INFO_PG, pgHistory);
    FUNCTION_LOG_END();

    Manifest *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *fileName = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel));

        // Get the main manifest file
        VerifyInfoFile verifyManifestInfo = verifyInfoFile(fileName, true, cipherPass);

        // If the main file did not error, then report on the copy's status and check checksums
        if (verifyManifestInfo.errorCode == 0)
        {
            result = verifyManifestInfo.manifest;
            manifestMove(result, memContextPrior());

            // Attempt to load the copy and report on it's status but don't keep it in memory
            VerifyInfoFile verifyManifestInfoCopy = verifyInfoFile(
                strNewFmt("%s%s", strZ(fileName), INFO_COPY_EXT), false, cipherPass);

            // If the copy loaded successfuly, then check the checksums
            if (verifyManifestInfoCopy.errorCode == 0)
            {
                // If the manifest and manifest.copy checksums don't match each other than one (or both) of the files could be
                // corrupt so log a warning but trust main
                if (!strEq(verifyManifestInfo.checksum, verifyManifestInfoCopy.checksum))
                    LOG_WARN("backup.manifest.copy does not match backup.manifest");
            }
        }
        else
        {
            // Attempt to load the copy if this is not the current backup - no attempt is made to check an in-progress backup.
            // currentBackup is only notional until the main file is checked because the backup.info file may not have existed or
            // the backup may have completed by the time we get here. If the main manifest is simply missing, it is assumed
            // the backup is an in-progress backup and verification is skipped, otherwise, it is no longer considered an in-progress
            // backup and an attempt will be made to load the manifest copy.
            if (!(*currentBackup && verifyManifestInfo.errorCode == errorTypeCode(&FileMissingError)))
            {
                *currentBackup = false;

                VerifyInfoFile verifyManifestInfoCopy = verifyInfoFile(
                    strNewFmt("%s%s", strZ(fileName), INFO_COPY_EXT), true, cipherPass);

                // If loaded successfully, then return the copy as usable
                if (verifyManifestInfoCopy.errorCode == 0)
                {
                    LOG_WARN_FMT("%s/backup.manifest is missing or unusable, using copy", strZ(backupLabel));

                    result = verifyManifestInfoCopy.manifest;
                    manifestMove(result, memContextPrior());
                }
            }
        }

        // If found a usable manifest then check that the database it was based on is in the history
        if (result != NULL)
        {
            bool found = false;
            const ManifestData *manData = manifestData(result);

            // Confirm the PG database information from the manifest is in the history list
            for (unsigned int infoPgIdx = 0; infoPgIdx < infoPgDataTotal(pgHistory); infoPgIdx++)
            {
                InfoPgData pgHistoryData = infoPgData(pgHistory, infoPgIdx);

                if (pgHistoryData.id == manData->pgId && pgHistoryData.systemId == manData->pgSystemId &&
                    pgHistoryData.version == manData->pgVersion)
                {
                    found = true;
                    break;
                }
            }

            // If the PG data is not found in the backup.info history, then warn but check all the files anyway
            if (!found)
                LOG_WARN_FMT("'%s' may not be recoverable - PG data is not in the backup.info history", strZ(backupLabel));
        }

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, result);
}

/***********************************************************************************************************************************
Check the history in the info files
***********************************************************************************************************************************/
void
verifyPgHistory(const InfoPg *archiveInfoPg, const InfoPg *backupInfoPg)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_PG, archiveInfoPg);
        FUNCTION_TEST_PARAM(INFO_PG, backupInfoPg);
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
        FUNCTION_TEST_PARAM_P(ARCHIVE_RESULT, archiveIdResult);
        FUNCTION_TEST_PARAM(STRING_LIST, walFileList);
        FUNCTION_TEST_PARAM(LIST, archiveIdResultList);
        FUNCTION_TEST_PARAM_P(UINT, jobErrorTotal);
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
            };
        }

        walFileIdx++;
    }
    while (walFileIdx < strLstSize(walFileList));

    // If walRange containes a range, then add the last walRange to the list
    if (walRange.start != NULL)
        lstAdd(archiveIdResult->walRangeList, &walRange);

    // Now if there are ranges for this archiveId then sort ascending by the stop file and add them
    if (lstSize(archiveIdResult->walRangeList) > 0)
    {
        lstSort(archiveIdResult->walRangeList, sortOrderAsc);
        lstAdd(archiveIdResultList, archiveIdResult);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Verify the job data archives
***********************************************************************************************************************************/
static ProtocolParallelJob *
verifyArchive(void *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
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
/* CSHANG Here I should pobably use an expression to get all the history files too "(^[0-F]{16}$)|(^[0-F]{8}\.history$)" (actually,
given the note below on S3 maybe we get everything and then create lists: history, WALpaths, junk) and then
 Create the stringlist files based on what last history file has: I only need the latest history file since history is continually
 copied from each to the next.

 NOTE!!! The timeline history file format is changed in version 9.3. Formats of versions 9.3 or later and earlier both are shown
 below but not in detail - so need to confirm this.

 Later version 9.3:
 timelineId	LSN	"reason"
 Until version 9.2:
 timelineId	WAL_segment	"reason"
 */
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
/* CSHANG because of S3 and because we may want to report on stuff in the directories that shouldn't be there, we should get
everything and then filter out the WAL via expression for walFileList and any other files we should just call it out in a separate
list. We'll need to do this for all lists for first get everything and then filter on an expression to get the valid stuff and
anything else we should put in a separate list (e.g. archiveIdInvalidFileList) to inform the user of "junk" in the directory.
Note, though, that .partial and .backup should not be considered "junk" in the WAL directory.
*/
                    if (strLstSize(jobData->walFileList) > 0)
                    {
                        // If the WAL segment size for this archiveId has not been set, get it from the first WAL
                        if (archiveIdResult.pgWalInfo.size == 0)
                        {
                            StorageRead *walRead = verifyFileLoad(
                                strNewFmt(
                                    STORAGE_REPO_ARCHIVE "/%s/%s/%s", strZ(archiveIdResult.archiveId), strZ(walPath),
                                    strZ(strLstGet(jobData->walFileList, 0))),
                                jobData->walCipherPass);

                            PgWal walInfo = pgWalFromBuffer(storageGetP(walRead, .exactSize = PG_WAL_HEADER_SIZE));

                            archiveIdResult.pgWalInfo.size = walInfo.size;
                            archiveIdResult.pgWalInfo.version = walInfo.version;
                        }

                        archiveIdResult.totalWalFile += strLstSize(jobData->walFileList);

                        createArchiveIdRange(
                            &archiveIdResult, jobData->walFileList, jobData->archiveIdResultList, &jobData->jobErrorTotal);
                    }
                }

    // CSHANG we will have to check every file - pg_resetwal can change the wal segment size at any time - grrrr. We can spot check in each timeline by checking the first file, but that won't help as we'll just wind up with a bunch of ranges since the segment size will stop matching at some point.  If WAL segment size is reset, then can't do PITR.
    /* CSHANG per David:
    16MB is the whole segment.
    The header is like 40 bytes.
    What we need is for pgWalFromFile() to look more like pgControlFromFile().
    i.e. takes a storage object and reads the first few bytes from the file.
    But, if you just want to read the first 512 bytes for now and use pgWalFromBuffer() then that's fine. We can improve that later.
    But people do use pg_resetwal just to change the WAL size. You won't lose data if you do a clean shutdown and then pg_resetwal.
    So, for our purposes the size should never change for a db-id.
    We'll need to put in code to check if the size changes and force them to do a stanza-upgrade in that case. (<- in archive push/get)
    So, for now you'll need to check the first WAL and use that size to verify the sizes of the rest. Later we'll pull size info from archive.info.
    */
                if (strLstSize(jobData->walFileList) > 0)
                {
                    do
                    {
                        const String *fileName = strLstGet(jobData->walFileList, 0);

                        // Get the fully qualified file name
                        const String *filePathName = strNewFmt(
                            STORAGE_REPO_ARCHIVE "/%s/%s/%s", strZ(archiveIdResult.archiveId), strZ(walPath), strZ(fileName));

                        // Get the checksum from the file name
                        String *checksum = strSubN(fileName, WAL_SEGMENT_NAME_SIZE + 1, HASH_TYPE_SHA1_SIZE_HEX);

                        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_VERIFY_FILE_STR);
                        protocolCommandParamAdd(command, VARSTR(filePathName));
                        protocolCommandParamAdd(command, VARSTR(checksum));
                        protocolCommandParamAdd(command, VARBOOL(false));                   // Can the size be verified?
                        protocolCommandParamAdd(command, VARUINT64(0));                     // File size to verify
                        protocolCommandParamAdd(command, VARSTR(jobData->walCipherPass));

                        // Assign job to result
                        result = protocolParallelJobNew(VARSTR(fileName), command);
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
                    // Log no WAL exists in the WAL path and remove the WAL path from the list (nothing to process)
                    LOG_WARN_FMT("path '%s/%s' is empty", strZ(archiveIdResult.archiveId), strZ(walPath));
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
            LOG_WARN_FMT("path '%s' is empty", strZ(archiveIdResult.archiveId));
            strLstRemoveIdx(jobData->archiveIdList, 0);

            // Add to the results for completeness
            lstAdd(jobData->archiveIdResultList, &archiveIdResult);
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Verify the job data backups
***********************************************************************************************************************************/
static ProtocolParallelJob *
verifyBackup(void *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
    FUNCTION_TEST_END();

    ProtocolParallelJob *result = NULL;

    VerifyJobData *jobData = data;

    // Process backup files, if any
    while (strLstSize(jobData->backupList) > 0)
    {
        result == NULL;

        BackupResult backupResult =
        {
            .backupLabel = strDup(strLstGet(jobData->backupList, 0)),
        };

        bool inProgressBackup = strEq(jobData->currentBackup, backupResult.backupLabel);

        // Get a usable backup manifest file
        Manifest *manifest = verifyManifestFile(
            backupResult.backupLabel, jobData->manifestCipherPass, &inProgressBackup, jobData->pgHistory);

        // If a usable backup.manifest file is not found
        if (manifest == NULL)
        {
            // Warn if it is not actually the current in-progress backup
            if (!inProgressBackup)
            {
                backupResult.status = backupMissingManifest;

                LOG_WARN_FMT("Manifest files missing for '%s' - backup may have expired", strZ(backupResult.backupLabel));
            }
            else
            {
                backupResult.status = backupInProgress;

                LOG_INFO_FMT("backup '%s' appears to be in progress, skipping", strZ(backupResult.backupLabel));
            }

            // Update the result status and skip
            lstAdd(jobData->backupResultList, &backupResult);

            // Remove this backup from the processing list
            strLstRemoveIdx(jobData->backupList, 0);
        }
        // Else process the files in the manifest
        else
        {
/* CSHANG questions:
1) What sections of the manifest can we really verify? I don't want to read the manifest again if I can help it so now that we have it,
what sections can we check?
    [backrest] - NO
    [backup] - maybe we should save all or a subset of this information in the result to check later? Definitely archive-start/stop but is anything else important?
                ==> No, start/stop all we need, at least for now
        backup-archive-start="000000010000000000000002"
        backup-archive-stop="000000010000000000000002"
        backup-label="20191112-173146F"
        backup-lsn-start="0/2000028"
        backup-lsn-stop="0/2000130"
        backup-timestamp-copy-start=1573579908
        backup-timestamp-start=1573579906
        backup-timestamp-stop=1573579919
        backup-type="full"
    [backup:option] - is there anything here we can/should check? If so, how?
                ==> No. Most of these are informational, and if something important like compress-type is wrong we’ll know it when we check the files.
        option-archive-check=true
        option-archive-copy=false
        option-backup-standby=false
        option-buffer-size=1048576
        option-checksum-page=true
        option-compress=true
        option-compress-level=6
        option-compress-level-network=3
        option-compress-type="gz"
        option-delta=false
        option-hardlink=false
        option-online=true
        option-process-max=1
    [backup:target] - NO (on the db so we can't check that)
    [db] - NO (on the db so we can't check that)
    [target:file] - definitely - this is the crux
        pg_data/PG_VERSION={"checksum":"ad552e6dc057d1d825bf49df79d6b98eba846ebe","master":true,"reference":"20200810-171426F","repo-size":23,"size":3,"timestamp":1597079647}
        pg_data/global/6100_vm={"checksum-page":true,"repo-size":20,"size":0,"timestamp":1574780487}
        pg_data/global/6114={"checksum":"348b9fc06b2db1b0e547bcf51ec7c7715143cd93","checksum-page":true,"repo-size":78,"size":8192,"timestamp":1574780487}
        pg_data/global/6115={"checksum":"0a1eda482229f7dfec6ec7083362ad2e80ce2262","checksum-page":true,"repo-size":76,"size":8192,"timestamp":1574780487}
        pg_data/global/pg_control={"checksum":"edb8d7e62358c8a0538d2a209da8ae76f289a7e0","master":true,"repo-size":213,"size":8192,"timestamp":1574780832}
        pg_data/global/pg_filenode.map={"checksum":"1b85310413a1541d7a326c2dbc3d0283b7da0f60","repo-size":136,"size":512,"timestamp":1574780487}
        pg_data/pg_logical/replorigin_checkpoint={"checksum":"347fc8f2df71bd4436e38bd1516ccd7ea0d46532","master":true,"repo-size":28,"size":8,"timestamp":1574780832}
        pg_data/pg_multixact/members/0000={"checksum":"0631457264ff7f8d5fb1edc2c0211992a67c73e6","repo-size":43,"size":8192,"timestamp":1574780487}
        pg_data/pg_multixact/offsets/0000={"checksum":"0631457264ff7f8d5fb1edc2c0211992a67c73e6","repo-size":43,"size":8192,"timestamp":1574780703}
        pg_data/pg_xact/0000={"checksum":"535bf8d445838537821cb3cb60c89e814a6287e6","repo-size":49,"size":8192,"timestamp":1574780824}
        pg_data/postgresql.auto.conf={"checksum":"c6b93d1c2880daea891c8879c86d167c6678facf","master":true,"repo-size":103,"size":88,"timestamp":1574780487}
        pg_data/tablespace_map={"checksum":"e71a909fe617319cff5c4463e0a65e056054d94b","master":true,"size":30,"timestamp":1574780855}
        pg_tblspc/16385/PG_10_201707211/16386/112={"checksum":"c0330e005f13857f54da25099ef919ca0c143cb6","checksum-page":true,"repo-size":74,"size":8192,"timestamp":1574780704}
    [target:file:default] - NO unless it is necessary for checking permissions?
    [target:link] - NO
    [target:link:default] - NO
    [target:path] - NO
    [target:path:default] - NO

2) If there are "reference" to prior backup, we should be able to skip checking them, no?
    ==> Yes, definitely skip them or we’ll just be doing the same work over and over.
*/
LOG_WARN("Processing MANIFEST"); // CSHANG Remove

            const ManifestData *manData = manifestData(const Manifest *this);
            backupResult.archiveStart = strDup(manData->archiveStart);
            backupResult.archiveStop = strDup(manData->archiveStop); // May not have this?

            // Get the cipher subpass used to decrypt files in the backup
            jobData->backupCipherPass = manifestCipherSubPass(jobData->manifest);
// CSHANG Do I need something like compress-type or level from the backup:option? Maybe compress-type is something we pass to verifyFile
// CSHANG It is possible to have a backup without all the WAL if option-archive-check=true is not set but in this is not on then all bets are off

// CSHANG Should free the manifest after complete here in order to get it out of memory and start on a new one
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
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(UINT, clientIdx);
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

    // Process backups - get manifest and verify it first thru function here vs sending verifyFile, log errors and incr job error
    if (jobData->backupProcessing)
    {
        result = verifyBackup(data);

        // If there is a result from backups, then return it
        if (result != NULL)
            FUNCTION_TEST_RETURN(result);
    }

    // }
    // MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
static String *
verifyErrorMsg(VerifyResult verifyResult)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, verifyResult);
    FUNCTION_TEST_END();

    String *result = NULL;
/* CSHANG TODO: Make these better (e.g. actual checksum does not match expected checksum")
 and maybe LOG_ERROR/WARN if appropriate to do one vs the other.
 */
    switch (verifyResult)
    {
        case verifyFileMissing:
        {
            result = strNew("file missing");
            break;
        }
        case verifyChecksumMismatch:
        {
            result = strNew("invalid checksum");
            break;
        }
        case verifySizeInvalid:
        {
            result = strNew("invalid size");
            break;
        }
        default:
        {
            result = strNew("unknown error");
            break;
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Render the results of the verify command
***********************************************************************************************************************************/
static String *
verifyRender(void *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);

    VerifyJobData *jobData = data;

    String *result = strNew("Results:\n");

    for (unsigned int archiveIdx = 0; archiveIdx < lstSize(jobData->archiveIdResultList); archiveIdx++)
    {
        ArchiveResult *archiveIdResult = lstGet(jobData->archiveIdResultList, archiveIdx);
        strCatFmt(
            result, "  archiveId: %s, total WAL files checked: %u\n", strZ(archiveIdResult->archiveId),
            archiveIdResult->totalWalFile);

        if (archiveIdResult->totalWalFile > 0)
        {
            unsigned int errMissing = 0;
            unsigned int errChecksum = 0;
            unsigned int errSize = 0;

            for (unsigned int walIdx = 0; walIdx < lstSize(archiveIdResult->walRangeList); walIdx++)
            {
                WalRange *walRange = lstGet(archiveIdResult->walRangeList, walIdx);

                LOG_DETAIL_FMT(
                    "archiveId: %s, wal start: %s, wal stop: %s", strZ(archiveIdResult->archiveId), strZ(walRange->start),
                    strZ(walRange->stop));

                unsigned int invalidIdx = 0;

                while (walRange->invalidFileList != NULL && invalidIdx < lstSize(walRange->invalidFileList))
                {
                    InvalidFile *invalidFile = lstGet(walRange->invalidFileList, invalidIdx);

                    switch (invalidFile->reason)
                    {
                        case verifyFileMissing:
                        {
                            errMissing++;;
                            break;
                        }
                        case verifyChecksumMismatch:
                        {
                            errChecksum++;
                            break;
                        }
                        case verifySizeInvalid:
                        {
                            errSize++;
                            break;
                        }
                        case verifyOk:
                        {
                            break;
                        }
                    }

                    invalidIdx++;
                }
            }

            strCatFmt(result, "    missing: %u, checksum invalid: %u, size invalid: %u\n", errMissing, errChecksum, errSize);
        }
    }

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
        FUNCTION_TEST_PARAM(STRING_LIST, backupList);
        FUNCTION_TEST_PARAM(INFO_BACKUP, backupInfo);
        FUNCTION_TEST_PARAM(STRING_LIST, archiveIdList);
        FUNCTION_TEST_PARAM(INFO_PG, pgHistory);
        FUNCTION_TEST_PARAM_P(UINT, jobErrorTotal);
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

            for (unsigned int histIdx = 0;  histIdx < infoPgDataTotal(pgHistory); histIdx++)
            {
                strLstAdd(archiveIdHistoryList, infoPgArchiveId(pgHistory, histIdx));
            }

            // Get all archiveIds on disk that are not in the history list
            String *missingFromHistory = strLstJoin(
                strLstMergeAnti(archiveIdList,
                    strLstSort(strLstComparatorSet(archiveIdHistoryList, archiveIdComparator), sortOrderAsc)), ", ");

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
Process the verify command
***********************************************************************************************************************************/
static String *
verifyProcess(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        unsigned int errorTotal = 0;
        String *resultStr = strNew("");

        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        const Storage *storage = storageRepo();

        // Get a usable backup info file
        InfoBackup *backupInfo = verifyBackupInfoFile();

        // If a usable backup.info file is not found, then report an error in the log
        if (backupInfo == NULL)
        {
            LOG_ERROR(errorTypeCode(&FormatError), "No usable backup.info file");
            errorTotal++;
        }

        // Get a usable archive info file
        InfoArchive *archiveInfo = verifyArchiveInfoFile();

        // If a usable archive.info file is not found, then report an error in the log
        if (archiveInfo == NULL)
        {
            LOG_ERROR(errorTypeCode(&FormatError), "No usable archive.info file");
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
                LOG_ERROR(errorTypeCode(&FormatError), errorMessage());
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
                .walPathList = strLstNew(),  // cshang need to create memcontex and later after processing loop, memContextDiscard(); see manifest.c line 1793
                .walFileList = strLstNew(),
                .backupFileList = strLstNew(),
                .pgHistory = infoArchivePg(archiveInfo),
                .manifestCipherPass = infoPgCipherPass(infoBackupPg(backupInfo)),
                .walCipherPass = infoPgCipherPass(infoArchivePg(archiveInfo)),
                .archiveIdResultList = lstNewP(sizeof(ArchiveResult), .comparator =  archiveIdComparator),
            };
/* CSHANG Should we be sorting the backups newest to oldest? That way we can just filter off the first one if it is a current
"in progress" backup and maybe LOG_WARN or LOG_INFO that skipping? That way we don't need separate logic for backup. BUT the
problem will still always be that the current backup may complete before we're done checking the archives. I feel like we need to
draw the line somewhere...or do we feel we need to check that it is not in the backup.info current list AND that it only has a copy
AND is the newest backup?

CSHANG maybe backu.info should be the ground truth. David says problems with backup.info reconstruct and that restore does not use
so it's why we're not doing it here but we keep going around whether we should and whether backup.info should be the ground truth.
I was thinking maybe we figure out what backups to check (pare down the list) BEFORE we start processing. Specifically, if we were
able to determine if the list has holes in any dependency chain. Does reconstruct do this? If originally had F1 D1 I1 then dependency
chain is F1 D1 I1 but when I go to check, and D1 is gone, then the I1 is no loner valid.
*/
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
                        const String *fileName = varStr(protocolParallelJobKey(job)); // CSHANG Actually, can probably make this the full filename again bcause we can just split the string on the forward slashes

                        // CSHANG The key will tell us what we just processed
                        // const VerifyResult verifyResult = (VerifyResult)varUIntForce(varLstGet(jobResult, 0))
                        // const String *filename = varStr(varLstGet(varVarLst(protocolParallelJobKey(job)), 1);
                        // const ENUM type = varUInt(varLstGet(varVarLst(protocolParallelJobKey(job)), 2); -- May need a type?
                        //
                        //
                        // The job was successful
                        if (protocolParallelJobErrorCode(job) == 0)
                        {
                            const VariantList *const jobResult = varVarLst(protocolParallelJobResult(job));
                            const VerifyResult verifyResult = (VerifyResult)varUIntForce(varLstGet(jobResult, 0));
                            const String *const filePathName = varStr(varLstGet(jobResult, 1));

                            if (verifyResult != verifyOk)
                            {
// CSHANG Should this be made a LOG_ERROR and the error count incremented? errorTotal is "fatal" error, but if the file went missing, it's not always an error - could be that expire came through and legitimately removed it so maybe only error on other than missing?
                                LOG_WARN_PID_FMT(processId, "%s: %s", strZ(verifyErrorMsg(verifyResult)), strZ(filePathName));

                                StringList *filePathLst = strLstNewSplit(filePathName, FSLASH_STR);

                                if (strEq(strLstGet(filePathLst, 0), STORAGE_REPO_ARCHIVE_STR))
                                {
                                    String *archiveId = strLstGet(filePathLst, 1);
                                    unsigned int index = lstFindIdx((List *)jobData.archiveIdResultList, &archiveId);

                                    if (index != LIST_NOT_FOUND)
                                    {
                                        ArchiveResult *archiveIdResult = lstGet(jobData.archiveIdResultList, index);

                                        // Get the WAL segment from the file name
                                        String *walSegment = strSubN(
                                            strLstGet(filePathLst, strLstSize(filePathLst) - 1), 0, WAL_SEGMENT_NAME_SIZE);

// CSHANG Might need a check for walRangeList > 0? If we get a file for this archiveId but we didn't have a walRange - no that can't happen. We're going from the list of filenames so if we get a result for a filename the corresponding archivId must have a range.

                                        for (unsigned int walIdx = 0; walIdx < lstSize(archiveIdResult->walRangeList); walIdx++)
                                        {
                                            WalRange *walRange = lstGet(archiveIdResult->walRangeList, walIdx);

                                            // If the file is less/equal to the stop file then it falls in this range since ranges
                                            // are sorted by stop file in ascending order, therefore first one found is the range.
                                            if (strCmp(walRange->stop, walSegment) >= 0)
                                            {
                                                InvalidFile invalidFile =
                                                {
                                                    .fileName = strDup(fileName),  // CSHANG Should this be filePathName?
                                                    .reason = verifyResult,
                                                };

                                                // Add the file to the range where it was found and exit the loop
                                                lstAdd(walRange->invalidFileList, &invalidFile);
                                                break;
                                            }
                                        }
                                    }
                                    else
                                    {
// CSHANG If the archive Id doesn't exist, it then log an error (this should never happen) so do we really need this and if so, then maybe break out as a fatal error?
                                        LOG_ERROR(errorTypeCode(&PathNotEmptyError), "NOT SURE IF SHOULD BE CHECKING FOR THIS...");
                                        errorTotal++;
                                    }
                                }

                                /* CSHANG pseudo code:
                                    Get the type of file this is (e.g. backup or archive file - probably just split the string into a stringlist by "/" and check that stringlist[0] = STORAGE_REPO_ARCHIVE or STORAGE_REPO_BACKUP)

                                    If archive file
                                        Get the archiveId from the filePathName (stringList[1] - is there a better way?)
                                        Loop through verifyResultData.archiveIdResultList and find the archiveId
                                        If found, find the range in which this file falls into and add the file name and the reason to the invalidFileList

                                    If backup file
                                        Get the backup label from the filePathName (stringList[1] - is there a better way?)
                                        Loop through verifyResultData.backupList and find the backup label - NO we are removing the labels as we go along, so we will need to build the result
                                        If found, add the file name and the reason to the invalidFileList


                                    Final stage, after all jobs are complete, is to reconcile the archive with the backup data which, it seems at this pioint is just determining if the backup is 1) consistent (no gaps) 2) can run through PITR (trickier - not sure what this would look like....)
                                    Let's say we have archives such that walList Ranges are:
                                    start 000000010000000000000001, stop 000000010000000000000005
                                    start 000000020000000000000005, stop 000000020000000000000006
                                    start 000000030000000000000007, stop 000000030000000000000007
NOTE After all jobs complete: If invalidFileList not NULL (or maybe size > 0) then there is a problem in this range
                                    PROBLEM: I am generating WAL ranges by timeline so in the above, because we are in a new timeline, it looks like a gap. So how would I determine that the following is OK? Would MUST use the archive timeline history file to confirm that indeed there are no actual gaps in the WAL

        full backup: 20200810-171426F
            wal start/stop: 000000010000000000000002 / 000000010000000000000002

        diff backup: 20200810-171426F_20200810-171442D
            wal start/stop: 000000010000000000000003 / 000000010000000000000003
            backup reference list: 20200810-171426F

        diff backup: 20200810-171426F_20200810-171445D
            wal start/stop: 000000010000000000000004 / 000000010000000000000004
            backup reference list: 20200810-171426F

        incr backup: 20200810-171426F_20200810-171459I
            wal start/stop: 000000020000000000000006 / 000000020000000000000006

/var/lib/pgbackrest/archive/demo/12-1/0000000100000000:
total 2280
-rw-r----- 1 postgres postgres 1994249 Aug 10 17:14 000000010000000000000001-da5d050e95663fe95f52dd5059db341b296ae1fa.gz
-rw-r----- 1 postgres postgres     370 Aug 10 17:14 000000010000000000000002.00000028.backup
-rw-r----- 1 postgres postgres   73388 Aug 10 17:14 000000010000000000000002-498acf8c1dc48233f305bdd24cbb7bdc970d1268.gz
-rw-r----- 1 postgres postgres     370 Aug 10 17:14 000000010000000000000003.00000028.backup
-rw-r----- 1 postgres postgres   73365 Aug 10 17:14 000000010000000000000003-b323c5739356590e18aa75c8079ec9ff06cb32b7.gz
-rw-r----- 1 postgres postgres     370 Aug 10 17:14 000000010000000000000004.00000028.backup
-rw-r----- 1 postgres postgres   73382 Aug 10 17:14 000000010000000000000004-0e54893dcff383538d3f6fd93f59b62e2bb42432.gz
-rw-r----- 1 postgres postgres  105843 Aug 10 17:14 000000010000000000000005-b1cc92d58afd5d6bf3a4a530f72bb9e3d3f2e8f6.gz

/var/lib/pgbackrest/archive/demo/12-1/0000000200000000:
total 192
-rw-r----- 1 postgres postgres 115133 Aug 10 17:15 000000020000000000000005-74bd3036721ccfdfec648fe1b6fd2cc7b60fe310.gz
-rw-r----- 1 postgres postgres    370 Aug 10 17:15 000000020000000000000006.00000028.backup
-rw-r----- 1 postgres postgres  73368 Aug 10 17:15 000000020000000000000006-c6d2580ccd6fbd2ee83bb4bf5cb445f673eb17ff.gz

/var/lib/pgbackrest/archive/demo/12-1/0000000300000000:
total 76
-rw-r----- 1 postgres postgres 74873 Aug 10 17:15 000000030000000000000007-fb920b357b0bccc168b572196dccd42fcca05f53.gz
                                */


                                // CSHANG No - maybe what we need to do is just store the full names in a list because we have to know which DB-ID the wal belongs to and tie that back to the backup data (from the manifest file) A: David says we shouldn't be tying back to backup.info, but rather the manifest - which is where the data in backup.info is coming from anyway
                                // CSHANG and what about individual backup files, if any one of them is invalid (or any gaps in archive), that entire backup needs to be marked invalid, right? So maybe we need to be creating a list of invalid backups such that String *strLstAddIfMissing(StringList *this, const String *string); is called when we find a backup that is not good. And remove from the jobdata.backupList()?

                            }
                        }
                        // CSHANG I am assuming that there will only be an error if something went horribly wrong and the program needs to exit...
                        // Else the job errored
                        else
                            THROW_CODE(protocolParallelJobErrorCode(job), strZ(protocolParallelJobErrorMessage(job)));

                        // Free the job
                        protocolParallelJobFree(job);
                    }
                }
                while (!protocolParallelDone(parallelExec));

                // HERE we will need to do the final reconciliation - checking backup required WAL against, valid WAL

                // Report results
                if (lstSize(jobData.archiveIdResultList) > 0)
                    resultStr = verifyRender(&jobData);
            }
            else
                LOG_WARN("no archives or backups exist in the repo");

            errorTotal += jobData.jobErrorTotal;
        }

        // Throw an error if cannot proceed
        if (errorTotal > 0)
            THROW_FMT(RuntimeError, "%u fatal errors encountered, see log for details", errorTotal);

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
        ioFdWriteOneStr(STDOUT_FILENO, verifyProcess());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
