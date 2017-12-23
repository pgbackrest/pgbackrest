/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "common/error.h"
#include "config/config.h"
#include "config/parse.h"
#include "perl/exec.h"
#include "version.h"

int main(int argListSize, const char *argList[])
{
    TRY_BEGIN()
    {
        // Parse command line
        configParse(argListSize, argList);

        // Display version
        if (!cfgCommandHelp() && cfgCommand() == cfgCmdVersion)
        {
            printf(PGBACKREST_NAME " " PGBACKREST_VERSION "\n");
            fflush(stdout);
            exit(0);
        }

        // Execute Perl for commands not implemented in C
        perlExec(perlCommand());
    }
    CATCH_ANY()
    {
        fprintf(stderr, "ERROR [%03d]: %s\n", errorCode(), errorMessage());
        fflush(stderr);
        exit(errorCode());
    }
    TRY_END();
}
