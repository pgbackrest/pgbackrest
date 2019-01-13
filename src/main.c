/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "command/archive/get/get.h"
#include "command/archive/push/push.h"
#include "command/help/help.h"
#include "command/info/info.h"
#include "command/command.h"
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

    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(INT, argListSize);
        FUNCTION_DEBUG_PARAM(CHARPY, argList);
    FUNCTION_DEBUG_END();

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

        // Archive get command
        // -------------------------------------------------------------------------------------------------------------------------
        else if (cfgCommand() == cfgCmdArchiveGet)
        {
            result = cmdArchiveGet();
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

    FUNCTION_DEBUG_RESULT(INT, exitSafe(result, error, 0));
}
