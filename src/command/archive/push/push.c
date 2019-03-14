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
#include "config/exec.h"
#include "perl/exec.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Push a WAL segment to the repository
***********************************************************************************************************************************/
void
cmdArchivePush(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

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

            // Loop and wait for the WAL segment to be pushed
            Wait *wait = waitNew((TimeMSec)(cfgOptionDbl(cfgOptArchiveTimeout) * MSEC_PER_SEC));

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
                    // The async process should not output on the console at all
                    KeyValue *optionReplace = kvNew();

                    kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptLogLevelConsole))), varNewStrZ("off"));
                    kvPut(optionReplace, varNewStr(strNew(cfgOptionName(cfgOptLogLevelStderr))), varNewStrZ("off"));

                    // Generate command options
                    StringList *commandExec = cfgExecParam(cfgCmdArchivePushAsync, optionReplace);
                    strLstInsert(commandExec, 0, cfgExe());
                    strLstAdd(commandExec, strLstGet(commandParam, 0));

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

                // Now that the async process has been launched, confess any errors that are found
                confessOnError = true;
            }
            while (!pushed && waitMore(wait));

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
        else
            perlExec();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
