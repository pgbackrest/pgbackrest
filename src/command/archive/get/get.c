/***********************************************************************************************************************************
Archive Get Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "command/archive/common.h"
#include "command/archive/get/file.h"
#include "command/archive/get/protocol.h"
#include "command/command.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/wait.h"
#include "config/config.h"
#include "config/exec.h"
#include "info/infoArchive.h"
#include "postgres/interface.h"
#include "protocol/helper.h"
#include "protocol/parallel.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Check for a list of archive files in the repository
***********************************************************************************************************************************/
typedef struct ArchiveFileMap
{
    const String *request;                                          // Archive file requested by archive_command
    const String *actual;                                           // Actual file in the repo (with path, checksum, ext, etc.)
} ArchiveFileMap;

typedef struct ArchiveGetCheckResult
{
    List *archiveFileMapList;                                       // List of mapped archive files, i.e. found in the repo

    const String *archiveId;                                        // Repo archive id
    CipherType cipherType;                                          // Repo cipher type
    const String *cipherPassArchive;                                // Repo archive cipher pass

    const ErrorType *errorType;                                     // Error type if there was an error
    const String *errorFile;                                        // Error file if there was an error
    const String *errorMessage;                                     // Error message if there was an error
} ArchiveGetCheckResult;

// Helper to find a single archive file in the repository using a cache to speed up the process and minimize storageListP() calls
typedef struct ArchiveGetFindCache
{
    const String *path;
    const StringList *fileList;
} ArchiveGetFindCache;

static bool
archiveGetFind(
    const String *archiveFileRequest, const String *archiveId, ArchiveGetCheckResult *getCheckResult, List *cache, bool single)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, archiveFileRequest);
        FUNCTION_LOG_PARAM(STRING, archiveId);
        FUNCTION_LOG_PARAM_P(VOID, getCheckResult);
        FUNCTION_LOG_PARAM(LIST, cache);
        FUNCTION_LOG_PARAM(BOOL, single);
    FUNCTION_LOG_END();

    ASSERT(archiveFileRequest != NULL);
    ASSERT(archiveId != NULL);
    ASSERT(getCheckResult != NULL);

    ArchiveFileMap archiveFileMap = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If a WAL segment search among the possible file names
        if (walIsSegment(archiveFileRequest))
        {
            // Get the path
            const String *path = strSubN(archiveFileRequest, 0, 16);

            // List to hold matches for the requested file
            StringList *matchList = NULL;

            // If a single file is requested then optimize by adding a more restrictive expression to reduce network bandwidth
            if (single)
            {
                matchList = storageListP(
                    storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(archiveId), strZ(path)),
                    .expression = strNewFmt(
                        "^%s%s-[0-f]{40}" COMPRESS_TYPE_REGEXP "{0,1}$", strZ(strSubN(archiveFileRequest, 0, 24)),
                            walIsPartial(archiveFileRequest) ? WAL_SEGMENT_PARTIAL_EXT : ""));
            }
            // Else multiple files will be requested so cache list results
            else
            {
                // Partial files cannot be in a list with multiple requests
                ASSERT(!walIsPartial(archiveFileRequest));

                // If the path does not exist in the cache then fetch it
                const ArchiveGetFindCache *cachePath = lstFind(cache, &path);

                if (cachePath == NULL)
                {
                    MEM_CONTEXT_BEGIN(lstMemContext(cache))
                    {
                        cachePath = lstAdd(
                            cache,
                            &(ArchiveGetFindCache)
                            {
                                .path = strDup(path),
                                .fileList = storageListP(
                                    storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(archiveId), strZ(path)),
                                    .expression = strNewFmt("^%s[0-F]{8}-[0-f]{40}" COMPRESS_TYPE_REGEXP "{0,1}$", strZ(path))),
                            });
                    }
                    MEM_CONTEXT_END();
                }

                // Get a list of all WAL segments that match
                matchList = strLstNew();

                for (unsigned int fileIdx = 0; fileIdx < strLstSize(cachePath->fileList); fileIdx++)
                {
                    if (strBeginsWith(strLstGet(cachePath->fileList, fileIdx), archiveFileRequest))
                        strLstAdd(matchList, strLstGet(cachePath->fileList, fileIdx));
                }
            }

            // If there is a single result then return it
            if (strLstSize(matchList) == 1)
            {
                MEM_CONTEXT_BEGIN(lstMemContext(getCheckResult->archiveFileMapList))
                {
                    archiveFileMap.actual = strNewFmt(
                        "%s/%s/%s", strZ(archiveId), strZ(path), strZ(strLstGet(matchList, 0)));
                }
                MEM_CONTEXT_END();
            }
            // Else error if there are multiple results
            if (strLstSize(matchList) > 1)
            {
                MEM_CONTEXT_BEGIN(lstMemContext(getCheckResult->archiveFileMapList))
                {
                    getCheckResult->errorType = &ArchiveDuplicateError;
                    getCheckResult->errorFile = strDup(archiveFileRequest);
                    getCheckResult->errorMessage = strNewFmt(
                        "duplicates found in archive for WAL segment %s: %s\n"
                            "HINT: are multiple primaries archiving to this stanza?",
                        strZ(archiveFileRequest), strZ(strLstJoin(strLstSort(matchList, sortOrderAsc), ", ")));
                }
                MEM_CONTEXT_END();
            }
        }
        // Else if not a WAL segment, see if it exists in the archive dir
        else if (storageExistsP(storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(archiveId), strZ(archiveFileRequest))))
        {
            MEM_CONTEXT_BEGIN(lstMemContext(getCheckResult->archiveFileMapList))
            {
                archiveFileMap.actual = strNewFmt("%s/%s", strZ(archiveId), strZ(archiveFileRequest));
            }
            MEM_CONTEXT_END();
        }

        if (archiveFileMap.actual != NULL)
        {
            MEM_CONTEXT_BEGIN(lstMemContext(getCheckResult->archiveFileMapList))
            {
                archiveFileMap.request = strDup(archiveFileRequest);
            }
            MEM_CONTEXT_END();

            lstAdd(getCheckResult->archiveFileMapList, &archiveFileMap);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, archiveFileMap.actual != NULL);
}

