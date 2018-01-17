/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "common/error.h"
#include "common/exit.h"
#include "config/config.h"
#include "config/load.h"
#include "perl/exec.h"
#include "version.h"

int main(int argListSize, const char *argList[])
{
    TRY_BEGIN()
    {
        // Load the configuration
        // -------------------------------------------------------------------------------------------------------------------------
        cfgLoad(argListSize, argList);

        // Display version
        // -------------------------------------------------------------------------------------------------------------------------
        if (!cfgCommandHelp() && cfgCommand() == cfgCmdVersion)
        {
            printf(PGBACKREST_NAME " " PGBACKREST_VERSION "\n");
            fflush(stdout);
            exit(0);
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
