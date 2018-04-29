/***********************************************************************************************************************************
Archive Push Command
***********************************************************************************************************************************/
#include <string.h>

#include "command/archive/common.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/wait.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Check for ok/error status files in the spool in/out directory
***********************************************************************************************************************************/
bool
archiveAsyncStatus(const String *walSegment, bool confessOnError)
{
    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StringList *fileList = storageListP(
            storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT), .expression = strNewFmt("^%s\\.(ok|error)$", strPtr(walSegment)));

        if (fileList != NULL && strLstSize(fileList) > 0)
        {
            // If more than one status file was found then assert - this could be a bug in the async process
            if (strLstSize(fileList) != 1)
            {
                THROW(
                    AssertError, "multiple status files found in '%s' for WAL segment '%s'",
                    strPtr(storagePathNP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT))), strPtr(walSegment));
            }

            // Get the status file content
            const String *statusFile = strLstGet(fileList, 0);

            String *content = strNewBuf(
                storageGetNP(storageNewReadNP(storageSpool(), strNewFmt("%s/%s", STORAGE_SPOOL_ARCHIVE_OUT, strPtr(statusFile)))));

            // Get the code and message if the file has content
            int code = 0;
            const String *message = NULL;

            if (strSize(content) != 0)
            {
                // Find the line feed after the error code -- should be the first one
                const char *linefeedPtr = strchr(strPtr(content), '\n');

                // Error if linefeed not found
                if (linefeedPtr == NULL)
                    THROW(FormatError, "%s content must have at least two lines", strPtr(statusFile));

                // Error if message is zero-length
                if (strlen(linefeedPtr + 1) == 0)
                    THROW(FormatError, "%s message must be > 0", strPtr(statusFile));

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
                    THROW(AssertError, "status file '%s' has no content", strPtr(statusFile));

                // Throw error using the code passed in the file
                THROW_CODE(code, strPtr(message));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}
