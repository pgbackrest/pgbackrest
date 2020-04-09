/***********************************************************************************************************************************
Archive Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "command/archive/common.h"
#include "common/debug.h"
#include "common/fork.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/wait.h"
#include "config/config.h"
#include "postgres/version.h"
#include "storage/helper.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
WAL segment constants
***********************************************************************************************************************************/
STRING_EXTERN(WAL_SEGMENT_REGEXP_STR,                               WAL_SEGMENT_REGEXP);
STRING_EXTERN(WAL_SEGMENT_PARTIAL_REGEXP_STR,                       WAL_SEGMENT_PARTIAL_REGEXP);
STRING_EXTERN(WAL_SEGMENT_DIR_REGEXP_STR,                           WAL_SEGMENT_DIR_REGEXP);
STRING_EXTERN(WAL_SEGMENT_FILE_REGEXP_STR,                          WAL_SEGMENT_FILE_REGEXP);

/***********************************************************************************************************************************
Global error file constant
***********************************************************************************************************************************/
#define STATUS_FILE_GLOBAL                                          "global"
    STRING_STATIC(STATUS_FILE_GLOBAL_STR,                           STATUS_FILE_GLOBAL);

#define STATUS_FILE_GLOBAL_ERROR                                    STATUS_FILE_GLOBAL STATUS_EXT_ERROR
    STRING_STATIC(STATUS_FILE_GLOBAL_ERROR_STR,                         STATUS_FILE_GLOBAL_ERROR);

/***********************************************************************************************************************************
Get the correct spool queue based on the archive mode
***********************************************************************************************************************************/
static const String *
archiveAsyncSpoolQueue(ArchiveMode archiveMode)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, archiveMode);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN((archiveMode == archiveModeGet ? STORAGE_SPOOL_ARCHIVE_IN_STR : STORAGE_SPOOL_ARCHIVE_OUT_STR));
}

