/***********************************************************************************************************************************
Archive Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "command/archive/common.h"
#include "common/debug.h"
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

/***********************************************************************************************************************************
Check for ok/error status files in the spool in/out directory
***********************************************************************************************************************************/
bool
archiveAsyncStatus(ArchiveMode archiveMode, const String *walSegment, bool confessOnError)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, archiveMode);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(BOOL, confessOnError);
    FUNCTION_LOG_END();

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *errorFile = NULL;
        bool errorFileExists = false;

        const String *spoolQueue = archiveAsyncSpoolQueue(archiveMode);

        String *okFile = strNewFmt("%s" STATUS_EXT_OK, strPtr(walSegment));
        bool okFileExists = storageExistsNP(storageSpool(), strNewFmt("%s/%s", strPtr(spoolQueue), strPtr(okFile)));

        // If the ok file does not exist then check to see if a file-specific or global error exists
        if (!okFileExists)
        {
            // Check for a file-specific error first
            errorFile = strNewFmt("%s" STATUS_EXT_ERROR, strPtr(walSegment));
            errorFileExists = storageExistsNP(storageSpool(), strNewFmt("%s/%s", strPtr(spoolQueue), strPtr(errorFile)));

            // If that doesn't exist then check for a global error
            if (!errorFileExists)
            {
                errorFile = STRDEF(STATUS_FILE_GLOBAL STATUS_EXT_ERROR);
                errorFileExists = storageExistsNP(storageSpool(), strNewFmt("%s/%s", strPtr(spoolQueue), strPtr(errorFile)));
            }
        }

        // If either of them exists then check what happened and report back
        if (okFileExists || errorFileExists)
        {
            // Get the status file content
            const String *statusFile = okFileExists ? okFile: errorFile;

            String *content = strNewBuf(
                storageGetNP(storageNewReadNP(storageSpool(), strNewFmt("%s/%s", strPtr(spoolQueue), strPtr(statusFile)))));

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
            else if (confessOnError)
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

/***********************************************************************************************************************************
Write an error status file
***********************************************************************************************************************************/
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

        storagePutNP(
            storageNewWriteNP(
                storageSpoolWrite(),
                strNewFmt("%s/%s" STATUS_EXT_ERROR, strPtr(archiveAsyncSpoolQueue(archiveMode)), strPtr(errorFile))),
            BUFSTR(strNewFmt("%d\n%s", code, strPtr(message))));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write an ok status file
***********************************************************************************************************************************/
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
        storagePutNP(
            storageNewWriteNP(
                storageSpoolWrite(),
                strNewFmt("%s/%s" STATUS_EXT_OK, strPtr(archiveAsyncSpoolQueue(archiveMode)), strPtr(walSegment))),
            warning == NULL ? NULL : BUFSTR(strNewFmt("0\n%s", strPtr(warning))));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is the segment partial?
***********************************************************************************************************************************/
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

/***********************************************************************************************************************************
Generates the location of the wal directory using a relative wal path and the supplied pg path
***********************************************************************************************************************************/
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
        if (pgPath == NULL)
        {
            THROW_FMT(
                OptionRequiredError,
                "option '" CFGOPT_PG1_PATH "' must be specified when relative wal paths are used\n"
                    "HINT: Is %%f passed to %s instead of %%p?\n"
                    "HINT: PostgreSQL may pass relative paths even with %%p depending on the environment.",
                strPtr(command));
        }

        result = strNewFmt("%s/%s", strPtr(pgPath), strPtr(walFile));
    }
    else
        result = strDup(walFile);

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Is the file a segment or some other file (e.g. .history, .backup, etc)
***********************************************************************************************************************************/
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

/***********************************************************************************************************************************
Find a WAL segment in the repository

The file name can have several things appended such as a hash, compression extension, and partial extension so it is possible to
have multiple files that match the segment, though more than one match is not a good thing.
***********************************************************************************************************************************/
String *
walSegmentFind(const Storage *storage, const String *archiveId, const String *walSegment)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, archiveId);
        FUNCTION_LOG_PARAM(STRING, walSegment);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(archiveId != NULL);
    ASSERT(walSegment != NULL);
    ASSERT(walIsSegment(walSegment));

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get a list of all WAL segments that match
        StringList *list = storageListP(
            storage, strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(strSubN(walSegment, 0, 16))),
            .expression = strNewFmt("^%s%s-[0-f]{40}(\\.gz){0,1}$", strPtr(strSubN(walSegment, 0, 24)),
                walIsPartial(walSegment) ? WAL_SEGMENT_PARTIAL_EXT : ""));

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

            // Copy file name of WAL segment found into the calling context
            memContextSwitch(MEM_CONTEXT_OLD());
            result = strDup(strLstGet(list, 0));
            memContextSwitch(MEM_CONTEXT_TEMP());
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Get the next WAL segment given a WAL segment and WAL segment size
***********************************************************************************************************************************/
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

/***********************************************************************************************************************************
Build a list of WAL segments based on a beginning WAL and number of WAL in the range (inclusive)
***********************************************************************************************************************************/
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
        result = strLstAdd(strLstNew(), walSegmentBegin);

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

        strLstMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}
