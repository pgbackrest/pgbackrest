/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>

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
    TRY_BEGIN()
    {
        // Load the configuration
        // -------------------------------------------------------------------------------------------------------------------------
        cfgLoad(argListSize, argList);

        // Display help
        // -------------------------------------------------------------------------------------------------------------------------
        if (cfgCommandHelp())
        {
            cmdHelp();
            exit(0);
        }

        // Display version
        // -------------------------------------------------------------------------------------------------------------------------
        if (cfgCommand() == cfgCmdVersion)
        {
            printf(PGBACKREST_NAME " " PGBACKREST_VERSION "\n");
            fflush(stdout);
            exit(0);
        }

        // Archive push command.  Currently only implements local operations of async archive push.
        // -------------------------------------------------------------------------------------------------------------------------
        if (cfgCommand() == cfgCmdArchivePush && cfgOptionBool(cfgOptArchiveAsync))
        {
            cmdBegin();
            cmdArchivePush();
            exit(exitSafe(false));
        }

        // Execute Perl for commands not implemented in C
        // -------------------------------------------------------------------------------------------------------------------------
        perlExec(perlCommand());
    }
    CATCH_ANY()
    {
        exit(exitSafe(true));
    }
    TRY_END();

    exit(exitSafe(false));
}
