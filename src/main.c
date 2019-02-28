/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command/archive/get/get.h"
#include "command/archive/push/push.h"
#include "command/command.h"
#include "command/help/help.h"
#include "command/info/info.h"
#include "command/local/local.h"
#include "command/remote/remote.h"
#include "common/debug.h"
#include "common/error.h"
#include "common/exit.h"
#include "config/config.h"
#include "config/load.h"
#include "postgres/interface.h"
#include "perl/exec.h"
#include "version.h"

int
main(int argListSize, const char *argList[])
{
#ifdef WITH_BACKTRACE
    stackTraceInit(argList[0]);
#endif

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INT, argListSize);
        FUNCTION_LOG_PARAM(CHARPY, argList);
    FUNCTION_LOG_END();

    // Initialize command with the start time
    cmdInit();

    volatile bool result = 0;
    volatile bool error = false;

    // Initialize exit handler
    exitInit();

    TRY_BEGIN()
    {
        // Load the configuration
        // -------------------------------------------------------------------------------------------------------------------------
        cfgLoad((unsigned int)argListSize, argList);

        // Display help
        // -------------------------------------------------------------------------------------------------------------------------
        if (cfgCommandHelp())
        {
            cmdHelp();
        }

        // Display version
        // -------------------------------------------------------------------------------------------------------------------------
        else if (cfgCommand() == cfgCmdVersion)
        {
            printf(PROJECT_NAME " " PROJECT_VERSION "\n");
            fflush(stdout);
        }

        // Local command.  Currently only implements a subset.
        // -------------------------------------------------------------------------------------------------------------------------
        else if (cfgCommand() == cfgCmdLocal && strEqZ(cfgOptionStr(cfgOptCommand), cfgCommandName(cfgCmdArchiveGetAsync)))
        {
            cmdLocal(STDIN_FILENO, STDOUT_FILENO);
        }

        // Remote command.  Currently only implements a subset.
        // -------------------------------------------------------------------------------------------------------------------------
        else if (cfgCommand() == cfgCmdRemote &&
                 (strEqZ(cfgOptionStr(cfgOptCommand), cfgCommandName(cfgCmdArchiveGet)) ||
                  strEqZ(cfgOptionStr(cfgOptCommand), cfgCommandName(cfgCmdArchiveGetAsync)) ||
                  strEqZ(cfgOptionStr(cfgOptCommand), cfgCommandName(cfgCmdInfo))))
        {
            cmdRemote(STDIN_FILENO, STDOUT_FILENO);
        }

        // Archive get command
        // -------------------------------------------------------------------------------------------------------------------------
        else if (cfgCommand() == cfgCmdArchiveGet)
        {
            result = cmdArchiveGet();
        }

        // Archive get async command
        // -------------------------------------------------------------------------------------------------------------------------
        else if (cfgCommand() == cfgCmdArchiveGetAsync)
        {
            cmdArchiveGetAsync();
        }

        // Archive push command.  Currently only implements local operations of async archive push.
        // -------------------------------------------------------------------------------------------------------------------------
        else if (cfgCommand() == cfgCmdArchivePush && cfgOptionBool(cfgOptArchiveAsync))
        {
            cmdArchivePush();
        }

        // Backup command.  Still executed in Perl but this implements running expire after backup.
        // -------------------------------------------------------------------------------------------------------------------------
        else if (cfgCommand() == cfgCmdBackup)
        {
#ifdef DEBUG
            // Check pg_control during testing so errors are more obvious.  Otherwise errors only happen in archive-get/archive-push
            // and end up in the PostgreSQL log which is not output in CI.  This can be removed once backup is written in C.
            if (cfgOptionBool(cfgOptOnline) && !cfgOptionBool(cfgOptBackupStandby) && !cfgOptionTest(cfgOptPgHost))
                pgControlFromFile(cfgOptionStr(cfgOptPgPath));
#endif

            // Run backup
            perlExec();

            // Switch to expire command
            cmdEnd(0, NULL);
            cfgCommandSet(cfgCmdExpire);
            cmdBegin(false);

            // Run expire
            perlExec();
        }

        // Info command
        // -------------------------------------------------------------------------------------------------------------------------
        else if (cfgCommand() == cfgCmdInfo)
        {
            cmdInfo();
        }

        // Execute Perl for commands not implemented in C
        // -------------------------------------------------------------------------------------------------------------------------
        else
        {
            result = perlExec();
        }
    }
    CATCH_ANY()
    {
        error = true;
    }
    TRY_END();

    FUNCTION_LOG_RETURN(INT, exitSafe(result, error, 0));
}
