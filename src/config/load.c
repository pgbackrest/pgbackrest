/***********************************************************************************************************************************
Configuration Load
***********************************************************************************************************************************/
#include <string.h>

#include "common/memContext.h"
#include "common/log.h"
#include "config/config.h"
#include "config/parse.h"

/***********************************************************************************************************************************
Load the configuration
***********************************************************************************************************************************/
void
cfgLoad(int argListSize, const char *argList[])
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Parse config from command line and config file
        configParse(argListSize, argList);

        // Initialize logging
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
    MEM_CONTEXT_TEMP_END();
}