static ArchiveGetCheckResult
archiveGetCheck(const StringList *archiveRequestList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_LIST, archiveRequestList);
    FUNCTION_LOG_END();

    ASSERT(archiveRequestList != NULL);
    ASSERT(strLstSize(archiveRequestList) > 0);

    ArchiveGetCheckResult result = {.archiveFileMapList = lstNewP(sizeof(ArchiveFileMap))};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get pg control info
        PgControl controlInfo = pgControlFromFile(storagePg());

        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();

        result.cipherType = cipherType(cfgOptionStr(cfgOptRepoCipherType));

        // Attempt to load the archive info file
        InfoArchive *info = infoArchiveLoadFile(
            storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, result.cipherType, cfgOptionStrNull(cfgOptRepoCipherPass));

        // Loop through the pg history and determine which archiveId to use based on the first file in the list
        bool found = false;
        const String *archiveId = NULL;
        List *cache = NULL;

        for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoArchivePg(info)); pgIdx++)
        {
            InfoPgData pgData = infoPgData(infoArchivePg(info), pgIdx);

            // Only use the archive id if it matches the current cluster
            if (pgData.systemId == controlInfo.systemId && pgData.version == controlInfo.version)
            {
                archiveId = infoPgArchiveId(infoArchivePg(info), pgIdx);
                cache = lstNewP(sizeof(ArchiveGetFindCache), .comparator = lstComparatorStr);

                found = archiveGetFind(
                    strLstGet(archiveRequestList, 0), archiveId, &result, cache, strLstSize(archiveRequestList) == 1);

                // If the file was found then use this archiveId for the rest of the files
                if (found)
                    break;
            }
        }

        // Error if no archive id was found -- this indicates a mismatch with the current cluster
        if (archiveId == NULL)
        {
            THROW_FMT(
                ArchiveMismatchError, "unable to retrieve the archive id for database version '%s' and system-id '%" PRIu64 "'",
                strZ(pgVersionToStr(controlInfo.version)), controlInfo.systemId);
        }

        // Continue only if the first file was found
        if (found)
        {
            // Copy repo data to result
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.archiveId = strDup(archiveId);
                result.cipherPassArchive = strDup(infoArchiveCipherPass(info));
            }
            MEM_CONTEXT_PRIOR_END();

            // Find the rest of the files in the list
            for (unsigned int archiveRequestIdx = 1; archiveRequestIdx < strLstSize(archiveRequestList); archiveRequestIdx++)
            {
                if (!archiveGetFind(strLstGet(archiveRequestList, archiveRequestIdx), archiveId, &result, cache, false))
                    break;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/***********************************************************************************************************************************
Clean the queue and prepare a list of WAL segments that the async process should get
***********************************************************************************************************************************/
static StringList *
queueNeed(const String *walSegment, bool found, uint64_t queueSize, size_t walSegmentSize, unsigned int pgVersion)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(BOOL, found);
        FUNCTION_LOG_PARAM(UINT64, queueSize);
        FUNCTION_LOG_PARAM(SIZE, walSegmentSize);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
    FUNCTION_LOG_END();

    ASSERT(walSegment != NULL);

    StringList *result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Determine the first WAL segment for the async process to get.  If the WAL segment requested by
        // PostgreSQL was not found then use that.  If the segment was found but the queue is not full then
        // start with the next segment.
        const String *walSegmentFirst =
            found ? walSegmentNext(walSegment, walSegmentSize, pgVersion) : walSegment;

        // Determine how many WAL segments should be in the queue.  The queue total must be at least 2 or it doesn't make sense to
        // have async turned on at all.
        unsigned int walSegmentQueueTotal = (unsigned int)(queueSize / walSegmentSize);

        if (walSegmentQueueTotal < 2)
            walSegmentQueueTotal = 2;

        // Build the ideal queue -- the WAL segments we want in the queue after the async process has run
        StringList *idealQueue = walSegmentRange(walSegmentFirst, walSegmentSize, pgVersion, walSegmentQueueTotal);

        // Get the list of files actually in the queue
        StringList *actualQueue = strLstSort(
            storageListP(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN_STR, .errorOnMissing = true), sortOrderAsc);

        // Only preserve files that match the ideal queue. error/ok files are deleted so the async process can try again.
        RegExp *regExpPreserve = regExpNew(strNewFmt("^(%s)$", strZ(strLstJoin(idealQueue, "|"))));

        // Build a list of WAL segments that are being kept so we can later make a list of what is needed
        StringList *keepQueue = strLstNew();

        for (unsigned int actualQueueIdx = 0; actualQueueIdx < strLstSize(actualQueue); actualQueueIdx++)
        {
            // Get file from actual queue
            const String *file = strLstGet(actualQueue, actualQueueIdx);

            // Does this match a file we want to preserve?
            if (regExpMatch(regExpPreserve, file))
                strLstAdd(keepQueue, file);

            // Else delete it
            else
                storageRemoveP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(file)), .errorOnMissing = true);
        }

        // Generate a list of the WAL that are needed by removing kept WAL from the ideal queue
        strLstSort(keepQueue, sortOrderAsc);

        for (unsigned int idealQueueIdx = 0; idealQueueIdx < strLstSize(idealQueue); idealQueueIdx++)
        {
            if (!strLstExists(keepQueue, strLstGet(idealQueue, idealQueueIdx)))
                strLstAdd(result, strLstGet(idealQueue, idealQueueIdx));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
int
cmdArchiveGet(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // PostgreSQL must be local
    pgIsLocalVerify();

    // Set the result assuming the archive file will not be found
    int result = 1;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check the parameters
        const StringList *commandParam = cfgCommandParam();

        if (strLstSize(commandParam) != 2)
        {
            if (strLstSize(commandParam) == 0)
                THROW(ParamRequiredError, "WAL segment to get required");

            if (strLstSize(commandParam) == 1)
                THROW(ParamRequiredError, "path to copy WAL segment required");

            THROW(ParamInvalidError, "extra parameters found");
        }

        // Get the segment name
        String *walSegment = strBase(strLstGet(commandParam, 0));

        // Destination is wherever we were told to move the WAL segment
        const String *walDestination =
            walPath(strLstGet(commandParam, 1), cfgOptionStr(cfgOptPgPath), STR(cfgCommandName(cfgCommand())));

        // Async get can only be performed on WAL segments, history or other files must use synchronous mode
        if (cfgOptionBool(cfgOptArchiveAsync) && walIsSegment(walSegment))
        {
            bool found = false;                                         // Has the WAL segment been found yet?
            bool queueFull = false;                                     // Is the queue half or more full?
            bool forked = false;                                        // Has the async process been forked yet?
            bool throwOnError = false;                                  // Should we throw errors?

            // Loop and wait for the WAL segment to be pushed
            Wait *wait = waitNew(cfgOptionUInt64(cfgOptArchiveTimeout));

            do
            {
                // Check for errors or missing files.  For archive-get ok indicates that the process succeeded but there is no WAL
                // file to download.
                if (archiveAsyncStatus(archiveModeGet, walSegment, throwOnError))
                {
                    storageRemoveP(
                        storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s" STATUS_EXT_OK, strZ(walSegment)),
                        .errorOnMissing = true);
                    break;
                }

                // Check if the WAL segment is already in the queue
                found = storageExistsP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(walSegment)));

                // If found then move the WAL segment to the destination directory
                if (found)
                {
                    // Source is the WAL segment in the spool queue
                    StorageRead *source = storageNewReadP(
                        storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(walSegment)));

                    // A move will be attempted but if the spool queue and the WAL path are on different file systems then a copy
                    // will be performed instead.
                    //
                    // It looks scary that we are disabling syncs and atomicity (in case we need to copy instead of move) but this
                    // is safe because if the system crashes Postgres will not try to reuse a restored WAL segment but will instead
                    // request it again using the restore_command. In the case of a move this hardly matters since path syncs are
                    // cheap but if a copy is required we could save a lot of writes.
                    StorageWrite *destination = storageNewWriteP(
                        storageLocalWrite(), walDestination, .noCreatePath = true, .noSyncFile = true, .noSyncPath = true,
                        .noAtomic = true);

                    // Move (or copy if required) the file
                    storageMoveP(storageSpoolWrite(), source, destination);

                    // Return success
                    LOG_INFO_FMT("found %s in the archive asynchronously", strZ(walSegment));
                    result = 0;

                    // Get a list of WAL segments left in the queue
                    StringList *queue = storageListP(
                        storageSpool(), STORAGE_SPOOL_ARCHIVE_IN_STR, .expression = WAL_SEGMENT_REGEXP_STR, .errorOnMissing = true);

                    if (strLstSize(queue) > 0)
                    {
                        // Get size of the WAL segment
                        uint64_t walSegmentSize = storageInfoP(storageLocal(), walDestination).size;

                        // Use WAL segment size to estimate queue size and determine if the async process should be launched
                        queueFull = strLstSize(queue) * walSegmentSize > cfgOptionUInt64(cfgOptArchiveGetQueueMax) / 2;
                    }
                }

                // If the WAL segment has not already been found then start the async process to get it.  There's no point in
                // forking the async process off more than once so track that as well.  Use an archive lock to prevent forking if
                // the async process was launched by another process.
                if (!forked && (!found || !queueFull)  &&
                    lockAcquire(
                        cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgOptionStr(cfgOptExecId), cfgLockType(), 0,
                        false))
                {
                    // Get control info
                    PgControl pgControl = pgControlFromFile(storagePg());

                    // Create the queue
                    storagePathCreateP(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN_STR);

                    // The async process should not output on the console at all
                    KeyValue *optionReplace = kvNew();

                    kvPut(optionReplace, VARSTR(CFGOPT_LOG_LEVEL_CONSOLE_STR), VARSTRDEF("off"));
                    kvPut(optionReplace, VARSTR(CFGOPT_LOG_LEVEL_STDERR_STR), VARSTRDEF("off"));

                    // Generate command options
                    StringList *commandExec = cfgExecParam(cfgCmdArchiveGet, cfgCmdRoleAsync, optionReplace, true, false);
                    strLstInsert(commandExec, 0, cfgExe());

                    // Clean the current queue using the list of WAL that we ideally want in the queue.  queueNeed()
                    // will return the list of WAL needed to fill the queue and this will be passed to the async process.
                    const StringList *queue = queueNeed(
                        walSegment, found, cfgOptionUInt64(cfgOptArchiveGetQueueMax), pgControl.walSegmentSize,
                        pgControl.version);

                    for (unsigned int queueIdx = 0; queueIdx < strLstSize(queue); queueIdx++)
                        strLstAdd(commandExec, strLstGet(queue, queueIdx));

                    // Clear errors for the current wal segment
                    archiveAsyncErrorClear(archiveModeGet, walSegment);

                    // Release the lock so the child process can acquire it
                    lockRelease(true);

                    // Execute the async process
                    archiveAsyncExec(archiveModeGet, commandExec);

                    // Mark the async process as forked so it doesn't get forked again.  A single run of the async process should be
                    // enough to do the job, running it again won't help anything.
                    forked = true;
                }

                // Exit loop if WAL was found
                if (found)
                    break;

                // Now that the async process has been launched, throw any errors that are found
                throwOnError = true;
            }
            while (waitMore(wait));

            // Log that the file was not found
            if (result == 1)
                LOG_INFO_FMT("unable to find %s in the archive asynchronously", strZ(walSegment));

        }
        // Else perform synchronous get
        else
        {
            // Check for archive files
            StringList *archiveRequestList = strLstNew();
            strLstAdd(archiveRequestList, walSegment);

            ArchiveGetCheckResult checkResult = archiveGetCheck(archiveRequestList);

            // Get the archive file
            if (lstSize(checkResult.archiveFileMapList) > 0)
            {
                ASSERT(lstSize(checkResult.archiveFileMapList) == 1);

                archiveGetFile(
                    storageLocalWrite(), ((ArchiveFileMap *)lstGet(checkResult.archiveFileMapList, 0))->actual, walDestination,
                    false, checkResult.cipherType, checkResult.cipherPassArchive);

                // If there was no error then the file existed
                LOG_INFO_FMT(
                    "found %s in the repo%u:%s archive", strZ(walSegment),
                    cfgOptionGroupIdxToKey(cfgOptGrpRepo, cfgOptionGroupIdxDefault(cfgOptGrpRepo)), strZ(checkResult.archiveId));

                result = 0;
            }
            // Else log that the file was not found
            else
                LOG_INFO_FMT("unable to find %s in the archive", strZ(walSegment));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}

