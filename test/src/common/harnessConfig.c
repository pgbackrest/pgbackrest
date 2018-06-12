/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "common/harnessDebug.h"
#include "common/logTest.h"

#include "config/load.h"
#include "config/parse.h"

/***********************************************************************************************************************************
Load a test configuration without any side effects

Log testing requires that log levels be set in a certain way but calls to cfgLoad() will reset that.  Also there's no need to open
log files, acquire locks, etc.
***********************************************************************************************************************************/
void
harnessCfgLoad(unsigned int argListSize, const char *argList[])
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, argListSize);
        FUNCTION_HARNESS_PARAM(CHARPY, argList);
    FUNCTION_HARNESS_END();

    configParse(argListSize, argList);
    logInit(logLevelInfo, logLevelOff, logLevelDebug, false);
    cfgLoadUpdateOption();

    FUNCTION_HARNESS_RESULT_VOID();
}
