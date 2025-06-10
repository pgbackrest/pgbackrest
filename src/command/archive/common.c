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
#include "common/type/convert.h"
#include "common/wait.h"
#include "config/config.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
WAL segment constants
***********************************************************************************************************************************/
STRING_EXTERN(WAL_SEGMENT_REGEXP_STR,                               WAL_SEGMENT_REGEXP);
STRING_EXTERN(WAL_SEGMENT_PARTIAL_REGEXP_STR,                       WAL_SEGMENT_PARTIAL_REGEXP);
STRING_EXTERN(WAL_SEGMENT_DIR_REGEXP_STR,                           WAL_SEGMENT_DIR_REGEXP);
STRING_EXTERN(WAL_SEGMENT_FILE_REGEXP_STR,                          WAL_SEGMENT_FILE_REGEXP);
STRING_EXTERN(WAL_TIMELINE_HISTORY_REGEXP_STR,                      WAL_TIMELINE_HISTORY_REGEXP);

/***********************************************************************************************************************************
Global error file constant
***********************************************************************************************************************************/
#define STATUS_FILE_GLOBAL                                          "global"
STRING_STATIC(STATUS_FILE_GLOBAL_STR,                               STATUS_FILE_GLOBAL);

#define STATUS_FILE_GLOBAL_ERROR                                    STATUS_FILE_GLOBAL STATUS_EXT_ERROR
STRING_STATIC(STATUS_FILE_GLOBAL_ERROR_STR,                         STATUS_FILE_GLOBAL_ERROR);

/***********************************************************************************************************************************
Get the correct spool queue based on the archive mode
***********************************************************************************************************************************/
static const String *
archiveAsyncSpoolQueue(const ArchiveMode archiveMode)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, archiveMode);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_CONST(
        STRING, archiveMode == archiveModeGet ? STORAGE_SPOOL_ARCHIVE_IN_STR : STORAGE_SPOOL_ARCHIVE_OUT_STR);
}

/**********************************************************************************************************************************/
FN_EXTERN void
archiveAsyncErrorClear(const ArchiveMode archiveMode, const String *const archiveFile)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, archiveMode);
        FUNCTION_LOG_PARAM(STRING, archiveFile);
    FUNCTION_LOG_END();

    ASSERT(archiveFile != NULL);

    String *const errorFile = strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s" STATUS_EXT_ERROR, strZ(archiveFile));

    storageRemoveP(storageSpoolWrite(), errorFile);
    storageRemoveP(storageSpoolWrite(), STRDEF(STORAGE_SPOOL_ARCHIVE_OUT "/" STATUS_FILE_GLOBAL_ERROR));

    strFree(errorFile);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN bool
