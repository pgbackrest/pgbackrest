/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/harnessConfig.h"
#include "common/harnessDebug.h"
#include "common/harnessLog.h"
#include "common/harnessTest.h"

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
    if (cfgDefOptionValid(commandId, cfgDefOptLogPath))
        strLstInsert(argList, 0, strNewFmt("--" CFGOPT_LOG_PATH "=%s", testDataPath()));

    // Set lock path if valid
    if (cfgDefOptionValid(commandId, cfgDefOptLockPath))
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
hrnCfgArgRaw(StringList *argList, ConfigOption optionId, const String *value)
{
    hrnCfgArgKeyRawZ(argList, optionId, 1, strZ(value));
}

void
hrnCfgArgKeyRaw(StringList *argList, ConfigOption optionId, unsigned optionKey, const String *value)
{
    hrnCfgArgKeyRawZ(argList, optionId, optionKey, strZ(value));
}

void
hrnCfgArgRawFmt(StringList *argList, ConfigOption optionId, const char *format, ...)
{
    char buffer[256];

    va_list argument;
    va_start(argument, format);
    (size_t)vsnprintf(buffer, sizeof(buffer) - 1, format, argument);
    va_end(argument);

    hrnCfgArgKeyRawZ(argList, optionId, 1, buffer);
}

void
hrnCfgArgKeyRawFmt(StringList *argList, ConfigOption optionId, unsigned optionKey, const char *format, ...)
{
    char buffer[256];

    va_list argument;
    va_start(argument, format);
    (size_t)vsnprintf(buffer, sizeof(buffer) - 1, format, argument);
    va_end(argument);

    hrnCfgArgKeyRawZ(argList, optionId, optionKey, buffer);
}

void
hrnCfgArgRawZ(StringList *argList, ConfigOption optionId, const char *value)
{
    hrnCfgArgKeyRawZ(argList, optionId, 1, value);
}

void
hrnCfgArgKeyRawZ(StringList *argList, ConfigOption optionId, unsigned optionKey, const char *value)
{
    strLstAdd(argList, strNewFmt("--%s=%s", cfgOptionName(optionId + optionKey - 1), value));
}

void
hrnCfgArgRawBool(StringList *argList, ConfigOption optionId, bool value)
{
    hrnCfgArgKeyRawBool(argList, optionId, 1, value);
}

void
hrnCfgArgKeyRawBool(StringList *argList, ConfigOption optionId, unsigned optionKey, bool value)
{
    strLstAdd(argList, strNewFmt("--%s%s", value ? "" : "no-", cfgOptionName(optionId + optionKey - 1)));
}

void
hrnCfgArgRawReset(StringList *argList, ConfigOption optionId)
{
    hrnCfgArgKeyRawReset(argList, optionId, 1);
}

void
hrnCfgArgKeyRawReset(StringList *argList, ConfigOption optionId, unsigned optionKey)
{
    strLstAdd(argList, strNewFmt("--reset-%s", cfgOptionName(optionId + optionKey - 1)));
}

/**********************************************************************************************************************************/
void
hrnCfgEnvRaw(ConfigOption optionId, const String *value)
{
    hrnCfgEnvIdRawZ(optionId, 1, strZ(value));
}

void
hrnCfgEnvIdRaw(ConfigOption optionId, unsigned optionKey, const String *value)
{
    hrnCfgEnvIdRawZ(optionId, optionKey, strZ(value));
}

void
hrnCfgEnvRawZ(ConfigOption optionId, const char *value)
{
    hrnCfgEnvIdRawZ(optionId, 1, value);
}

void
hrnCfgEnvIdRawZ(ConfigOption optionId, unsigned optionKey, const char *value)
{
    setenv(strZ(strNewFmt(HRN_PGBACKREST_ENV "%s", cfgOptionName(optionId + optionKey - 1))), value, true);
}

void
hrnCfgEnvRemoveRaw(ConfigOption optionId)
{
    hrnCfgEnvIdRemoveRaw(optionId, 1);
}

void
hrnCfgEnvIdRemoveRaw(ConfigOption optionId, unsigned optionKey)
{
    unsetenv(strZ(strNewFmt(HRN_PGBACKREST_ENV "%s", cfgOptionName(optionId + optionKey - 1))));
}
