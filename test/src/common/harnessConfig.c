/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/harnessDebug.h"
#include "common/harnessLog.h"
#include "common/harnessTest.h"

#include "config/config.intern.h"
#include "config/define.h"
#include "config/load.h"
#include "config/parse.h"
#include "storage/helper.h"
#include "version.h"

/**********************************************************************************************************************************/
void
harnessCfgLoadRaw(unsigned int argListSize, const char *argList[])
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, argListSize);
        FUNCTION_HARNESS_PARAM(CHARPY, argList);
    FUNCTION_HARNESS_END();

    // Free objects in storage helper
    storageHelperFree();

    configParse(argListSize, argList, false);
    cfgLoadUpdateOption();

    // Set dry-run mode for storage and logging
    storageHelperDryRunInit(cfgOptionValid(cfgOptDryRun) && cfgOptionBool(cfgOptDryRun));
#ifndef NO_LOG
    harnessLogDryRunSet(cfgOptionValid(cfgOptDryRun) && cfgOptionBool(cfgOptDryRun));
#endif

    FUNCTION_HARNESS_RESULT_VOID();
}

/**********************************************************************************************************************************/
void
harnessCfgLoadRole(ConfigCommand commandId, ConfigCommandRole commandRoleId, const StringList *argListParam)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(ENUM, commandId);
        FUNCTION_HARNESS_PARAM(ENUM, commandRoleId);
        FUNCTION_HARNESS_PARAM(STRING_LIST, argListParam);
    FUNCTION_HARNESS_END();

    // Make a copy of the arg list that we can modify
    StringList *argList = strLstDup(argListParam);

    // Set log path if valid
    if (cfgDefOptionValid(commandId, cfgOptLogPath))
        strLstInsert(argList, 0, strNewFmt("--" CFGOPT_LOG_PATH "=%s", testDataPath()));

    // Set lock path if valid
    if (cfgDefOptionValid(commandId, cfgOptLockPath))
        strLstInsert(argList, 0, strNewFmt("--" CFGOPT_LOCK_PATH "=%s/lock", testDataPath()));

    // Insert the command so it does not interfere with parameters
    strLstInsert(argList, 0, cfgCommandRoleNameParam(commandId, commandRoleId, COLON_STR));

    // Insert the project exe
    strLstInsert(argList, 0, STRDEF(testProjectExe()));

    harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

    FUNCTION_HARNESS_RESULT_VOID();
}

/**********************************************************************************************************************************/
void
harnessCfgLoad(ConfigCommand commandId, const StringList *argListParam)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(ENUM, commandId);
        FUNCTION_HARNESS_PARAM(STRING_LIST, argListParam);
    FUNCTION_HARNESS_END();

    harnessCfgLoadRole(commandId, cfgCmdRoleDefault, argListParam);

    FUNCTION_HARNESS_RESULT_VOID();
}

/**********************************************************************************************************************************/
void
hrnCfgArgRaw(StringList *argList, ConfigOption optionId, const char *value)
{
    hrnCfgArgIdRaw(argList, optionId, 1, value);
}

void
hrnCfgArgIdRaw(StringList *argList, ConfigOption optionId, unsigned optionIdx, const char *value)
{
    strLstAdd(argList, strNewFmt("--%s=%s", cfgOptionRawIdxName(optionId, optionIdx - 1), value));
}
