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
#include "storage/write.intern.h"

/***********************************************************************************************************************************
Constants for log messages that are used multiple times to keep them consistent
***********************************************************************************************************************************/
#define FOUND_IN_ARCHIVE_MSG                                        "found %s in the archive"
#define FOUND_IN_REPO_ARCHIVE_MSG                                   "found %s in the repo%u: %s archive"
#define UNABLE_TO_FIND_IN_ARCHIVE_MSG                               "unable to find %s in the archive"
#define UNABLE_TO_FIND_VALID_REPO_MSG                               "unable to find a valid repository"
#define REPO_INVALID_OR_ERR_MSG                                     "some repositories were invalid or encountered errors"

/***********************************************************************************************************************************
Check for a list of archive files in the repository
***********************************************************************************************************************************/
typedef struct ArchiveFileMap
{
    const String *request;                                          // Archive file requested by archive_command
    List *actualList;                                               // Actual files in various repos/archiveIds
    StringList *warnList;                                           // Warnings that need to be reported by the async process
} ArchiveFileMap;

typedef struct ArchiveGetCheckResult
{
    List *archiveFileMapList;                                       // List of mapped archive files, i.e. found in the repo

    // Global error that affects all repos
    const ErrorType *errorType;                                     // Error type if there was an error
    const String *errorFile;                                        // Error file if there was an error
    const String *errorMessage;                                     // Error message if there was an error
    const StringList *warnList;                                     // Warnings that need to be reported by the async process
} ArchiveGetCheckResult;

// Helper to add an error to an error list and warn if the error is not already in the list
static void
archiveGetErrorAdd(StringList *warnList, bool log, unsigned int repoIdx, const ErrorType *type, const String *message)
{
    const String *warn = strNewFmt(
        "repo%u: [%s] %s", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx), errorTypeName(type), strZ(message));

    if (!strLstExists(warnList, warn))
    {
        if (log)
            LOG_WARN(strZ(warn));

        strLstAdd(warnList, warn);
    }
}

// Helper to find a single archive file in the repository using a cache to speed up the process and minimize storageListP() calls
typedef struct ArchiveGetFindCachePath
{
    const String *path;                                             // Cached path in the archiveId
    const StringList *fileList;                                     // List of files in the cache path
} ArchiveGetFindCachePath;

typedef struct ArchiveGetFindCacheArchive
{
    const String *archiveId;                                        // ArchiveId in the repo
    List *pathList;                                                 // List of paths cached for archiveId
} ArchiveGetFindCacheArchive;

typedef struct ArchiveGetFindCacheRepo
{
    unsigned int repoIdx;
    CipherType cipherType;                                          // Repo cipher type
    const String *cipherPassArchive;                                // Repo archive cipher pass
    List *archiveList;
    StringList *warnList;                                           // Track repo warnings so each is only reported once
} ArchiveGetFindCacheRepo;