/**********************************************************************************************************************************/
void
archiveAsyncErrorClear(ArchiveMode archiveMode, const String *archiveFile)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, archiveMode);
        FUNCTION_LOG_PARAM(STRING, archiveFile);
    FUNCTION_LOG_END();

    storageRemoveP(
        storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s" STATUS_EXT_ERROR, strPtr(archiveFile)));
    storageRemoveP(storageSpoolWrite(), STRDEF(STORAGE_SPOOL_ARCHIVE_OUT "/" STATUS_FILE_GLOBAL_ERROR));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
archiveAsyncStatus(ArchiveMode archiveMode, const String *walSegment, bool throwOnError)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, archiveMode);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(BOOL, throwOnError);
    FUNCTION_LOG_END();

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *errorFile = NULL;
        bool errorFileExists = false;

        const String *spoolQueue = archiveAsyncSpoolQueue(archiveMode);

        String *okFile = strNewFmt("%s" STATUS_EXT_OK, strPtr(walSegment));
        bool okFileExists = storageExistsP(storageSpool(), strNewFmt("%s/%s", strPtr(spoolQueue), strPtr(okFile)));

        // If the ok file does not exist then check to see if a file-specific or global error exists
        if (!okFileExists)
        {
            // Check for a file-specific error first
            errorFile = strNewFmt("%s" STATUS_EXT_ERROR, strPtr(walSegment));
            errorFileExists = storageExistsP(storageSpool(), strNewFmt("%s/%s", strPtr(spoolQueue), strPtr(errorFile)));

            // If that doesn't exist then check for a global error
            if (!errorFileExists)
            {
                errorFile = STATUS_FILE_GLOBAL_ERROR_STR;
                errorFileExists = storageExistsP(storageSpool(), strNewFmt("%s/%s", strPtr(spoolQueue), strPtr(errorFile)));
            }
        }

        // If either of them exists then check what happened and report back
        if (okFileExists || errorFileExists)
        {
            // Get the status file content
            const String *statusFile = okFileExists ? okFile: errorFile;

            String *content = strNewBuf(
                storageGetP(storageNewReadP(storageSpool(), strNewFmt("%s/%s", strPtr(spoolQueue), strPtr(statusFile)))));

            // Get the code and message if the file has content
            int code = 0;
            const String *message = NULL;

            if (strSize(content) != 0)
            {
                // Find the line feed after the error code -- should be the first one
                const char *linefeedPtr = strchr(strPtr(content), '\n');

                // Error if linefeed not found
                if (linefeedPtr == NULL)
                    THROW_FMT(FormatError, "%s content must have at least two lines", strPtr(statusFile));

                // Error if message is zero-length
                if (strlen(linefeedPtr + 1) == 0)
                    THROW_FMT(FormatError, "%s message must be > 0", strPtr(statusFile));

                // Get contents
                code = varIntForce(VARSTR(strNewN(strPtr(content), (size_t)(linefeedPtr - strPtr(content)))));
                message = strTrim(strNew(linefeedPtr + 1));
            }

            // Process OK files
            if (okFileExists)
            {
                // If there is content in the status file it is a warning
                if (strSize(content) != 0)
                {
                    // If error code is not success, then this was a renamed error file
                    if (code != 0)
                    {
                        message = strNewFmt(
                            "WAL segment '%s' was not pushed due to error [%d] and was manually skipped: %s", strPtr(walSegment),
                            code, strPtr(message));
                    }

                    LOG_WARN(strPtr(message));
                }

                result = true;
            }
            else if (throwOnError)
            {
                // Error status files must have content
                if (strSize(content) == 0)
                    THROW_FMT(AssertError, "status file '%s' has no content", strPtr(statusFile));

                // Throw error using the code passed in the file
                THROW_CODE(code, strPtr(message));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
void
archiveAsyncStatusErrorWrite(ArchiveMode archiveMode, const String *walSegment, int code, const String *message)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, archiveMode);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(INT, code);
        FUNCTION_LOG_PARAM(STRING, message);
    FUNCTION_LOG_END();

    ASSERT(code != 0);
    ASSERT(message != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *errorFile = walSegment == NULL ? STATUS_FILE_GLOBAL_STR : walSegment;

        storagePutP(
            storageNewWriteP(
                storageSpoolWrite(),
                strNewFmt("%s/%s" STATUS_EXT_ERROR, strPtr(archiveAsyncSpoolQueue(archiveMode)), strPtr(errorFile))),
            BUFSTR(strNewFmt("%d\n%s", code, strPtr(message))));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
archiveAsyncStatusOkWrite(ArchiveMode archiveMode, const String *walSegment, const String *warning)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, archiveMode);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(STRING, warning);
    FUNCTION_LOG_END();

    ASSERT(walSegment != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Write file
        storagePutP(
            storageNewWriteP(
                storageSpoolWrite(),
                strNewFmt("%s/%s" STATUS_EXT_OK, strPtr(archiveAsyncSpoolQueue(archiveMode)), strPtr(walSegment))),
            warning == NULL ? NULL : BUFSTR(strNewFmt("0\n%s", strPtr(warning))));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
archiveAsyncExec(ArchiveMode archiveMode, const StringList *commandExec)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, archiveMode);
        FUNCTION_LOG_PARAM(STRING_LIST, commandExec);
    FUNCTION_LOG_END();

    ASSERT(commandExec != NULL);

    // Fork off the async process
    pid_t pid = forkSafe();

    if (pid == 0)
    {
        // Disable logging and close log file
        logClose();

        // Detach from parent process
        forkDetach();

        // Execute the binary.  This statement will not return if it is successful.
        THROW_ON_SYS_ERROR_FMT(
            execvp(strPtr(strLstGet(commandExec, 0)), (char ** const)strLstPtr(commandExec)) == -1, ExecuteError,
            "unable to execute asynchronous '%s'", archiveMode == archiveModeGet ? CFGCMD_ARCHIVE_GET : CFGCMD_ARCHIVE_PUSH);
    }

#ifdef DEBUG_EXEC_TIME
    // Get the time to measure how long it takes for the forked process to exit
    TimeMSec timeBegin = timeMSec();
#endif

    // The process that was just forked should return immediately
    int processStatus;

    THROW_ON_SYS_ERROR(waitpid(pid, &processStatus, 0) == -1, ExecuteError, "unable to wait for forked process");

    // The first fork should exit with success.  If not, something went wrong during the second fork.
    CHECK(WIFEXITED(processStatus) && WEXITSTATUS(processStatus) == 0);

#ifdef DEBUG_EXEC_TIME
    // If the process does not exit immediately then something probably went wrong with the double fork.  It's possible that this
    // test will fail on very slow systems so it may need to be tuned.  The idea is to make sure that the waitpid() above is not
    // waiting on the async process.
    ASSERT(timeMSec() - timeBegin < 10);
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
walIsPartial(const String *walSegment)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, walSegment);
    FUNCTION_LOG_END();

    ASSERT(walSegment != NULL);
    ASSERT(walIsSegment(walSegment));

    FUNCTION_LOG_RETURN(BOOL, strEndsWithZ(walSegment, WAL_SEGMENT_PARTIAL_EXT));
}