archiveAsyncStatus(const ArchiveMode archiveMode, const String *const walSegment, const bool throwOnError, const bool warnOnOk)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, archiveMode);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(BOOL, throwOnError);
        FUNCTION_LOG_PARAM(BOOL, warnOnOk);
    FUNCTION_LOG_END();

    ASSERT(walSegment != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *errorFile = NULL;
        bool errorFileExists = false;

        const String *const spoolQueue = archiveAsyncSpoolQueue(archiveMode);

        String *const okFile = strNewFmt("%s" STATUS_EXT_OK, strZ(walSegment));
        const bool okFileExists = storageExistsP(storageSpool(), strNewFmt("%s/%s", strZ(spoolQueue), strZ(okFile)));

        // If the ok file does not exist then check to see if a file-specific or global error exists
        if (!okFileExists)
        {
            // Check for a file-specific error first
            errorFile = strNewFmt("%s" STATUS_EXT_ERROR, strZ(walSegment));
            errorFileExists = storageExistsP(storageSpool(), strNewFmt("%s/%s", strZ(spoolQueue), strZ(errorFile)));

            // If that doesn't exist then check for a global error
            if (!errorFileExists)
            {
                errorFile = STATUS_FILE_GLOBAL_ERROR_STR;
                errorFileExists = storageExistsP(storageSpool(), strNewFmt("%s/%s", strZ(spoolQueue), strZ(errorFile)));
            }
        }

        // If either of them exists then check what happened and report back
        if (okFileExists || errorFileExists)
        {
            // Get the status file content
            const String *const statusFile = okFileExists ? okFile : errorFile;

            const String *const content = strNewBuf(
                storageGetP(storageNewReadP(storageSpool(), strNewFmt("%s/%s", strZ(spoolQueue), strZ(statusFile)))));

            // Get the code and message if the file has content
            int code = 0;
            const String *message = NULL;

            if (strSize(content) != 0)
            {
                // Find the line feed after the error code -- should be the first one
                const char *const linefeedPtr = strchr(strZ(content), '\n');

                // Error if linefeed not found
                if (linefeedPtr == NULL)
                    THROW_FMT(FormatError, "%s content must have at least two lines", strZ(statusFile));

                // Error if message is zero-length
                if (strlen(linefeedPtr + 1) == 0)
                    THROW_FMT(FormatError, "%s message must be > 0", strZ(statusFile));

                // Get contents
                code = varIntForce(VARSTR(strNewZN(strZ(content), (size_t)(linefeedPtr - strZ(content)))));
                message = strTrim(strNewZ(linefeedPtr + 1));
            }

            // Process OK files
            if (okFileExists)
            {
                // If there is content in the status file it is a warning
                if (strSize(content) != 0 && warnOnOk)
                {
                    // If error code is not success, then this was a renamed error file
                    if (code != 0)
                    {
                        message = strNewFmt(
                            "WAL segment '%s' was not pushed due to error [%d] and was manually skipped: %s", strZ(walSegment),
                            code, strZ(message));
                    }

                    LOG_WARN(strZ(message));
                }

                result = true;
            }
            else if (throwOnError)
            {
                // Error status files must have content
                if (strSize(content) == 0)
                    THROW_FMT(AssertError, "status file '%s' has no content", strZ(statusFile));

                // Throw error using the code passed in the file
                THROW_CODE(code, strZ(message));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
archiveAsyncStatusErrorWrite(
    const ArchiveMode archiveMode, const String *const walSegment, const int code, const String *const message)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, archiveMode);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(INT, code);
        FUNCTION_LOG_PARAM(STRING, message);
    FUNCTION_LOG_END();

    ASSERT(code != 0);
    ASSERT(message != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const errorFile = walSegment == NULL ? STATUS_FILE_GLOBAL_STR : walSegment;

        storagePutP(
            storageNewWriteP(
                storageSpoolWrite(),
                strNewFmt("%s/%s" STATUS_EXT_ERROR, strZ(archiveAsyncSpoolQueue(archiveMode)), strZ(errorFile))),
            BUFSTR(strNewFmt("%d\n%s", code, strZ(message))));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
archiveAsyncStatusOkWrite(const ArchiveMode archiveMode, const String *const walSegment, const String *const warning)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, archiveMode);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(STRING, warning);
    FUNCTION_LOG_END();

    ASSERT(walSegment != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Write file
        storagePutP(
            storageNewWriteP(
                storageSpoolWrite(), strNewFmt("%s/%s" STATUS_EXT_OK, strZ(archiveAsyncSpoolQueue(archiveMode)), strZ(walSegment))),
            warning == NULL ? NULL : BUFSTR(strNewFmt("0\n%s", strZ(warning))));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
archiveAsyncExec(const ArchiveMode archiveMode, const StringList *const commandExec)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, archiveMode);
        FUNCTION_LOG_PARAM(STRING_LIST, commandExec);
    FUNCTION_LOG_END();

    ASSERT(commandExec != NULL);

    // Fork off the async process
    const pid_t pid = forkSafe();

    if (pid == 0)
    {
        // Disable logging and close log file
        logClose();

        // Detach from parent process
        forkDetach();

        // Close any open file descriptors above the standard three (stdin, stdout, stderr). Don't check the return value since we
        // don't know which file descriptors are actually open (might be none). It's possible that there are open files >= 1024 but
        // there is no easy way to detect that and this should give us enough descriptors to do our work.
        for (int fd = 3; fd < 1024; fd++)
            close(fd);

        // Execute the binary. This statement will not return if it is successful. execvp() requires non-const parameters because it
        // modifies them after the fork.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        THROW_ON_SYS_ERROR_FMT(
            execvp(strZ(strLstGet(commandExec, 0)), UNCONSTIFY(char **, strLstPtr(commandExec))) == -1, ExecuteError,
            "unable to execute asynchronous '%s'", archiveMode == archiveModeGet ? CFGCMD_ARCHIVE_GET : CFGCMD_ARCHIVE_PUSH);
#pragma GCC diagnostic pop
    }

#ifdef DEBUG_EXEC_TIME
    // Get the time to measure how long it takes for the forked process to exit
    const TimeMSec timeBegin = timeMSec();
#endif

    // The process that was just forked should return immediately
    int processStatus;

    THROW_ON_SYS_ERROR(waitpid(pid, &processStatus, 0) == -1, ExecuteError, "unable to wait for forked process");

    // The first fork should exit with success. If not, something went wrong during the second fork.
    CHECK(ExecuteError, WIFEXITED(processStatus) && WEXITSTATUS(processStatus) == 0, "error on first fork");

#ifdef DEBUG_EXEC_TIME
    // If the process does not exit immediately then something probably went wrong with the double fork. It's possible that this
    // test will fail on very slow systems so it may need to be tuned. The idea is to make sure that the waitpid() above is not
    // waiting on the async process.
    CHECK(AssertError, timeMSec() - timeBegin < 10, "the process does not exit immediately");
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN int
archiveIdComparator(const void *const archiveId1, const void *const archiveId2)
{
    ASSERT(strstr(strZ(*(const String *const *)archiveId1), "-") != NULL);
    ASSERT(strstr(strZ(*(const String *const *)archiveId2), "-") != NULL);

    const int id1 = cvtZToInt(strstr(strZ(*(const String *const *)archiveId1), "-") + 1);
    const int id2 = cvtZToInt(strstr(strZ(*(const String *const *)archiveId2), "-") + 1);

    return LST_COMPARATOR_CMP(id1, id2);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
walIsPartial(const String *const walSegment)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, walSegment);
    FUNCTION_LOG_END();

    ASSERT(walSegment != NULL);
    ASSERT(walIsSegment(walSegment));

    FUNCTION_LOG_RETURN(BOOL, strEndsWithZ(walSegment, WAL_SEGMENT_PARTIAL_EXT));
}

