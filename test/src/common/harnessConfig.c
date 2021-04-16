/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/harnessConfig.h"
#include "common/harnessDebug.h"
#include "common/harnessLog.h"
#include "common/harnessTest.h"

#include "config/config.intern.h"
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

    // Log parameters
    hrnTestLogPrefix(__LINE__, true);

    printf("config load:");

    for (unsigned int argIdx = 0; argIdx < argListSize; argIdx++)
        printf(" %s", argList[argIdx]);

    printf("\n");

    fflush(stdout);

    // Parse config
    configParse(storageLocal(), argListSize, argList, false);

    // Set dry-run mode for storage and logging
    harnessLogDryRunSet(cfgOptionValid(cfgOptDryRun) && cfgOptionBool(cfgOptDryRun));
    storageHelperDryRunInit(cfgOptionValid(cfgOptDryRun) && cfgOptionBool(cfgOptDryRun));

    // Apply special option rules
    cfgLoadUpdateOption();

    // Use a static exec-id for testing if it is not set explicitly
    if (cfgOptionValid(cfgOptExecId) && !cfgOptionTest(cfgOptExecId))
        cfgOptionSet(cfgOptExecId, cfgSourceParam, VARSTRDEF("1-test"));

    FUNCTION_HARNESS_RETURN_VOID();
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

    // Set job retry to 0 if it is valid
    if (cfgParseOptionValid(commandId, commandRoleId, cfgOptJobRetry))
        strLstInsert(argList, 0, STRDEF("--" CFGOPT_JOB_RETRY "=0"));

    // Set log path if valid
    if (cfgParseOptionValid(commandId, commandRoleId, cfgOptLogPath))
        strLstInsert(argList, 0, strNewFmt("--" CFGOPT_LOG_PATH "=%s", testDataPath()));

    // Set lock path if valid
    if (cfgParseOptionValid(commandId, commandRoleId, cfgOptLockPath))
        strLstInsert(argList, 0, strNewFmt("--" CFGOPT_LOCK_PATH "=%s/lock", testDataPath()));

    // Insert the command so it does not interfere with parameters
    strLstInsert(argList, 0, cfgCommandRoleNameParam(commandId, commandRoleId, COLON_STR));

    // Insert the project exe
    strLstInsert(argList, 0, STRDEF(testProjectExe()));

    harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

    FUNCTION_HARNESS_RETURN_VOID();
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

    FUNCTION_HARNESS_RETURN_VOID();
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
    vsnprintf(buffer, sizeof(buffer) - 1, format, argument);
    va_end(argument);

    hrnCfgArgKeyRawZ(argList, optionId, 1, buffer);
}

void
hrnCfgArgKeyRawFmt(StringList *argList, ConfigOption optionId, unsigned optionKey, const char *format, ...)
{
    char buffer[256];

    va_list argument;
    va_start(argument, format);
    vsnprintf(buffer, sizeof(buffer) - 1, format, argument);
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
    strLstAdd(argList, strNewFmt("--%s=%s", cfgParseOptionKeyIdxName(optionId, optionKey - 1), value));
}

void
hrnCfgArgRawStrId(StringList *argList, ConfigOption optionId, StringId value)
{
    hrnCfgArgKeyRawStrId(argList, optionId, 1, value);
}

void
hrnCfgArgKeyRawStrId(StringList *argList, ConfigOption optionId, unsigned optionKey, StringId value)
{
    char buffer[STRID_MAX + 1];
    strIdToZ(value, buffer);

    hrnCfgArgKeyRawZ(argList, optionId, optionKey, buffer);
}

void
hrnCfgArgRawBool(StringList *argList, ConfigOption optionId, bool value)
{
    hrnCfgArgKeyRawBool(argList, optionId, 1, value);
}

void
hrnCfgArgKeyRawBool(StringList *argList, ConfigOption optionId, unsigned optionKey, bool value)
{
    strLstAdd(argList, strNewFmt("--%s%s", value ? "" : "no-", cfgParseOptionKeyIdxName(optionId, optionKey - 1)));
}

void
hrnCfgArgRawNegate(StringList *argList, ConfigOption optionId)
{
    hrnCfgArgKeyRawNegate(argList, optionId, 1);
}

void
hrnCfgArgKeyRawNegate(StringList *argList, ConfigOption optionId, unsigned optionKey)
{
    strLstAdd(argList, strNewFmt("--no-%s", cfgParseOptionKeyIdxName(optionId, optionKey - 1)));
}

void
hrnCfgArgRawReset(StringList *argList, ConfigOption optionId)
{
    hrnCfgArgKeyRawReset(argList, optionId, 1);
}

void
hrnCfgArgKeyRawReset(StringList *argList, ConfigOption optionId, unsigned optionKey)
{
    strLstAdd(argList, strNewFmt("--reset-%s", cfgParseOptionKeyIdxName(optionId, optionKey - 1)));
}

/**********************************************************************************************************************************/
__attribute__((always_inline)) static inline const char *
hrnCfgEnvName(ConfigOption optionId, unsigned optionKey)
{
    return strZ(
        strReplaceChr(strUpper(strNewFmt(HRN_PGBACKREST_ENV "%s", cfgParseOptionKeyIdxName(optionId, optionKey - 1))), '-', '_'));
}

void
hrnCfgEnvRaw(ConfigOption optionId, const String *value)
{
    hrnCfgEnvKeyRawZ(optionId, 1, strZ(value));
}

void
hrnCfgEnvKeyRaw(ConfigOption optionId, unsigned optionKey, const String *value)
{
    hrnCfgEnvKeyRawZ(optionId, optionKey, strZ(value));
}

void
hrnCfgEnvRawZ(ConfigOption optionId, const char *value)
{
    hrnCfgEnvKeyRawZ(optionId, 1, value);
}

void
hrnCfgEnvKeyRawZ(ConfigOption optionId, unsigned optionKey, const char *value)
{
    setenv(hrnCfgEnvName(optionId, optionKey), value, true);
}

void
hrnCfgEnvRemoveRaw(ConfigOption optionId)
{
    hrnCfgEnvKeyRemoveRaw(optionId, 1);
}

void
hrnCfgEnvKeyRemoveRaw(ConfigOption optionId, unsigned optionKey)
{
    unsetenv(hrnCfgEnvName(optionId, optionKey));
}