/**********************************************************************************************************************************/
String *
walPath(const String *walFile, const String *pgPath, const String *command)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walFile);
        FUNCTION_LOG_PARAM(STRING, pgPath);
        FUNCTION_LOG_PARAM(STRING, command);
    FUNCTION_LOG_END();

    ASSERT(walFile != NULL);
    ASSERT(command != NULL);

    String *result = NULL;

    if (!strBeginsWithZ(walFile, "/"))
    {
        // Error if walFile has a relative path and pgPath is not set
        if (pgPath == NULL)
        {
            THROW_FMT(
                OptionRequiredError,
                "option '" CFGOPT_PG1_PATH "' must be specified when relative wal paths are used\n"
                    "HINT: is %%f passed to %s instead of %%p?\n"
                    "HINT: PostgreSQL may pass relative paths even with %%p depending on the environment.",
                strPtr(command));
        }

        // Get the working directory
        char currentWorkDir[4096];
        THROW_ON_SYS_ERROR(getcwd(currentWorkDir, sizeof(currentWorkDir)) == NULL, FormatError, "unable to get cwd");

        // Check if the working directory is the same as pgPath
        if (!strEqZ(pgPath, currentWorkDir))
        {
            // If not we'll change the working directory to pgPath and see if that equals the working directory we got called with
            THROW_ON_SYS_ERROR_FMT(chdir(strPtr(pgPath)) != 0, PathMissingError, "unable to chdir() to '%s'", strPtr(pgPath));

            // Get the new working directory
            char newWorkDir[4096];
            THROW_ON_SYS_ERROR(getcwd(newWorkDir, sizeof(newWorkDir)) == NULL, FormatError, "unable to get cwd");

            // Error if the new working directory is not equal to the original current working directory.  This means that
            // PostgreSQL and pgBackrest have a different idea about where the PostgreSQL data directory is located.
            if (strcmp(currentWorkDir, newWorkDir) != 0)
                THROW_FMT(AssertError, "working path '%s' is not the same path as '%s'", currentWorkDir, strPtr(pgPath));
        }

        result = strNewFmt("%s/%s", strPtr(pgPath), strPtr(walFile));
    }
    else
        result = strDup(walFile);

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
bool
walIsSegment(const String *walSegment)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, walSegment);
    FUNCTION_LOG_END();

    ASSERT(walSegment != NULL);

    // Create the regular expression to identify WAL segments if it does not already exist
    static RegExp *regExpSegment = NULL;

    if (regExpSegment == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            regExpSegment = regExpNew(WAL_SEGMENT_PARTIAL_REGEXP_STR);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(BOOL, regExpMatch(regExpSegment, walSegment));
}

