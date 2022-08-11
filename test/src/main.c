/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>

#include "command/command.h"
#include "command/exit.h"
#include "command/help/help.h"
#include "command/test/test.h"
#include "common/debug.h"
#include "common/error.h"
#include "common/log.h"
#include "common/macro.h"
#include "common/memContext.h"
#include "common/stat.h"
#include "config/load.h"
#include "config/parse.h"
#include "storage/posix/storage.h"
#include "version.h"

/***********************************************************************************************************************************
Include automatically generated help data
***********************************************************************************************************************************/
#include "command/help/help.auto.c.inc"

int
main(int argListSize, const char *argList[])
{
    // Set stack trace and mem context error cleanup handlers
    static const ErrorHandlerFunction errorHandlerList[] = {stackTraceClean, memContextClean};
    errorHandlerSet(errorHandlerList, LENGTH_OF(errorHandlerList));

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INT, argListSize);
        FUNCTION_LOG_PARAM(CHARPY, argList);
    FUNCTION_LOG_END();

    // Initialize command with the start time
    cmdInit();

    // Initialize statistics collector
    statInit();

    // Initialize exit handler
    exitInit();

    // Process commands
    volatile int result = 0;
    volatile bool error = false;

    TRY_BEGIN()
    {
        // Load the configuration
        // -------------------------------------------------------------------------------------------------------------------------
        cfgLoad((unsigned int)argListSize, argList);

        // Display help
        // -------------------------------------------------------------------------------------------------------------------------
        if (cfgCommandHelp())
        {
            cmdHelp(BUF(helpData, sizeof(helpData)));
        }
        else
        {
            switch (cfgCommand())
            {
                // Test
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdTest:
                {
                    cmdTest(
                        cfgOptionStr(cfgOptRepoPath), cfgOptionStr(cfgOptTestPath), cfgOptionStr(cfgOptVm),
                        cfgOptionUInt(cfgOptVmId), cfgCommandParam(), cfgOptionTest(cfgOptTest) ? cfgOptionUInt(cfgOptTest) : 0,
                        cfgOptionUInt64(cfgOptScale), logLevelEnum(cfgOptionStrId(cfgOptLogLevelTest)),
                        cfgOptionBool(cfgOptLogTimestamp), cfgOptionStrNull(cfgOptTz), cfgOptionBool(cfgOptCoverage),
                        cfgOptionBool(cfgOptProfile), cfgOptionBool(cfgOptOptimize));

                    break;
                }

                // Display version
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdVersion:
                    printf(PROJECT_NAME " Test " PROJECT_VERSION "\n");
                    fflush(stdout);
                    break;

                // Error on commands that should have already been handled
                // -----------------------------------------------------------------------------------------------------------------
                case cfgCmdHelp:
                case cfgCmdNone:
                case cfgCmdNoop:
                    THROW_FMT(AssertError, "'%s' command should have been handled", cfgCommandName());
                    break;
            }
        }
    }
    CATCH_FATAL()
    {
        error = true;
        result = exitSafe(result, true, 0);
    }
    TRY_END();

    FUNCTION_LOG_RETURN(INT, error ? result : exitSafe(result, false, 0));
}
