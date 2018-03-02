/***********************************************************************************************************************************
Configuration Load
***********************************************************************************************************************************/
#include <string.h>
#include <sys/stat.h>

#include "common/memContext.h"
#include "common/log.h"
#include "config/config.h"
#include "config/load.h"
#include "config/parse.h"

/***********************************************************************************************************************************
Load the configuration
***********************************************************************************************************************************/
void
cfgLoad(int argListSize, const char *argList[])
{
    cfgLoadParam(argListSize, argList, NULL);
}

void
cfgLoadParam(int argListSize, const char *argList[], String *exe)
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
            for (int optionIdx = 0; optionIdx <= cfgOptionIndexTotal(cfgOptPgHost); optionIdx++)
            {
                if (cfgOptionTest(cfgOptPgHost + optionIdx) && cfgOptionSource(cfgOptPgHostCmd + optionIdx) == cfgSourceDefault)
                    cfgOptionDefaultSet(cfgOptPgHostCmd + optionIdx, varNewStr(cfgExe()));
            }
        }

        // Neutralize the umask to make the repository file/path modes more consistent
        if (cfgOptionValid(cfgOptNeutralUmask) && cfgOptionBool(cfgOptNeutralUmask))
        {
            umask(0000);
        }

        // Protocol timeout should be greater than db timeout
        if (cfgOptionTest(cfgOptDbTimeout) && cfgOptionTest(cfgOptProtocolTimeout) &&
            cfgOptionDbl(cfgOptProtocolTimeout) <= cfgOptionDbl(cfgOptDbTimeout))
        {
            // If protocol-timeout is default then increase it to be greater than db-timeout
            if (cfgOptionSource(cfgOptProtocolTimeout) == cfgSourceDefault)
            {
                cfgOptionSet(cfgOptProtocolTimeout, cfgSourceDefault, varNewDbl(cfgOptionDbl(cfgOptDbTimeout) + 30));
            }
            else
            {
                THROW(OptionInvalidValueError,
                    "'%f' is not valid for '%s' option\nHINT '%s' option (%f) should be greater than '%s' option (%f).",
                    cfgOptionDbl(cfgOptProtocolTimeout), cfgOptionName(cfgOptProtocolTimeout),
                    cfgOptionName(cfgOptProtocolTimeout), cfgOptionDbl(cfgOptProtocolTimeout), cfgOptionName(cfgOptDbTimeout),
                    cfgOptionDbl(cfgOptDbTimeout));
            }
        }

        // Make sure that repo and pg host settings are not both set - cannot both be remote
        bool pgHostFound = false;
        for (int optionIdx = 0; optionIdx <= cfgOptionIndexTotal(cfgOptPgHost); optionIdx++)
        {
            if (cfgOptionTest(cfgOptPgHost + optionIdx))
            {
                pgHostFound = true;
                break;
            }
        }

        // If a pg-host was found, see if a repo-host is configured
        if (pgHostFound == true)
        {
            for (int optionIdx = 0; optionIdx <= cfgOptionIndexTotal(cfgOptRepoHost); optionIdx++)
            {
                if (cfgOptionTest(cfgOptRepoHost + optionIdx))
                {
                    THROW(ConfigError, "pg and repo hosts cannot both be configured as remote");
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();
}
