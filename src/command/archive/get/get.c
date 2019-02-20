/***********************************************************************************************************************************
Archive Get Command
***********************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "command/archive/common.h"
#include "command/archive/get/file.h"
#include "command/command.h"
#include "common/debug.h"
#include "common/fork.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/wait.h"
#include "config/config.h"
#include "config/exec.h"
#include "perl/exec.h"
#include "postgres/interface.h"
#include "storage/helper.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Clean the queue and prepare a list of WAL segments that the async process should get
***********************************************************************************************************************************/
static StringList *
queueNeed(const String *walSegment, bool found, size_t queueSize, size_t walSegmentSize, unsigned int pgVersion)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(BOOL, found);
        FUNCTION_LOG_PARAM(SIZE, queueSize);
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

        // Only preserve files that match the ideal queue. '.error'/'.ok' files are deleted so the async process can try again.
        RegExp *regExpPreserve = regExpNew(strNewFmt("^(%s)$", strPtr(strLstJoin(idealQueue, "|"))));

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
                storageRemoveNP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(file)));
        }

        // Generate a list of the WAL that are needed by removing kept WAL from the ideal queue
        for (unsigned int idealQueueIdx = 0; idealQueueIdx < strLstSize(idealQueue); idealQueueIdx++)
        {
            if (!strLstExists(keepQueue, strLstGet(idealQueue, idealQueueIdx)))
                strLstAdd(result, strLstGet(idealQueue, idealQueueIdx));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Push a WAL segment to the repository
***********************************************************************************************************************************/
int
cmdArchiveGet(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

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

        // Destination is wherever we were told to move the WAL segment.  In some cases the path that PostgreSQL passes will not be
        // absolute so prefix pg-path.
        const String *walDestination = strLstGet(commandParam, 1);

        if (!strBeginsWithZ(walDestination, "/"))
            walDestination = strNewFmt("%s/%s", strPtr(cfgOptionStr(cfgOptPgPath)), strPtr(walDestination));

        // Async get can only be performed on WAL segments, history or other files must use synchronous mode
        if (cfgOptionBool(cfgOptArchiveAsync) && walIsSegment(walSegment))
        {
            bool found = false;                                         // Has the WAL segment been found yet?
            bool queueFull = false;                                     // Is the queue half or more full?
            bool forked = false;                                        // Has the async process been forked yet?
            bool confessOnError = false;                                // Should we confess errors?

            // Loop and wait for the WAL segment to be pushed
            Wait *wait = waitNew((TimeMSec)(cfgOptionDbl(cfgOptArchiveTimeout) * MSEC_PER_SEC));

            do
            {
                // Check for errors or missing files.  For archive-get '.ok' indicates that the process succeeded but there is no
                // WAL file to download.
                if (archiveAsyncStatus(archiveModeGet, walSegment, confessOnError))
                {
                    storageRemoveP(
                        storageSpoolWrite(),
                        strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s.ok", strPtr(walSegment)), .errorOnMissing = true);
                    break;
                }

                // Check if the WAL segment is already in the queue
                found = storageExistsNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(walSegment)));

                // If found then move the WAL segment to the destination directory
                if (found)
                {
                    // Source is the WAL segment in the spool queue
                    StorageFileRead *source = storageNewReadNP(
                        storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(walSegment)));

                    // A move will be attempted but if the spool queue and the WAL path are on different file systems then a copy
                    // will be performed instead.
                    //
                    // It looks scary that we are disabling syncs and atomicity (in case we need to copy intead of move) but this
                    // is safe because if the system crashes Postgres will not try to reuse a restored WAL segment but will instead
                    // request it again using the restore_command. In the case of a move this hardly matters since path syncs are
                    // cheap but if a copy is required we could save a lot of writes.
                    StorageFileWrite *destination = storageNewWriteP(
                        storageLocalWrite(), walDestination, .noCreatePath = true, .noSyncFile = true, .noSyncPath = true,
                        .noAtomic = true);

                    // Move (or copy if required) the file
                    storageMoveNP(storageSpoolWrite(), source, destination);

                    // Return success
                    result = 0;

                    // Get a list of WAL segments left in the queue
                    StringList *queue = storageListP(
                        storageSpool(), STORAGE_SPOOL_ARCHIVE_IN_STR, .expression = WAL_SEGMENT_REGEXP_STR);

                    if (strLstSize(queue) > 0)
                    {
                        // Get size of the WAL segment
                        size_t walSegmentSize = storageInfoNP(storageLocal(), walDestination).size;

                        // Use WAL segment size to estimate queue size and determine if the async process should be launched
                        queueFull =
                            strLstSize(queue) * walSegmentSize > (size_t)cfgOptionInt64(cfgOptArchiveGetQueueMax) / 2;
                    }
                }

                // If the WAL segment has not already been found then start the async process to get it.  There's no point in
                // forking the async process off more than once so track that as well.  Use an archive lock to prevent forking if
                // the async process was launched by another process.
                if (!forked && (!found || !queueFull)  &&
                    lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 0, false))
                {
                    // Get control info
                    PgControl pgControl = pgControlFromFile(cfgOptionStr(cfgOptPgPath));

                    // Create the queue
                    storagePathCreateNP(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN_STR);

                    // The async process should not output on the console at all
                    KeyValue *optionReplace = kvNew();

                    kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptLogLevelConsole))), varNewStrZ("off"));
                    kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptLogLevelStderr))), varNewStrZ("off"));

                    // Generate command options
                    StringList *commandExec = cfgExecParam(cfgCmdArchiveGetAsync, optionReplace);
                    strLstInsert(commandExec, 0, cfgExe());

                    // Clean the current queue using the list of WAL that we ideally want in the queue.  queueNeed()
                    // will return the list of WAL needed to fill the queue and this will be passed to the async process.
                    const StringList *queue = queueNeed(
                        walSegment, found, (size_t)cfgOptionInt64(cfgOptArchiveGetQueueMax), pgControl.walSegmentSize,
                        pgControl.version);

                    for (unsigned int queueIdx = 0; queueIdx < strLstSize(queue); queueIdx++)
                        strLstAdd(commandExec, strLstGet(queue, queueIdx));

                    // Release the lock and mark the async process as forked
                    lockRelease(true);
                    forked = true;

                    // Fork off the async process
                    if (fork() == 0)
                    {
                        // Detach from parent process
                        forkDetach();

                        // Execute the binary.  This statement will not return if it is successful.
                        THROW_ON_SYS_ERROR_FMT(
                            execvp(strPtr(cfgExe()), (char ** const)strLstPtr(commandExec)) == -1,
                            ExecuteError, "unable to execute '%s'", cfgCommandName(cfgCmdArchiveGetAsync));
                    }
                }

                // Exit loop if WAL was found
                if (found)
                    break;

                // Now that the async process has been launched, confess any errors that are found
                confessOnError = true;
            }
            while (waitMore(wait));
        }
        // Else perform synchronous get
        else
        {
            // Get the repo storage in case it is remote and encryption settings need to be pulled down
            storageRepo();

            // Get the archive file
            result = archiveGetFile(
                walSegment, walDestination, cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
        }

        // Log whether or not the file was found
        if (result == 0)
            LOG_INFO("found %s in the archive", strPtr(walSegment));
        else
            LOG_INFO("unable to find %s in the archive", strPtr(walSegment));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}
