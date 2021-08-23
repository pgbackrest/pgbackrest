/***********************************************************************************************************************************
Archive Push Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <unistd.h>

#include "command/archive/common.h"
#include "command/archive/push/file.h"
#include "command/archive/push/protocol.h"
#include "command/command.h"
#include "command/control/common.h"
#include "common/compress/helper.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/wait.h"
#include "config/config.h"
#include "config/exec.h"
#include "info/infoArchive.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "protocol/parallel.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants for log messages that are used multiple times to keep them consistent
***********************************************************************************************************************************/
#define UNABLE_TO_FIND_VALID_REPO_MSG                               "unable to find a valid repository"

/***********************************************************************************************************************************
Ready file extension constants
***********************************************************************************************************************************/
#define STATUS_EXT_READY                                            ".ready"
#define STATUS_EXT_READY_SIZE                                       (sizeof(STATUS_EXT_READY) - 1)

/***********************************************************************************************************************************
Format the warning when a file is dropped
***********************************************************************************************************************************/
static String *
archivePushDropWarning(const String *walFile, uint64_t queueMax)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, walFile);
        FUNCTION_TEST_PARAM(UINT64, queueMax);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(
        strNewFmt("dropped WAL file '%s' because archive queue exceeded %s", strZ(walFile), strZ(strSizeFormat(queueMax))));
}

