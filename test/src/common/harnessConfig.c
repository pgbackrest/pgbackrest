/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "common/harnessDebug.h"
#include "common/harnessLog.h"

#include "config/load.h"
#include "config/parse.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Load a test configuration without any side effects

There's no need to open log files, acquire locks, reset log levels, etc.
***********************************************************************************************************************************/
void
harnessCfgLoad(unsigned int argListSize, const char *argList[])
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, argListSize);
        FUNCTION_HARNESS_PARAM(CHARPY, argList);
    FUNCTION_HARNESS_END();

    // Free objects in storage helper
    storageHelperFree();

    configParse(argListSize, argList, false);
    cfgLoadUpdateOption();

    FUNCTION_HARNESS_RESULT_VOID();
}
