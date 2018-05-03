/***********************************************************************************************************************************
Archive Push Command
***********************************************************************************************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "command/archive/common.h"
#include "common/assert.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/wait.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Check for ok/error status files in the spool in/out directory
***********************************************************************************************************************************/
bool
archiveAsyncStatus(ArchiveMode archiveMode, const String *walSegment, bool confessOnError)
{
    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *spoolQueue = strNew(archiveMode == archiveModeGet ? STORAGE_SPOOL_ARCHIVE_IN : STORAGE_SPOOL_ARCHIVE_OUT);

        StringList *fileList = storageListP(
            storageSpool(), spoolQueue, .expression = strNewFmt("^%s\\.(ok|error)$", strPtr(walSegment)));

        if (fileList != NULL && strLstSize(fileList) > 0)
        {
            // If more than one status file was found then assert - this could be a bug in the async process
            if (strLstSize(fileList) != 1)
            {
                THROW_FMT(
                    AssertError, "multiple status files found in '%s' for WAL segment '%s'",
                    strPtr(storagePath(storageSpool(), spoolQueue)), strPtr(walSegment));
            }

            // Get the status file content
            const String *statusFile = strLstGet(fileList, 0);

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
                code = varIntForce(varNewStr(strNewN(strPtr(content), (size_t)(linefeedPtr - strPtr(content)))));
                message = strTrim(strNew(linefeedPtr + 1));
            }

            // Process OK files
            if (strEndsWithZ(statusFile, ".ok"))
            {
                // If there is content in the status file it is a warning
                if (strSize(content) != 0)
                {
                    // If error code is not success, then this was a renamed .error file
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

    return result;
}

/***********************************************************************************************************************************
Get the next WAL segment given a WAL segment and WAL segment size
***********************************************************************************************************************************/
String *
walSegmentNext(const String *walSegment, size_t walSegmentSize, uint pgVersion)
{
    ASSERT_DEBUG(walSegment != NULL);
    ASSERT_DEBUG(strSize(walSegment) == 24);
    ASSERT_DEBUG(UINT32_MAX % walSegmentSize == walSegmentSize - 1);
    ASSERT_DEBUG(pgVersion >= PG_VERSION_11 || walSegmentSize == 16 * 1024 * 1024);

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

    return strNewFmt("%08X%08X%08X", timeline, major, minor);
}

/***********************************************************************************************************************************
Build a list of WAL segments based on a beginning WAL and number of WAL in the range (inclusive)
***********************************************************************************************************************************/
StringList *
walSegmentRange(const String *walSegmentBegin, size_t walSegmentSize, uint pgVersion, uint range)
{
    ASSERT_DEBUG(range > 0);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = strLstAdd(strLstNew(), walSegmentBegin);

        if (range > 1)
        {
            String *current = strDup(walSegmentBegin);

            for (uint rangeIdx = 0; rangeIdx < range - 1; rangeIdx++)
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

    return result;
}
