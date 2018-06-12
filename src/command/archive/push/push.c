/***********************************************************************************************************************************
Archive Push Command
***********************************************************************************************************************************/
#include <unistd.h>

#include "command/archive/common.h"
#include "command/command.h"
#include "common/debug.h"
#include "common/fork.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/wait.h"
#include "config/config.h"
#include "config/load.h"
#include "perl/exec.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Push a WAL segment to the repository
***********************************************************************************************************************************/
void
cmdArchivePush()
{
    FUNCTION_DEBUG_VOID(logLevelDebug);

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
                pushed = archiveAsyncStatus(archiveModePush, walSegment, confessOnError);

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
                    THROW_FMT(
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

    FUNCTION_DEBUG_RESULT_VOID();
}