/**********************************************************************************************************************************/
FN_EXTERN String *
walPath(const String *walFile, const String *pgPath, const String *command)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walFile);
        FUNCTION_LOG_PARAM(STRING, pgPath);
        FUNCTION_LOG_PARAM(STRING, command);
    FUNCTION_LOG_END();

    ASSERT(walFile != NULL);
    ASSERT(command != NULL);

    String *result;

    if (!strBeginsWithZ(walFile, "/"))
    {
        // Error if walFile has a relative path and pgPath is not set
        if (pgPath == NULL)
        {
            THROW_FMT(
                OptionRequiredError,
                "option '%s' must be specified when relative wal paths are used\n"
                "HINT: is %%f passed to %s instead of %%p?\n"
                "HINT: PostgreSQL may pass relative paths even with %%p depending on the environment.",
                cfgOptionName(cfgOptPgPath), strZ(command));
        }

        // Get the working directory
        char currentWorkDir[4096];
        THROW_ON_SYS_ERROR(getcwd(currentWorkDir, sizeof(currentWorkDir)) == NULL, FormatError, "unable to get cwd");

        // Check if the working directory is the same as pgPath
        if (!strEqZ(pgPath, currentWorkDir))
        {
            // If not we'll change the working directory to pgPath and see if that equals the working directory we got called with
            THROW_ON_SYS_ERROR_FMT(chdir(strZ(pgPath)) != 0, PathMissingError, "unable to chdir() to '%s'", strZ(pgPath));

            // Get the new working directory
            char newWorkDir[4096];
            THROW_ON_SYS_ERROR(getcwd(newWorkDir, sizeof(newWorkDir)) == NULL, FormatError, "unable to get cwd");

            // Error if the new working directory is not equal to the original current working directory. This means that PostgreSQL
            // and pgBackrest have a different idea about where the PostgreSQL data directory is located.
            if (strcmp(currentWorkDir, newWorkDir) != 0)
            {
                THROW_FMT(
                    OptionInvalidValueError,
                    PG_NAME " working directory '%s' is not the same as option %s '%s'\n"
                    "HINT: is the " PG_NAME " data_directory configured the same as the %s option?",
                    currentWorkDir, cfgOptionName(cfgOptPgPath), strZ(pgPath), cfgOptionName(cfgOptPgPath));
            }
        }

        result = strNewFmt("%s/%s", strZ(pgPath), strZ(walFile));
    }
    else
        result = strDup(walFile);

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
walIsSegment(const String *const walSegment)
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
FN_EXTERN String *
walSegmentNext(const String *const walSegment, const size_t walSegmentSize, const unsigned int pgVersion)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(SIZE, walSegmentSize);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
    FUNCTION_LOG_END();

    ASSERT(walSegment != NULL);
    ASSERT(strSize(walSegment) == 24);
    ASSERT(UINT32_MAX % walSegmentSize == walSegmentSize - 1);

    // Extract WAL parts
    uint32_t timeline;
    uint32_t major;
    uint32_t minor;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        timeline = pgTimelineFromWalSegment(walSegment);
        major = (uint32_t)strtol(strZ(strSubN(walSegment, 8, 8)), NULL, 16);
        minor = (uint32_t)strtol(strZ(strSubN(walSegment, 16, 8)), NULL, 16);

        // Increment minor and adjust major dir on overflow
        minor++;

        if (minor > UINT32_MAX / walSegmentSize)
        {
            major++;
            minor = 0;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, strNewFmt("%08X%08X%08X", timeline, major, minor));
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
walSegmentRange(
    const String *const walSegmentBegin, const size_t walSegmentSize, const unsigned int pgVersion, const unsigned int range)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walSegmentBegin);
        FUNCTION_LOG_PARAM(SIZE, walSegmentSize);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
    FUNCTION_LOG_END();

    ASSERT(range > 0);

    StringList *const result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        strLstAdd(result, walSegmentBegin);

        if (range > 1)
        {
            String *current = strDup(walSegmentBegin);

            for (unsigned int rangeIdx = 0; rangeIdx < range - 1; rangeIdx++)
            {
                String *const next = walSegmentNext(current, walSegmentSize, pgVersion);

                strLstAdd(result, next);

                strFree(current);
                current = next;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}
