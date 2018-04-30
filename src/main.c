/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "command/archive/get/get.h"
#include "command/archive/push/push.h"
#include "command/help/help.h"
#include "command/command.h"
#include "common/error.h"
#include "common/exit.h"
#include "config/config.h"
#include "config/load.h"
#include "perl/exec.h"
#include "version.h"

int
main(int argListSize, const char *argList[])
{
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
            printf(PGBACKREST_NAME " " PGBACKREST_VERSION "\n");
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

    return exitSafe(result, error, 0);
}