/**********************************************************************************************************************************/
static ProtocolParallelJob *archiveGetAsyncCallback(void *data, unsigned int clientIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(UINT, clientIdx);
    FUNCTION_TEST_END();

    // No special logic based on the client, we'll just get the next job
    (void)clientIdx;

    // Get a new job if there are any left
    ArchiveGetCheckResult *checkResult = data;

    if (lstSize(checkResult->archiveFileMapList) > 0)
    {
        const ArchiveFileMap archiveFileMap = *((ArchiveFileMap *)lstGet(checkResult->archiveFileMapList, 0));
        lstRemoveIdx(checkResult->archiveFileMapList, 0);

        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_ARCHIVE_GET_STR);
        protocolCommandParamAdd(command, VARSTR(archiveFileMap.request));
        protocolCommandParamAdd(command, VARSTR(archiveFileMap.actual));
        protocolCommandParamAdd(command, VARUINT(checkResult->cipherType));
        protocolCommandParamAdd(command, VARSTR(checkResult->cipherPassArchive));

        FUNCTION_TEST_RETURN(protocolParallelJobNew(VARSTR(archiveFileMap.request), command));
    }

    FUNCTION_TEST_RETURN(NULL);
}

void
cmdArchiveGetAsync(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        TRY_BEGIN()
        {
            // PostgreSQL must be local
            pgIsLocalVerify();

            // Check the parameters
            if (strLstSize(cfgCommandParam()) < 1)
                THROW(ParamInvalidError, "at least one wal segment is required");

            LOG_INFO_FMT(
                "get %u WAL file(s) from archive: %s%s",
                strLstSize(cfgCommandParam()), strZ(strLstGet(cfgCommandParam(), 0)),
                strLstSize(cfgCommandParam()) == 1 ?
                    "" : strZ(strNewFmt("...%s", strZ(strLstGet(cfgCommandParam(), strLstSize(cfgCommandParam()) - 1)))));

            // Check for archive files
            ArchiveGetCheckResult checkResult = archiveGetCheck(cfgCommandParam());

            // If any files are missing get the first one used to construct the "unable to find" warning
            const String *archiveFileMissing = NULL;

            if (lstSize(checkResult.archiveFileMapList) < strLstSize(cfgCommandParam()))
                archiveFileMissing = strLstGet(cfgCommandParam(), lstSize(checkResult.archiveFileMapList));

            // Get archive files that were found
            if (lstSize(checkResult.archiveFileMapList) > 0)
            {
                // Create the parallel executor
                ProtocolParallel *parallelExec = protocolParallelNew(
                    cfgOptionUInt64(cfgOptProtocolTimeout) / 2, archiveGetAsyncCallback, &checkResult);

                for (unsigned int processIdx = 1; processIdx <= cfgOptionUInt(cfgOptProcessMax); processIdx++)
                    protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypeRepo, 0, processIdx));

                // Process jobs
                do
                {
                    unsigned int completed = protocolParallelProcess(parallelExec);

                    for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
                    {
                        // Get the job and job key
                        ProtocolParallelJob *job = protocolParallelResult(parallelExec);
                        unsigned int processId = protocolParallelJobProcessId(job);
                        const String *walSegment = varStr(protocolParallelJobKey(job));

                        // The job was successful
                        if (protocolParallelJobErrorCode(job) == 0)
                        {
                            LOG_DETAIL_PID_FMT(
                                processId,
                                "found %s in the repo%u:%s archive", strZ(walSegment),
                                cfgOptionGroupIdxToKey(cfgOptGrpRepo, cfgOptionGroupIdxDefault(cfgOptGrpRepo)),
                                strZ(checkResult.archiveId));
                        }
                        // Else the job errored
                        else
                        {
                            LOG_WARN_PID_FMT(
                                processId,
                                "could not get %s from the archive (will be retried): [%d] %s", strZ(walSegment),
                                protocolParallelJobErrorCode(job), strZ(protocolParallelJobErrorMessage(job)));

                            archiveAsyncStatusErrorWrite(
                                archiveModeGet, walSegment, protocolParallelJobErrorCode(job),
                                protocolParallelJobErrorMessage(job));
                        }

                        protocolParallelJobFree(job);
                    }
                }
                while (!protocolParallelDone(parallelExec));
            }

            // Log an error from archiveGetCheck() after any existing files have been fetched. This ordering is important because we
            // need to fetch as many valid files as possible before throwing an error.
            if (checkResult.errorType != NULL)
            {
                LOG_WARN_FMT(
                    "could not get %s from the archive (will be retried): [%d] %s", strZ(checkResult.errorFile),
                    errorTypeCode(checkResult.errorType), strZ(checkResult.errorMessage));

                archiveAsyncStatusErrorWrite(
                    archiveModeGet, checkResult.errorFile, errorTypeCode(checkResult.errorType), checkResult.errorMessage);
            }
            // Else log a warning if any files were missing
            else if (archiveFileMissing != NULL)
            {
                LOG_DETAIL_FMT("unable to find %s in the archive", strZ(archiveFileMissing));
                archiveAsyncStatusOkWrite(archiveModeGet, archiveFileMissing, NULL);
            }
        }
        // On any global error write a single error file to cover all unprocessed files
        CATCH_ANY()
        {
            archiveAsyncStatusErrorWrite(archiveModeGet, NULL, errorCode(), STR(errorMessage()));
            RETHROW();
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