/**********************************************************************************************************************************/
String *
walSegmentFind(const Storage *storage, const String *archiveId, const String *walSegment, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, archiveId);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(archiveId != NULL);
    ASSERT(walSegment != NULL);
    ASSERT(walIsSegment(walSegment));

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Wait *wait = waitNew(timeout);

        do
        {
            // Get a list of all WAL segments that match
            StringList *list = storageListP(
                storage, strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(strSubN(walSegment, 0, 16))),
                .expression = strNewFmt("^%s%s-[0-f]{40}" COMPRESS_TYPE_REGEXP "{0,1}$", strPtr(strSubN(walSegment, 0, 24)),
                    walIsPartial(walSegment) ? WAL_SEGMENT_PARTIAL_EXT : ""), .nullOnMissing = true);

            // If there are results
            if (list != NULL && strLstSize(list) > 0)
            {
                // Error if there is more than one match
                if (strLstSize(list) > 1)
                {
                    THROW_FMT(
                        ArchiveDuplicateError,
                        "duplicates found in archive for WAL segment %s: %s\n"
                            "HINT: are multiple primaries archiving to this stanza?",
                        strPtr(walSegment), strPtr(strLstJoin(strLstSort(list, sortOrderAsc), ", ")));
                }

                // Copy file name of WAL segment found into the prior context
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = strDup(strLstGet(list, 0));
                }
                MEM_CONTEXT_PRIOR_END();
            }
        }
        while (result == NULL && waitMore(wait));
    }
    MEM_CONTEXT_TEMP_END();

    if (result == NULL && timeout != 0)
    {
        THROW_FMT(
            ArchiveTimeoutError,
            "WAL segment %s was not archived before the %" PRIu64 "ms timeout\n"
                "HINT: check the archive_command to ensure that all options are correct (especially --stanza).\n"
                "HINT: check the PostgreSQL server log for errors.",
            strPtr(walSegment), timeout);
    }

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
String *
walSegmentNext(const String *walSegment, size_t walSegmentSize, unsigned int pgVersion)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(SIZE, walSegmentSize);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
    FUNCTION_LOG_END();

    ASSERT(walSegment != NULL);
    ASSERT(strSize(walSegment) == 24);
    ASSERT(UINT32_MAX % walSegmentSize == walSegmentSize - 1);
    ASSERT(pgVersion >= PG_VERSION_11 || walSegmentSize == 16 * 1024 * 1024);

    // Extract WAL parts
    uint32_t timeline = 0;
    uint32_t major = 0;
    uint32_t minor = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        timeline = (uint32_t)strtol(strPtr(strSubN(walSegment, 0, 8)), NULL, 16);
        major = (uint32_t)strtol(strPtr(strSubN(walSegment, 8, 8)), NULL, 16);
        minor = (uint32_t)strtol(strPtr(strSubN(walSegment, 16, 8)), NULL, 16);

        // Increment minor and adjust major dir on overflow
        minor++;

        if (minor > UINT32_MAX / walSegmentSize)
        {
            major++;
            minor = 0;
        }

        // Special hack for PostgreSQL < 9.3 which skipped minor FF
        if (minor == 0xFF && pgVersion < PG_VERSION_93)
        {
            major++;
            minor = 0;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, strNewFmt("%08X%08X%08X", timeline, major, minor));
}

/**********************************************************************************************************************************/
StringList *
walSegmentRange(const String *walSegmentBegin, size_t walSegmentSize, unsigned int pgVersion, unsigned int range)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walSegmentBegin);
        FUNCTION_LOG_PARAM(SIZE, walSegmentSize);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
    FUNCTION_LOG_END();

    ASSERT(range > 0);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = strLstNew();
        strLstAdd(result, walSegmentBegin);

        if (range > 1)
        {
            String *current = strDup(walSegmentBegin);

            for (unsigned int rangeIdx = 0; rangeIdx < range - 1; rangeIdx++)
            {
                String *next = walSegmentNext(current, walSegmentSize, pgVersion);

                strLstAdd(result, next);

                strFree(current);
                current = next;
            }
        }

        strLstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}