static bool
archiveGetFind(
    const String *archiveFileRequest, ArchiveGetCheckResult *getCheckResult, List *cacheRepoList, const StringList *warnList,
    bool single)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, archiveFileRequest);
        FUNCTION_LOG_PARAM_P(VOID, getCheckResult);
        FUNCTION_LOG_PARAM(LIST, cacheRepoList);
        FUNCTION_LOG_PARAM(STRING_LIST, warnList);
        FUNCTION_LOG_PARAM(BOOL, single);
    FUNCTION_LOG_END();

    ASSERT(archiveFileRequest != NULL);
    ASSERT(getCheckResult != NULL);
    ASSERT(cacheRepoList != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Is the archive file a WAL segment?
        bool isSegment = walIsSegment(archiveFileRequest);

        // Get the WAL segment path
        const String *path = isSegment ? strSubN(archiveFileRequest, 0, 16) : NULL;

        // List to hold matches for the requested file
        List *matchList = lstNewP(sizeof(ArchiveGetFile));

        // List of file level warnings
        StringList *fileWarnList = strLstDup(warnList);

        // Errored repo total to track if all repos errored
        unsigned int repoErrorTotal = 0;

        // Check each repo
        for (unsigned int repoCacheIdx = 0; repoCacheIdx < lstSize(cacheRepoList); repoCacheIdx++)
        {
            ArchiveGetFindCacheRepo *cacheRepo = lstGet(cacheRepoList, repoCacheIdx);

            TRY_BEGIN()
            {
                // Check each archiveId
                for (unsigned int archiveCacheIdx = 0; archiveCacheIdx < lstSize(cacheRepo->archiveList); archiveCacheIdx++)
                {
                    ArchiveGetFindCacheArchive *cacheArchive = lstGet(cacheRepo->archiveList, archiveCacheIdx);

                    // If a WAL segment then search among the possible file names
                    if (isSegment)
                    {
                        StringList *segmentList = NULL;

                        // If a single file is requested then optimize by adding a restrictive expression to reduce bandwidth
                        if (single)
                        {
                            segmentList = storageListP(
                                storageRepoIdx(cacheRepo->repoIdx),
                                strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(cacheArchive->archiveId), strZ(path)),
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
                            const ArchiveGetFindCachePath *cachePath = lstFind(cacheArchive->pathList, &path);

                            if (cachePath == NULL)
                            {
                                MEM_CONTEXT_BEGIN(lstMemContext(cacheArchive->pathList))
                                {
                                    cachePath = lstAdd(
                                        cacheArchive->pathList,
                                        &(ArchiveGetFindCachePath)
                                        {
                                            .path = strDup(path),
                                            .fileList = storageListP(
                                                storageRepoIdx(cacheRepo->repoIdx),
                                                strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(cacheArchive->archiveId), strZ(path)),
                                                .expression = strNewFmt(
                                                    "^%s[0-F]{8}-[0-f]{40}" COMPRESS_TYPE_REGEXP "{0,1}$", strZ(path))),
                                        });
                                }
                                MEM_CONTEXT_END();
                            }

                            // Get a list of all WAL segments that match
                            segmentList = strLstNew();

                            for (unsigned int fileIdx = 0; fileIdx < strLstSize(cachePath->fileList); fileIdx++)
                            {
                                if (strBeginsWith(strLstGet(cachePath->fileList, fileIdx), archiveFileRequest))
                                    strLstAdd(segmentList, strLstGet(cachePath->fileList, fileIdx));
                            }
                        }

                        // Add segments to match list
                        for (unsigned int segmentIdx = 0; segmentIdx < strLstSize(segmentList); segmentIdx++)
                        {
                            MEM_CONTEXT_BEGIN(lstMemContext(getCheckResult->archiveFileMapList))
                            {
                                lstAdd(
                                    matchList,
                                    &(ArchiveGetFile)
                                    {
                                        .file = strNewFmt(
                                            "%s/%s/%s", strZ(cacheArchive->archiveId), strZ(path),
                                            strZ(strLstGet(segmentList, segmentIdx))),
                                        .repoIdx = cacheRepo->repoIdx,
                                        .archiveId = cacheArchive->archiveId,
                                        .cipherType = cacheRepo->cipherType,
                                        .cipherPassArchive = cacheRepo->cipherPassArchive,
                                    });
                            }
                            MEM_CONTEXT_END();
                        }
                    }
                    // Else if not a WAL segment, see if it exists in the archiveId path
                    else if (storageExistsP(
                        storageRepoIdx(cacheRepo->repoIdx), strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(cacheArchive->archiveId),
                        strZ(archiveFileRequest))))
                    {
                        MEM_CONTEXT_BEGIN(lstMemContext(getCheckResult->archiveFileMapList))
                        {
                            lstAdd(
                                matchList,
                                &(ArchiveGetFile)
                                {
                                    .file = strNewFmt("%s/%s", strZ(cacheArchive->archiveId), strZ(archiveFileRequest)),
                                    .repoIdx = cacheRepo->repoIdx,
                                    .archiveId = cacheArchive->archiveId,
                                    .cipherType = cacheRepo->cipherType,
                                    .cipherPassArchive = cacheRepo->cipherPassArchive,
                                });
                        }
                        MEM_CONTEXT_END();
                    }
                }
            }
            // Log errors as warnings and continue
            CATCH_ANY()
            {
                repoErrorTotal++;
                archiveGetErrorAdd(cacheRepo->warnList, true, cacheRepo->repoIdx, errorType(), STR(errorMessage()));
                archiveGetErrorAdd(fileWarnList, false, cacheRepo->repoIdx, errorType(), STR(errorMessage()));
            }
            TRY_END();
        }

        // If all repos errored out then set the global error since processing cannot continue past this segment
        ASSERT(repoErrorTotal <= lstSize(cacheRepoList));

        if (repoErrorTotal == lstSize(cacheRepoList))
        {
            ASSERT(!strLstEmpty(fileWarnList));

            MEM_CONTEXT_BEGIN(lstMemContext(getCheckResult->archiveFileMapList))
            {
                getCheckResult->errorType = &RepoInvalidError;
                getCheckResult->errorFile = strDup(archiveFileRequest);
                getCheckResult->errorMessage = strNewFmt(UNABLE_TO_FIND_VALID_REPO_MSG);
                getCheckResult->warnList = strLstMove(fileWarnList, memContextCurrent());
            }
            MEM_CONTEXT_END();

        }
        // Else if a file was found
        else if (!lstEmpty(matchList))
        {
            bool error = false;

            // If a segment match list is > 1 then check for duplicates
            if (isSegment && lstSize(matchList) > 1)
            {
                // Count the number of unique hashes
                StringList *hashList = strLstNew();

                for (unsigned int matchIdx = 0; matchIdx < lstSize(matchList); matchIdx++)
                    strLstAddIfMissing(hashList, strSubN(((ArchiveGetFile *)lstGet(matchList, matchIdx))->file, 25, 40));

                // If there is more than one unique hash then there are duplicates
                if (strLstSize(hashList) > 1)
                {
                    // Build list of duplicates
                    unsigned int repoKeyLast = 0;
                    String *message = strNew("");
                    bool first = true;

                    for (unsigned int matchIdx = 0; matchIdx < lstSize(matchList); matchIdx++)
                    {
                        ArchiveGetFile *file = lstGet(matchList, matchIdx);
                        unsigned int repoKey = cfgOptionGroupIdxToKey(cfgOptGrpRepo, file->repoIdx);

                        if (repoKey != repoKeyLast)
                        {
                            strCatFmt(message, "\nrepo%u:", repoKey);
                            repoKeyLast = repoKey;
                            first = true;
                        }

                        if (first)
                            first = false;
                        else
                            strCatChr(message, ',');

                        strCatFmt(message, " %s", strZ(file->file));
                    }

                    // Set as global error since processing cannot continue past this segment
                    MEM_CONTEXT_BEGIN(lstMemContext(getCheckResult->archiveFileMapList))
                    {
                        getCheckResult->errorType = &ArchiveDuplicateError;
                        getCheckResult->errorFile = strDup(archiveFileRequest);
                        getCheckResult->errorMessage = strNewFmt(
                            "duplicates found for WAL segment %s:%s\n"
                                "HINT: are multiple primaries archiving to this stanza?",
                            strZ(archiveFileRequest), strZ(message));
                        getCheckResult->warnList = strLstMove(fileWarnList, memContextCurrent());
                    }
                    MEM_CONTEXT_END();

                    error = true;
                }
            }

            // Files are valid so add them to the map
            if (!error)
            {
                MEM_CONTEXT_BEGIN(lstMemContext(getCheckResult->archiveFileMapList))
                {
                    ArchiveFileMap map =
                    {
                        .request = strDup(archiveFileRequest),
                        .actualList = lstNewP(sizeof(ArchiveGetFile)),
                        .warnList = strLstMove(fileWarnList, memContextCurrent()),
                    };

                    for (unsigned int matchIdx = 0; matchIdx < lstSize(matchList); matchIdx++)
                        lstAdd(map.actualList, lstGet(matchList, matchIdx));

                    lstAdd(getCheckResult->archiveFileMapList, &map);
                }
                MEM_CONTEXT_END();

                result = true;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

static ArchiveGetCheckResult
archiveGetCheck(const StringList *archiveRequestList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_LIST, archiveRequestList);
    FUNCTION_LOG_END();

    ASSERT(archiveRequestList != NULL);
    ASSERT(!strLstEmpty(archiveRequestList));

    ArchiveGetCheckResult result = {.archiveFileMapList = lstNewP(sizeof(ArchiveFileMap), .comparator = lstComparatorStr)};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // List of warnings
        StringList *warnList = strLstNew();

        // Get pg control info
        PgControl controlInfo = pgControlFromFile(storagePg());

        // Build list of repos/archiveIds where WAL may be found
        List *cacheRepoList = lstNewP(sizeof(ArchiveGetFindCacheRepo));

        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            // If a repo was specified then skip all other repos
            if (cfgOptionTest(cfgOptRepo) && cfgOptionUInt(cfgOptRepo) != cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx))
                continue;

            TRY_BEGIN()
            {
                // Get the repo storage in case it is remote and encryption settings need to be pulled down
                storageRepoIdx(repoIdx);

                ArchiveGetFindCacheRepo cacheRepo =
                {
                    .repoIdx = repoIdx,
                    .cipherType = cipherType(cfgOptionIdxStr(cfgOptRepoCipherType, repoIdx)),
                    .archiveList = lstNewP(sizeof(ArchiveGetFindCacheArchive)),
                    .warnList = strLstNew(),
                };

                // Attempt to load the archive info file
                InfoArchive *info = infoArchiveLoadFile(
                    storageRepoIdx(repoIdx), INFO_ARCHIVE_PATH_FILE_STR, cacheRepo.cipherType,
                    cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));

                // Copy cipher pass into the result list context once rather than making a copy per candidate file later
                MEM_CONTEXT_BEGIN(lstMemContext(result.archiveFileMapList))
                {
                    cacheRepo.cipherPassArchive = strDup(infoArchiveCipherPass(info));
                }
                MEM_CONTEXT_END();

                // Loop through pg history and determine which archiveIds to use
                StringList *archivePathList = NULL;

                for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoArchivePg(info)); pgIdx++)
                {
                    InfoPgData pgData = infoPgData(infoArchivePg(info), pgIdx);

                    // Only use the archive id if it matches the current cluster
                    if (pgData.systemId == controlInfo.systemId && pgData.version == controlInfo.version)
                    {
                        const String *archiveId = infoPgArchiveId(infoArchivePg(info), pgIdx);
                        bool found = true;

                        // If the archiveId is in the past make sure the path exists
                        if (pgIdx != 0)
                        {
                            // Get list of archiveId paths in the archive path
                            if (archivePathList == NULL)
                                archivePathList = storageListP(storageRepoIdx(repoIdx), STORAGE_REPO_ARCHIVE_STR);

                            if (!strLstExists(archivePathList, archiveId))
                                found = false;
                        }

                        // If the archiveId is most recent or has files then add it
                        if (found)
                        {
                            ArchiveGetFindCacheArchive cacheArchive =
                            {
                                .pathList = lstNewP(sizeof(ArchiveGetFindCachePath), .comparator = lstComparatorStr),
                            };

                            // Copy archiveId into the result list context once rather than making a copy per candidate file later
                            MEM_CONTEXT_BEGIN(lstMemContext(result.archiveFileMapList))
                            {
                                cacheArchive.archiveId = strDup(archiveId);
                            }
                            MEM_CONTEXT_END();

                            lstAdd(cacheRepo.archiveList, &cacheArchive);
                        }
                    }
                }

                // Error if no archive id was found -- this indicates a mismatch with the current cluster
                if (lstEmpty(cacheRepo.archiveList))
                {
                    archiveGetErrorAdd(
                        warnList, true, repoIdx, &ArchiveMismatchError,
                        strNewFmt(
                            "unable to retrieve the archive id for database version '%s' and system-id '%" PRIu64 "'",
                            strZ(pgVersionToStr(controlInfo.version)), controlInfo.systemId));
                }
                // Else add repo to list
                else
                    lstAdd(cacheRepoList, &cacheRepo);
            }
            // Log errors as warnings and continue
            CATCH_ANY()
            {
                archiveGetErrorAdd(warnList, true, repoIdx, errorType(), STR(errorMessage()));
            }
            TRY_END();
        }

        // Error if there are no repos to check
        if (lstEmpty(cacheRepoList))
        {
            ASSERT(!strLstEmpty(warnList));

            // Set as global error since processing cannot continue past this segment
            MEM_CONTEXT_BEGIN(lstMemContext(result.archiveFileMapList))
            {
                result.errorType = &RepoInvalidError;
                result.errorMessage = strNew(UNABLE_TO_FIND_VALID_REPO_MSG);
                result.warnList = strLstMove(warnList, memContextCurrent());
            }
            MEM_CONTEXT_END();
        }
        else
        {
            // Any remaining errors will be reported as warnings since at least one repo is valid
            MEM_CONTEXT_BEGIN(lstMemContext(result.archiveFileMapList))
            {
                result.warnList = strLstMove(warnList, memContextCurrent());
            }
            MEM_CONTEXT_END();

            // Find files in the list
            for (unsigned int archiveRequestIdx = 0; archiveRequestIdx < strLstSize(archiveRequestList); archiveRequestIdx++)
            {
                if (!archiveGetFind(
                        strLstGet(archiveRequestList, archiveRequestIdx), &result, cacheRepoList, warnList,
                        strLstSize(archiveRequestList) == 1))
                {
                    break;
                }
            }

            // Sort the list to make searching for files faster
            lstSort(result.archiveFileMapList, sortOrderAsc);
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
        StringList *idealQueue = strLstSort(
            walSegmentRange(walSegmentFirst, walSegmentSize, pgVersion, walSegmentQueueTotal), sortOrderAsc);

        // Get the list of files actually in the queue
        StringList *actualQueue = strLstSort(
            storageListP(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN_STR, .errorOnMissing = true), sortOrderAsc);

        // Build a list of WAL segments that are being kept so we can later make a list of what is needed
        StringList *keepQueue = strLstNew();

        for (unsigned int actualQueueIdx = 0; actualQueueIdx < strLstSize(actualQueue); actualQueueIdx++)
        {
            // Get file from actual queue
            const String *file = strLstGet(actualQueue, actualQueueIdx);

            // Does this file match a WAL segment we want to preserve?
            if (strLstExists(idealQueue, file))
            {
                strLstAdd(keepQueue, file);
            }
            // Else delete if it does not match an ok file for a WAL segment that has already been preserved. If an ok file exists
            // in addition to the segment then it contains warnings which need to be preserved.
            else if (
                !strEndsWithZ(file, STATUS_EXT_OK) ||
                !strLstExists(actualQueue, strSubN(file, 0, strSize(file) - STATUS_EXT_OK_SIZE)))
            {
                storageRemoveP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(file)), .errorOnMissing = true);
            }
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
            if (strLstEmpty(commandParam))
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
                // Check if the WAL segment is already in the queue
                found = storageExistsP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(walSegment)));

                // Check for errors or missing files. For archive-get ok indicates that the process succeeded but there is no WAL
                // file to download, or that there was a warning.
                if (archiveAsyncStatus(archiveModeGet, walSegment, throwOnError))
                {
                    storageRemoveP(
                        storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s" STATUS_EXT_OK, strZ(walSegment)),
                        .errorOnMissing = true);

                    if (!found)
                        break;
                }

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
                    LOG_INFO_FMT(FOUND_IN_ARCHIVE_MSG " asynchronously", strZ(walSegment));
                    result = 0;

                    // Get a list of WAL segments left in the queue
                    StringList *queue = storageListP(
                        storageSpool(), STORAGE_SPOOL_ARCHIVE_IN_STR, .expression = WAL_SEGMENT_REGEXP_STR, .errorOnMissing = true);

                    if (!strLstEmpty(queue))
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
                LOG_INFO_FMT(UNABLE_TO_FIND_IN_ARCHIVE_MSG " asynchronously", strZ(walSegment));
        }
        // Else perform synchronous get
        else
        {
            // Check for the archive file
            StringList *archiveRequestList = strLstNew();
            strLstAdd(archiveRequestList, walSegment);

            ArchiveGetCheckResult checkResult = archiveGetCheck(archiveRequestList);

            // If there was an error then throw it
            if (checkResult.errorType != NULL)
                THROW_CODE(errorTypeCode(checkResult.errorType), strZ(checkResult.errorMessage));

            // Get the archive file
            if (!lstEmpty(checkResult.archiveFileMapList))
            {
                // There can only be one file mapping since only one file was requested
                ASSERT(lstSize(checkResult.archiveFileMapList) == 1);
                const ArchiveFileMap *fileMap = lstGet(checkResult.archiveFileMapList, 0);

                // Get the file
                ArchiveGetFileResult fileResult = archiveGetFile(
                    storageLocalWrite(), fileMap->request, fileMap->actualList, walDestination);

                // Output file warnings
                for (unsigned int warnIdx = 0; warnIdx < strLstSize(fileResult.warnList); warnIdx++)
                    LOG_WARN(strZ(strLstGet(fileResult.warnList, warnIdx)));

                // If there was no error then the file existed
                ArchiveGetFile *file = lstGet(fileMap->actualList, fileResult.actualIdx);
                ASSERT(file != NULL);

                LOG_INFO_FMT(
                    FOUND_IN_REPO_ARCHIVE_MSG, strZ(walSegment), cfgOptionGroupIdxToKey(cfgOptGrpRepo, file->repoIdx),
                    strZ(file->archiveId));

                result = 0;
            }
            // Else log that the file was not found
            else
                LOG_INFO_FMT(UNABLE_TO_FIND_IN_ARCHIVE_MSG, strZ(walSegment));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}

