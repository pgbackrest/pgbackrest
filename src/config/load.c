/***********************************************************************************************************************************
Configuration Load
***********************************************************************************************************************************/
#include <string.h>

#include "common/memContext.h"
#include "common/log.h"
#include "config/config.h"
#include "config/load.h"
#include "config/parse.h"

/***********************************************************************************************************************************
Load the configuration
***********************************************************************************************************************************/
void
cfgLoad(unsigned int argListSize, const char *argList[])
{
    cfgLoadParam(argListSize, argList, NULL);
}

void
cfgLoadParam(unsigned int argListSize, const char *argList[], String *exe)
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Parse config from command line and config file
        configParse(argListSize, argList);

        // Initialize logging
        if (cfgCommand() != cfgCmdLocal && cfgCommand() != cfgCmdRemote)
        {
            LogLevel logLevelConsole = logLevelOff;
            LogLevel logLevelStdErr = logLevelOff;
            bool logTimestamp = true;

            if (cfgOptionValid(cfgOptLogLevelConsole))
                logLevelConsole = logLevelEnum(strPtr(cfgOptionStr(cfgOptLogLevelConsole)));

            if (cfgOptionValid(cfgOptLogLevelStderr))
                logLevelStdErr = logLevelEnum(strPtr(cfgOptionStr(cfgOptLogLevelStderr)));

            if (cfgOptionValid(cfgOptLogTimestamp))
                logTimestamp = cfgOptionBool(cfgOptLogTimestamp);

            logInit(logLevelConsole, logLevelStdErr, logTimestamp);
        }

        // If an exe was passed in the use that
        if (exe != NULL)
            cfgExeSet(exe);

        // Set default for repo-host-cmd
        if (cfgOptionValid(cfgOptRepoHost) && cfgOptionTest(cfgOptRepoHost) &&
            cfgOptionSource(cfgOptRepoHostCmd) == cfgSourceDefault)
        {
            cfgOptionDefaultSet(cfgOptRepoHostCmd, varNewStr(cfgExe()));
        }

        // Set default for pg-host-cmd
        if (cfgOptionValid(cfgOptPgHostCmd))
        {
            for (unsigned int optionIdx = 0; optionIdx < cfgOptionIndexTotal(cfgOptPgHost); optionIdx++)
            {
                if (cfgOptionTest(cfgOptPgHost + optionIdx) && cfgOptionSource(cfgOptPgHostCmd + optionIdx) == cfgSourceDefault)
                    cfgOptionDefaultSet(cfgOptPgHostCmd + optionIdx, varNewStr(cfgExe()));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();
}
