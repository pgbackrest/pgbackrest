/***********************************************************************************************************************************
Archive Push Command
***********************************************************************************************************************************/
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "command/command.h"
#include "common/fork.h"
#include "common/error.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/wait.h"
#include "config/config.h"
#include "config/load.h"
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
            bool forked = false;                                        // Has the async process been forked yet?
            bool confessOnError = false;                                // Should we confess errors?
            bool server = false;                                        // Is this the async server process?

            // Loop and wait for the WAL segment to be pushed
            Wait *wait = waitNew(cfgOptionDbl(cfgOptArchiveTimeout));

            do
            {
                // Check if the WAL segment has been pushed.  Errors will not be confessed on the first try to allow the async
                // process a chance to fix them.
                pushed = walStatus(walSegment, confessOnError);

                // If the WAL segment has not already been pushed then start the async process to push it.  There's no point in
                // forking the async process off more than once so track that as well.  Use an archive lock to prevent more than
                // one async process being launched.
                if (!pushed && !forked &&
                    lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 0, false))
                {
                    // Fork off the async process
                    if (fork() == 0)
                    {
                        // This is the server process
                        server = true;

                        // The async process should not output on the console at all
                        cfgOptionSet(cfgOptLogLevelConsole, cfgSourceParam, varNewStrZ("off"));
                        cfgOptionSet(cfgOptLogLevelStderr, cfgSourceParam, varNewStrZ("off"));
                        cfgLoadLogSetting();

                        // Open the log file
                        logFileSet(
                            strPtr(strNewFmt("%s/%s-%s-async.log", strPtr(cfgOptionStr(cfgOptLogPath)),
                            strPtr(cfgOptionStr(cfgOptStanza)), cfgCommandName(cfgCommand()))));

                        // Log command info since we are starting a new log
                        cmdBegin(true);

                        // Detach from parent process
                        forkDetach();

                        // Execute async process and catch exceptions
                        TRY_BEGIN()
                        {
                            perlExec();
                        }
                        CATCH_ANY()
                        {
                            RETHROW();
                        }
                        FINALLY()
                        {
                            // Release the lock (mostly here for testing since it would be freed in exitSafe() anyway)
                            lockRelease(true);
                        }
                        TRY_END();
                    }
                    // Else mark async process as forked
                    else
                    {
                        lockClear(true);
                        forked = true;
                    }
                }

                // Now that the async process has been launched, confess any errors that are found
                confessOnError = true;
            }
            while (!server && !pushed && waitMore(wait));

            // The aysnc server does not give notifications
            if (!server)
            {
                // If the WAL segment was not pushed then error
                if (!pushed)
                {
                    THROW(
                        ArchiveTimeoutError, "unable to push WAL segment '%s' asynchronously after %lg second(s)",
                        strPtr(walSegment), cfgOptionDbl(cfgOptArchiveTimeout));
                }

                // Log success
                LOG_INFO("pushed WAL segment %s asynchronously", strPtr(walSegment));
            }
        }
        else
            THROW(AssertError, "archive-push in C does not support synchronous mode");
    }
    MEM_CONTEXT_TEMP_END();
}
