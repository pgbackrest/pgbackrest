/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "command/archive/push/push.h"
#include "command/help/help.h"
#include "common/error.h"
#include "common/exit.h"
#include "config/config.h"
#include "config/load.h"
#include "perl/exec.h"
#include "version.h"

int
main(int argListSize, const char *argList[])
{
    bool error = false;

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

        // Archive push command.  Currently only implements local operations of async archive push.
        // -------------------------------------------------------------------------------------------------------------------------
        else if (cfgCommand() == cfgCmdArchivePush && cfgOptionBool(cfgOptArchiveAsync))
        {
            cmdArchivePush();
        }

        // Execute Perl for commands not implemented in C
        // -------------------------------------------------------------------------------------------------------------------------
        else
        {
            perlExec();
        }
    }
    CATCH_ANY()
    {
        error = true;
    }
    TRY_END();

    return exitSafe(error);
}