/**********************************************************************************************************************************/
typedef struct ArchiveGetAsyncData
{
    const List *const archiveFileMapList;                           // List of wal segments to process
    unsigned int archiveFileIdx;                                    // Current index in the list to be processed
} ArchiveGetAsyncData;

static ProtocolParallelJob *archiveGetAsyncCallback(void *data, unsigned int clientIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(UINT, clientIdx);
    FUNCTION_TEST_END();

    // No special logic based on the client, we'll just get the next job
    (void)clientIdx;

    // Get a new job if there are any left
    ArchiveGetAsyncData *jobData = data;

    if (jobData->archiveFileIdx < lstSize(jobData->archiveFileMapList))
    {
        const ArchiveFileMap *archiveFileMap = lstGet(jobData->archiveFileMapList, jobData->archiveFileIdx);
        jobData->archiveFileIdx++;

        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_ARCHIVE_GET_STR);
        protocolCommandParamAdd(command, VARSTR(archiveFileMap->request));

        // Add actual files to get
        for (unsigned int actualIdx = 0; actualIdx < lstSize(archiveFileMap->actualList); actualIdx++)
        {
            const ArchiveGetFile *actual = lstGet(archiveFileMap->actualList, actualIdx);

            protocolCommandParamAdd(command, VARSTR(actual->file));
            protocolCommandParamAdd(command, VARUINT(actual->repoIdx));
            protocolCommandParamAdd(command, VARSTR(actual->archiveId));
            protocolCommandParamAdd(command, VARUINT(actual->cipherType));
            protocolCommandParamAdd(command, VARSTR(actual->cipherPassArchive));
        }

        FUNCTION_TEST_RETURN(protocolParallelJobNew(VARSTR(archiveFileMap->request), command));
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

            // If any files are missing get the first one (used to construct the "unable to find" warning)
            const String *archiveFileMissing = NULL;

            if (lstSize(checkResult.archiveFileMapList) < strLstSize(cfgCommandParam()))
                archiveFileMissing = strLstGet(cfgCommandParam(), lstSize(checkResult.archiveFileMapList));

            // Get archive files that were found
            if (!lstEmpty(checkResult.archiveFileMapList))
            {
                // Create the parallel executor
                ArchiveGetAsyncData jobData = {.archiveFileMapList = checkResult.archiveFileMapList};

                ProtocolParallel *parallelExec = protocolParallelNew(
                    cfgOptionUInt64(cfgOptProtocolTimeout) / 2, archiveGetAsyncCallback, &jobData);

                for (unsigned int processIdx = 1; processIdx <= cfgOptionUInt(cfgOptProcessMax); processIdx++)
                    protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypeRepo, 0, processIdx));

                // Process jobs
                do
                {
                    unsigned int completed = protocolParallelProcess(parallelExec);

                    for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
                    {
                        // Get the job
                        ProtocolParallelJob *job = protocolParallelResult(parallelExec);
                        unsigned int processId = protocolParallelJobProcessId(job);

                        // Get wal segment name and archive file map
                        const String *walSegment = varStr(protocolParallelJobKey(job));
                        const ArchiveFileMap *fileMap = lstFind(checkResult.archiveFileMapList, &walSegment);
                        ASSERT(fileMap != NULL);

                        // Build warnings for status file
                        String *warning = strNew("");

                        if (!strLstEmpty(fileMap->warnList))
                            strCatFmt(warning, "%s", strZ(strLstJoin(fileMap->warnList, "\n")));

                        // The job was successful
                        if (protocolParallelJobErrorCode(job) == 0)
                        {
                            // Get the actual file retrieved
                            const VariantList *fileResult = varVarLst(protocolParallelJobResult(job));
                            ArchiveGetFile *file = lstGet(fileMap->actualList, varUIntForce(varLstGet(fileResult, 0)));
                            ASSERT(file != NULL);

                            // Output file warnings
                            StringList *fileWarnList = strLstNewVarLst(varVarLst(varLstGet(fileResult, 1)));

                            for (unsigned int warnIdx = 0; warnIdx < strLstSize(fileWarnList); warnIdx++)
                                LOG_WARN_PID(processId, strZ(strLstGet(fileWarnList, warnIdx)));

                            // Build file warnings for status file
                            if (!strLstEmpty(fileWarnList))
                                strCatFmt(warning, "%s%s", strSize(warning) == 0 ? "" : "\n", strZ(strLstJoin(fileWarnList, "\n")));

                            if (strSize(warning) != 0)
                                archiveAsyncStatusOkWrite(archiveModeGet, walSegment, warning);

                            LOG_DETAIL_PID_FMT(
                                processId, FOUND_IN_REPO_ARCHIVE_MSG, strZ(walSegment),
                                cfgOptionGroupIdxToKey(cfgOptGrpRepo, file->repoIdx), strZ(file->archiveId));

                            // Rename temp WAL segment to actual name. This is done after the ok file is written so if the ok file
                            // has warnings it is guaranteed to exist before the foreground process the WAL segment.
                            storageMoveP(
                                storageSpoolWrite(),
                                storageNewReadP(
                                    storageSpool(),
                                    strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s." STORAGE_FILE_TEMP_EXT, strZ(walSegment))),
                                storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(walSegment))));
                        }
                        // Else the job errored
                        else
                        {
                            LOG_WARN_PID_FMT(
                                processId, "[%s] %s", errorTypeName(errorTypeFromCode(protocolParallelJobErrorCode(job))),
                                strZ(protocolParallelJobErrorMessage(job)));

                            archiveAsyncStatusErrorWrite(
                                archiveModeGet, walSegment, protocolParallelJobErrorCode(job),
                                strNewFmt(
                                    "%s%s", strZ(protocolParallelJobErrorMessage(job)),
                                    strSize(warning) == 0 ? "" : strZ(strNewFmt("\n%s", strZ(warning)))));
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
                LOG_WARN_FMT("[%s] %s", errorTypeName(checkResult.errorType), strZ(checkResult.errorMessage));

                String *message = strDup(checkResult.errorMessage);

                if (!strLstEmpty(checkResult.warnList))
                    strCatFmt(message, "\n%s", strZ(strLstJoin(checkResult.warnList, "\n")));

                archiveAsyncStatusErrorWrite(
                    archiveModeGet, checkResult.errorFile, errorTypeCode(checkResult.errorType), message);
            }
            // Else log a warning if any files were missing
            else if (archiveFileMissing != NULL)
            {
                LOG_DETAIL_FMT(UNABLE_TO_FIND_IN_ARCHIVE_MSG, strZ(archiveFileMissing));

                String *message = NULL;

                if (!strLstEmpty(checkResult.warnList))
                    message = strLstJoin(checkResult.warnList, "\n");

                archiveAsyncStatusOkWrite(archiveModeGet, archiveFileMissing, message);
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
