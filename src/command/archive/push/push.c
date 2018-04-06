/***********************************************************************************************************************************
Archive Push Command
***********************************************************************************************************************************/
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common/error.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/typec.h"
#include "common/wait.h"
#include "config/config.h"
#include "perl/exec.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Check for ok/error status files in the spool out directory
***********************************************************************************************************************************/
static bool
walStatus(const String *walSegment, bool confessOnError)
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
                storageGetNP(storageOpenReadNP(storageSpool(), strNewFmt("%s/%s", STORAGE_SPOOL_ARCHIVE_OUT, strPtr(statusFile)))));

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

/***********************************************************************************************************************************
Push a WAL segment to the repository
***********************************************************************************************************************************/
void
cmdArchivePush()
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Make sure there is a parameter to retrieve the WAL segment from
        const StringList *commandParam = cfgCommandParam();

        if (strLstSize(commandParam) != 1)
            THROW(ParamRequiredError, "WAL segment to push required");

        // Get the segment name
        String *walSegment = strBase(strLstGet(commandParam, 0));

        if (cfgOptionBool(cfgOptArchiveAsync))
        {
            bool pushed = false;                                        // Has the WAL segment been pushed yet?
            bool confessOnError = false;                                // Should we confess errors?

            // Loop and wait for the WAL segment to be pushed
            Wait *wait = waitNew(cfgOptionDbl(cfgOptArchiveTimeout));

            do
            {
                // Check if the WAL segment has been pushed.  Errors will not be confessed on the first try to allow the async
                // process a chance to fix them.
                pushed = walStatus(walSegment, confessOnError);

                // If the WAL segment has not already been pushed then start the async process to push it
                if (!pushed)
                {
                    // Async process is currently implemented in Perl
                    int processId = 0;

                    if ((processId = fork()) == 0)
                    {
                        // Only want to see warnings and errors on the console from async process
                        cfgOptionSet(cfgOptLogLevelConsole, cfgSourceParam, varNewStrZ("warn"));

                        // Execute async process
                        perlExec();
                    }
                    // Wait for async process to exit (this should happen quickly) and report any errors
                    else
                    {
                        int processStatus;

                        if (waitpid(processId, &processStatus, 0) != processId)             // {uncoverable - fork() does not fail}
                            THROW_SYS_ERROR(AssertError, "unable to find perl child process");  // {uncoverable+}

                        if (WEXITSTATUS(processStatus) != 0)
                            THROW(AssertError, "perl exited with error %d", WEXITSTATUS(processStatus));
                    }
                }

                // Now that the async process has been launched, confess any errors that are found
                confessOnError = true;
            }
            while (!pushed && waitMore(wait));

            waitFree(wait);

            // If the WAL segment was not pushed then error
            if (!pushed)
            {
                THROW(
                    ArchiveTimeoutError, "unable to push WAL segment '%s' asynchronously after %lg second(s)", strPtr(walSegment),
                    cfgOptionDbl(cfgOptArchiveTimeout));
            }

            // Log success
            LOG_INFO("pushed WAL segment %s asynchronously", strPtr(walSegment));
        }
        else
            THROW(AssertError, "archive-push in C does not support synchronous mode");
    }
    MEM_CONTEXT_TEMP_END();
}