/***********************************************************************************************************************************
Determine if the WAL process list has become large enough to drop
***********************************************************************************************************************************/
static bool
archivePushDrop(const String *walPath, const StringList *const processList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walPath);
        FUNCTION_LOG_PARAM(STRING_LIST, processList);
    FUNCTION_LOG_END();

    const uint64_t queueMax = cfgOptionUInt64(cfgOptArchivePushQueueMax);
    uint64_t queueSize = 0;
    bool result = false;

    for (unsigned int processIdx = 0; processIdx < strLstSize(processList); processIdx++)
    {
        queueSize += storageInfoP(
            storagePg(), strNewFmt("%s/%s", strZ(walPath), strZ(strLstGet(processList, processIdx)))).size;

        if (queueSize > queueMax)
        {
            result = true;
            break;
        }
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Get the list of WAL files ready to be pushed according to PostgreSQL
***********************************************************************************************************************************/
static StringList *
archivePushReadyList(const String *walPath)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, walPath);
    FUNCTION_LOG_END();

    ASSERT(walPath != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = strLstNew();

        // Read the ready files from the archive_status directory
        StringList *readyListRaw = strLstSort(
            storageListP(
                storagePg(), strNewFmt("%s/" PG_PATH_ARCHIVE_STATUS, strZ(walPath)),
                .expression = STRDEF("\\" STATUS_EXT_READY "$"), .errorOnMissing = true),
            sortOrderAsc);

        for (unsigned int readyIdx = 0; readyIdx < strLstSize(readyListRaw); readyIdx++)
        {
            strLstAdd(
                result,
                strSubN(strLstGet(readyListRaw, readyIdx), 0, strSize(strLstGet(readyListRaw, readyIdx)) - STATUS_EXT_READY_SIZE));
        }

        strLstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Determine which WAL files need to be pushed to the archive when in async mode

This is the heart of the "look ahead" functionality in async archiving.  Any files in the out directory that do not end in ok are
removed and any ok files that do not have a corresponding ready file in archive_status (meaning it has been acknowledged by
PostgreSQL) are removed.  Then all ready files that do not have a corresponding ok file (meaning it has already been processed) are
returned for processing.
***********************************************************************************************************************************/
static StringList *
archivePushProcessList(const String *walPath)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, walPath);
    FUNCTION_LOG_END();

    ASSERT(walPath != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create the spool out path if it does not already exist
        storagePathCreateP(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT_STR);

        // Read the status files from the spool directory, then remove any files that do not end in ok and create a list of the
        // ok files for further processing
        StringList *statusList = strLstSort(
            storageListP(storageSpool(), STORAGE_SPOOL_ARCHIVE_OUT_STR, .errorOnMissing = true), sortOrderAsc);

        StringList *okList = strLstNew();

        for (unsigned int statusIdx = 0; statusIdx < strLstSize(statusList); statusIdx++)
        {
            const String *statusFile = strLstGet(statusList, statusIdx);

            if (strEndsWithZ(statusFile, STATUS_EXT_OK))
                strLstAdd(okList, strSubN(statusFile, 0, strSize(statusFile) - STATUS_EXT_OK_SIZE));
            else
            {
                storageRemoveP(
                    storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s", strZ(statusFile)), .errorOnMissing = true);
            }
        }

        // Read the ready files from the archive_status directory
        StringList *readyList = archivePushReadyList(walPath);

        // Remove ok files that are not in the ready list
        StringList *okRemoveList = strLstMergeAnti(okList, readyList);

        for (unsigned int okRemoveIdx = 0; okRemoveIdx < strLstSize(okRemoveList); okRemoveIdx++)
        {
            storageRemoveP(
                storageSpoolWrite(),
                strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s" STATUS_EXT_OK, strZ(strLstGet(okRemoveList, okRemoveIdx))),
                .errorOnMissing = true);
        }

        // Return all ready files that are not in the ok list
        result = strLstMove(strLstMergeAnti(readyList, okList), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Check that pg_control and archive.info match and get the archive id and archive cipher passphrase (if present)

As much information as possible is collected here so that async archiving has as little work as possible to do for each file.  Sync
archiving does not benefit but it makes sense to use the same function.
***********************************************************************************************************************************/
typedef struct ArchivePushCheckResult
{
    unsigned int pgVersion;                                         // PostgreSQL version
    uint64_t pgSystemId;                                            // PostgreSQL system id
    List *repoList;                                                 // Data for each repo
    StringList *errorList;                                          // Errors while checking repos
} ArchivePushCheckResult;

static ArchivePushCheckResult
archivePushCheck(bool pgPathSet)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, pgPathSet);
    FUNCTION_LOG_END();

    ArchivePushCheckResult result = {.repoList = lstNewP(sizeof(ArchivePushFileRepoData)), .errorList = strLstNew()};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If we have access to pg_control then load it to get the pg version and system id. If we can't load pg_control then we'll
        // still compare the pg info stored in the repo to the WAL segment and also all the repos against each other.
        if (pgPathSet)
        {
            // Get info from pg_control
            PgControl pgControl = pgControlFromFile(storagePg());
            result.pgVersion = pgControl.version;
            result.pgSystemId = pgControl.systemId;
        }

        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            TRY_BEGIN()
            {
                // Get the repo storage in case it is remote and encryption settings need to be pulled down
                storageRepoIdx(repoIdx);

                // Get cipher type
                CipherType repoCipherType = cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx);

                // Attempt to load the archive info file
                InfoArchive *info = infoArchiveLoadFile(
                    storageRepoIdx(repoIdx), INFO_ARCHIVE_PATH_FILE_STR, repoCipherType,
                    cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));

                // Get archive id for the most recent version -- archive-push will only operate against the most recent version
                String *archiveId = infoPgArchiveId(infoArchivePg(info), infoPgDataCurrentId(infoArchivePg(info)));
                InfoPgData archiveInfo = infoPgData(infoArchivePg(info), infoPgDataCurrentId(infoArchivePg(info)));

                // Ensure that stanza version and system identifier match pg_control when available or the other repos when
                // pg_control is not available
                if (pgPathSet || repoIdx > 0)
                {
                    if (result.pgVersion != archiveInfo.version || result.pgSystemId != archiveInfo.systemId)
                    {
                        THROW_FMT(
                            ArchiveMismatchError,
                            "%s version %s, system-id %" PRIu64 " do not match %s stanza version %s, system-id %" PRIu64
                            "\nHINT: are you archiving to the correct stanza?",
                            pgPathSet ? PG_NAME : strZ(strNewFmt("repo%u stanza", cfgOptionGroupIdxToKey(cfgOptGrpRepo, 0))),
                            strZ(pgVersionToStr(result.pgVersion)), result.pgSystemId,
                            strZ(strNewFmt("repo%u", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx))),
                            strZ(pgVersionToStr(archiveInfo.version)), archiveInfo.systemId);
                    }
                }

                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result.pgVersion = archiveInfo.version;
                    result.pgSystemId = archiveInfo.systemId;

                    lstAdd(
                        result.repoList,
                        &(ArchivePushFileRepoData)
                        {
                            .repoIdx = repoIdx,
                            .archiveId = strDup(archiveId),
                            .cipherType = repoCipherType,
                            .cipherPass = strDup(infoArchiveCipherPass(info)),
                        });
                }
                MEM_CONTEXT_PRIOR_END();
            }
            CATCH_ANY()
            {
                strLstAdd(
                    result.errorList,
                    strNewFmt(
                        "repo%u: [%s] %s", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx), errorTypeName(errorType()),
                        errorMessage()));
            }
            TRY_END();
        }
    }
    MEM_CONTEXT_TEMP_END();

    // If no valid repos were found then error
    if (lstEmpty(result.repoList))
    {
        ASSERT(strLstSize(result.errorList) > 0);

        THROW_FMT(RepoInvalidError, UNABLE_TO_FIND_VALID_REPO_MSG ":\n%s", strZ(strLstJoin(result.errorList, "\n")));
    }

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
void
cmdArchivePush(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    ASSERT(cfgCommand() == cfgCmdArchivePush);

    // PostgreSQL must be local
    pgIsLocalVerify();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Make sure there is a parameter to retrieve the WAL segment from
        const StringList *commandParam = cfgCommandParam();

        if (strLstSize(commandParam) != 1)
            THROW(ParamRequiredError, "WAL segment to push required");

        // Test for stop file
        lockStopTest();

        // Get the segment name
        String *walFile = walPath(strLstGet(commandParam, 0), cfgOptionStrNull(cfgOptPgPath), STR(cfgCommandName()));
        String *archiveFile = strBase(walFile);

        if (cfgOptionBool(cfgOptArchiveAsync))
        {
            bool pushed = false;                                        // Has the WAL segment been pushed yet?
            bool forked = false;                                        // Has the async process been forked yet?
            bool throwOnError = false;                                  // Should we throw errors?

            // pg1-path is not optional for async mode
            if (!cfgOptionTest(cfgOptPgPath))
            {
                THROW_FMT(
                    OptionRequiredError, "'" CFGCMD_ARCHIVE_PUSH "' command in async mode requires option '%s'",
                    cfgOptionName(cfgOptPgPath));
            }

            // Loop and wait for the WAL segment to be pushed
            Wait *wait = waitNew(cfgOptionUInt64(cfgOptArchiveTimeout));

            do
            {
                // Check if the WAL segment has been pushed.  Errors will not be thrown on the first try to allow the async process
                // a chance to fix them.
                pushed = archiveAsyncStatus(archiveModePush, archiveFile, throwOnError, true);

                // If the WAL segment has not already been pushed then start the async process to push it.  There's no point in
                // forking the async process off more than once so track that as well.  Use an archive lock to prevent more than
                // one async process being launched.
                if (!pushed && !forked &&
                    lockAcquire(
                        cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgOptionStr(cfgOptExecId), cfgLockType(), 0,
                        false))
                {
                    // The async process should not output on the console at all
                    KeyValue *optionReplace = kvNew();

                    kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_CONSOLE), VARSTRDEF("off"));
                    kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_STDERR), VARSTRDEF("off"));

                    // Generate command options
                    StringList *commandExec = cfgExecParam(cfgCmdArchivePush, cfgCmdRoleAsync, optionReplace, true, false);
                    strLstInsert(commandExec, 0, cfgExe());
                    strLstAdd(commandExec, strPath(walFile));

                    // Clear errors for the current archive file
                    archiveAsyncErrorClear(archiveModePush, archiveFile);

                    // Release the lock so the child process can acquire it
                    lockRelease(true);

                    // Execute the async process
                    archiveAsyncExec(archiveModePush, commandExec);

                    // Mark the async process as forked so it doesn't get forked again.  A single run of the async process should be
                    // enough to do the job, running it again won't help anything.
                    forked = true;
                }

                // Now that the async process has been launched, throw any errors that are found
                throwOnError = true;
            }
            while (!pushed && waitMore(wait));

            // If the WAL segment was not pushed then error
            if (!pushed)
            {
                THROW_FMT(
                    ArchiveTimeoutError, "unable to push WAL file '%s' to the archive asynchronously after %s second(s)",
                    strZ(archiveFile), strZ(cfgOptionDisplay(cfgOptArchiveTimeout)));
            }

            // Log success
            LOG_INFO_FMT("pushed WAL file '%s' to the archive asynchronously", strZ(archiveFile));
        }
        else
        {
            // Check if the push queue has been exceeded
            if (cfgOptionTest(cfgOptArchivePushQueueMax) &&
                archivePushDrop(strPath(walFile), archivePushReadyList(strPath(walFile))))
            {
                LOG_WARN(strZ(archivePushDropWarning(archiveFile, cfgOptionUInt64(cfgOptArchivePushQueueMax))));
            }
            // Else push the file
            else
            {
                // Check archive info for each repo
                ArchivePushCheckResult archiveInfo = archivePushCheck(cfgOptionTest(cfgOptPgPath));

                // Push the file to the archive
                ArchivePushFileResult fileResult = archivePushFile(
                    walFile, cfgOptionBool(cfgOptArchiveHeaderCheck), archiveInfo.pgVersion, archiveInfo.pgSystemId, archiveFile,
                    compressTypeEnum(cfgOptionStr(cfgOptCompressType)), cfgOptionInt(cfgOptCompressLevel), archiveInfo.repoList,
                    archiveInfo.errorList);

                // If a warning was returned then log it
                for (unsigned int warnIdx = 0; warnIdx < strLstSize(fileResult.warnList); warnIdx++)
                    LOG_WARN(strZ(strLstGet(fileResult.warnList, warnIdx)));

                // Log success
                LOG_INFO_FMT("pushed WAL file '%s' to the archive", strZ(archiveFile));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
typedef struct ArchivePushAsyncData
{
    const String *walPath;                                          // Path to pg_wal/pg_xlog
    const StringList *walFileList;                                  // List of wal files to process
    unsigned int walFileIdx;                                        // Current index in the list to be processed
    CompressType compressType;                                      // Type of compression for WAL segments
    int compressLevel;                                              // Compression level for wal files
    ArchivePushCheckResult archiveInfo;                             // Archive info
} ArchivePushAsyncData;

static ProtocolParallelJob *
archivePushAsyncCallback(void *data, unsigned int clientIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(UINT, clientIdx);
    FUNCTION_TEST_END();

    ProtocolParallelJob *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // No special logic based on the client, we'll just get the next job
        (void)clientIdx;

        // Get a new job if there are any left
        ArchivePushAsyncData *jobData = data;

        if (jobData->walFileIdx < strLstSize(jobData->walFileList))
        {
            const String *walFile = strLstGet(jobData->walFileList, jobData->walFileIdx);
            jobData->walFileIdx++;

            ProtocolCommand *const command = protocolCommandNew(PROTOCOL_COMMAND_ARCHIVE_PUSH_FILE);
            PackWrite *const param = protocolCommandParam(command);

            pckWriteStrP(param, strNewFmt("%s/%s", strZ(jobData->walPath), strZ(walFile)));
            pckWriteBoolP(param, cfgOptionBool(cfgOptArchiveHeaderCheck));
            pckWriteU32P(param, jobData->archiveInfo.pgVersion);
            pckWriteU64P(param, jobData->archiveInfo.pgSystemId);
            pckWriteStrP(param, walFile);
            pckWriteU32P(param, jobData->compressType);
            pckWriteI32P(param, jobData->compressLevel);
            pckWriteStrLstP(param, jobData->archiveInfo.errorList);

            // Add data for each repo to push to
            pckWriteArrayBeginP(param);

            for (unsigned int repoListIdx = 0; repoListIdx < lstSize(jobData->archiveInfo.repoList); repoListIdx++)
            {
                ArchivePushFileRepoData *data = lstGet(jobData->archiveInfo.repoList, repoListIdx);

                pckWriteObjBeginP(param);
                pckWriteU32P(param, data->repoIdx);
                pckWriteStrP(param, data->archiveId);
                pckWriteU64P(param, data->cipherType);
                pckWriteStrP(param, data->cipherPass);
                pckWriteObjEndP(param);
            }

            pckWriteArrayEndP(param);

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result = protocolParallelJobNew(VARSTR(walFile), command);
            }
            MEM_CONTEXT_PRIOR_END();
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

void
cmdArchivePushAsync(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    ASSERT(cfgCommand() == cfgCmdArchivePush && cfgCommandRole() == cfgCmdRoleAsync);

    // PostgreSQL must be local
    pgIsLocalVerify();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Make sure there is a parameter with the wal path
        const StringList *commandParam = cfgCommandParam();

        if (strLstSize(commandParam) != 1)
            THROW(ParamRequiredError, "WAL path to push required");

        ArchivePushAsyncData jobData =
        {
            .walPath = strLstGet(commandParam, 0),
            .compressType = compressTypeEnum(cfgOptionStr(cfgOptCompressType)),
            .compressLevel = cfgOptionInt(cfgOptCompressLevel),
        };

        TRY_BEGIN()
        {
            // Test for stop file
            lockStopTest();

            // Get a list of WAL files that are ready for processing
            jobData.walFileList = archivePushProcessList(jobData.walPath);

            // The archive-push:async command should not have been called unless there are WAL files to process
            if (strLstEmpty(jobData.walFileList))
                THROW(AssertError, "no WAL files to process");

            LOG_INFO_FMT(
                "push %u WAL file(s) to archive: %s%s", strLstSize(jobData.walFileList), strZ(strLstGet(jobData.walFileList, 0)),
                strLstSize(jobData.walFileList) == 1 ?
                    "" : strZ(strNewFmt("...%s", strZ(strLstGet(jobData.walFileList, strLstSize(jobData.walFileList) - 1)))));

            // Drop files if queue max has been exceeded
            if (cfgOptionTest(cfgOptArchivePushQueueMax) && archivePushDrop(jobData.walPath, jobData.walFileList))
            {
                for (unsigned int walFileIdx = 0; walFileIdx < strLstSize(jobData.walFileList); walFileIdx++)
                {
                    const String *walFile = strLstGet(jobData.walFileList, walFileIdx);
                    const String *warning = archivePushDropWarning(walFile, cfgOptionUInt64(cfgOptArchivePushQueueMax));

                    archiveAsyncStatusOkWrite(archiveModePush, walFile, warning);
                    LOG_WARN(strZ(warning));
                }
            }
            // Else continue processing
            else
            {
                // Check archive info for each repo
                jobData.archiveInfo = archivePushCheck(true);

                // Create the parallel executor
                ProtocolParallel *parallelExec = protocolParallelNew(
                    cfgOptionUInt64(cfgOptProtocolTimeout) / 2, archivePushAsyncCallback, &jobData);

                for (unsigned int processIdx = 1; processIdx <= cfgOptionUInt(cfgOptProcessMax); processIdx++)
                    protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypeRepo, 0, processIdx));

                // Process jobs
                MEM_CONTEXT_TEMP_RESET_BEGIN()
                {
                    do
                    {
                        unsigned int completed = protocolParallelProcess(parallelExec);

                        for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
                        {
                            protocolKeepAlive();

                            // Get the job and job key
                            ProtocolParallelJob *job = protocolParallelResult(parallelExec);
                            unsigned int processId = protocolParallelJobProcessId(job);
                            const String *walFile = varStr(protocolParallelJobKey(job));

                            // The job was successful
                            if (protocolParallelJobErrorCode(job) == 0)
                            {
                                // Output file warnings
                                StringList *fileWarnList = pckReadStrLstP(protocolParallelJobResult(job));

                                for (unsigned int warnIdx = 0; warnIdx < strLstSize(fileWarnList); warnIdx++)
                                    LOG_WARN_PID(processId, strZ(strLstGet(fileWarnList, warnIdx)));

                                // Log success
                                LOG_DETAIL_PID_FMT(processId, "pushed WAL file '%s' to the archive", strZ(walFile));

                                // Write the status file
                                archiveAsyncStatusOkWrite(
                                    archiveModePush, walFile, strLstEmpty(fileWarnList) ? NULL : strLstJoin(fileWarnList, "\n"));
                            }
                            // Else the job errored
                            else
                            {
                                LOG_WARN_PID_FMT(
                                    processId,
                                    "could not push WAL file '%s' to the archive (will be retried): [%d] %s", strZ(walFile),
                                    protocolParallelJobErrorCode(job), strZ(protocolParallelJobErrorMessage(job)));

                                archiveAsyncStatusErrorWrite(
                                    archiveModePush, walFile, protocolParallelJobErrorCode(job),
                                    protocolParallelJobErrorMessage(job));
                            }

                            protocolParallelJobFree(job);
                        }

                        // Reset the memory context occasionally so we don't use too much memory or slow down processing
                        MEM_CONTEXT_TEMP_RESET(1000);
                    }
                    while (!protocolParallelDone(parallelExec));
                }
                MEM_CONTEXT_TEMP_END();
            }
        }
        // On any global error write a single error file to cover all unprocessed files
        CATCH_ANY()
        {
            archiveAsyncStatusErrorWrite(archiveModePush, NULL, errorCode(), STR(errorMessage()));
            RETHROW();
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
