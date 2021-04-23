/***********************************************************************************************************************************
Command and Option Configuration
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/memContext.h"
#include "config/config.intern.h"
#include "config/parse.h"

/***********************************************************************************************************************************
Map command names to ids and vice versa
***********************************************************************************************************************************/
typedef struct ConfigCommandData
{
    const char *name;

    bool lockRequired:1;
    bool lockRemoteRequired:1;
    unsigned int lockType:2;

    bool logFile:1;
    unsigned int logLevelDefault:4;
} ConfigCommandData;

#define CONFIG_COMMAND_LIST(...)                                                                                                   \
    {__VA_ARGS__};

#define CONFIG_COMMAND(...)                                                                                                        \
    {__VA_ARGS__},

#define CONFIG_COMMAND_LOCK_REQUIRED(lockRequiredParam)                                                                            \
    .lockRequired = lockRequiredParam,
#define CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(lockRemoteRequiredParam)                                                               \
    .lockRemoteRequired = lockRemoteRequiredParam,
#define CONFIG_COMMAND_LOCK_TYPE(lockTypeParam)                                                                                    \
    .lockType = lockTypeParam,
#define CONFIG_COMMAND_LOG_FILE(logFileParam)                                                                                      \
    .logFile = logFileParam,
#define CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelDefaultParam)                                                                     \
    .logLevelDefault = logLevelDefaultParam,
#define CONFIG_COMMAND_NAME(nameParam)                                                                                             \
    .name = nameParam,

/***********************************************************************************************************************************
Include the automatically generated configuration data
***********************************************************************************************************************************/
#include "config/config.auto.c"

/***********************************************************************************************************************************
Data for the currently loaded configuration
***********************************************************************************************************************************/
Config *configLocal = NULL;

/**********************************************************************************************************************************/
void
cfgInit(Config *config)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, config);
    FUNCTION_TEST_END();

    ASSERT(config != NULL);

    // Free the old context
    if (configLocal != NULL)
        memContextFree(configLocal->memContext);

    // Set config and move context to top so it persists for the life of the program
    configLocal = config;
    memContextMove(configLocal->memContext, memContextTop());

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
ConfigCommand
cfgCommand(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(configLocal == NULL ? cfgCmdNone : configLocal->command);
}

ConfigCommandRole
cfgCommandRole(void)
{
    FUNCTION_TEST_VOID();
    ASSERT(configLocal != NULL);
    FUNCTION_TEST_RETURN(configLocal->commandRole);
}

void
cfgCommandSet(ConfigCommand commandId, ConfigCommandRole commandRoleId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
        FUNCTION_TEST_PARAM(ENUM, commandRoleId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(commandId <= cfgCmdNone);

    configLocal->command = commandId;
    configLocal->commandRole = commandRoleId;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
cfgCommandHelp(void)
{
    FUNCTION_TEST_VOID();
    ASSERT(configLocal != NULL);
    FUNCTION_TEST_RETURN(configLocal->help);
}

/**********************************************************************************************************************************/
ConfigCommand
cfgCommandId(const char *commandName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, commandName);
    FUNCTION_TEST_END();

    ASSERT(commandName != NULL);

    ConfigCommand commandId;

    for (commandId = 0; commandId < cfgCmdNone; commandId++)
        if (strcmp(commandName, configCommandData[commandId].name) == 0)
            break;

    FUNCTION_TEST_RETURN(commandId);
}

/**********************************************************************************************************************************/
VariantList *
cfgCommandJobRetry(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);

    // Return NULL if no retries
    unsigned int retryTotal = cfgOptionUInt(cfgOptJobRetry);

    if (retryTotal == 0)
        FUNCTION_TEST_RETURN(NULL);

    // Build retry list
    VariantList *result = varLstNew();

    for (unsigned int retryIdx = 0; retryIdx < cfgOptionUInt(cfgOptJobRetry); retryIdx++)
        varLstAdd(result, varNewUInt64(retryIdx == 0 ? 0 : cfgOptionUInt64(cfgOptJobRetryInterval)));

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
const char *
cfgCommandName(ConfigCommand commandId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
    FUNCTION_TEST_END();

    ASSERT(commandId < cfgCmdNone);

    FUNCTION_TEST_RETURN(configCommandData[commandId].name);
}

String *
cfgCommandRoleNameParam(ConfigCommand commandId, ConfigCommandRole commandRoleId, const String *separator)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
        FUNCTION_TEST_PARAM(ENUM, commandRoleId);
        FUNCTION_TEST_PARAM(STRING, separator);
    FUNCTION_TEST_END();

    String *result = strNew(cfgCommandName(commandId));

    if (commandRoleId != cfgCmdRoleDefault)
        strCatFmt(result, "%s%s", strZ(separator), strZ(cfgCommandRoleStr(commandRoleId)));

    FUNCTION_TEST_RETURN(result);
}

String *
cfgCommandRoleName(void)
{
    FUNCTION_TEST_VOID();

    FUNCTION_TEST_RETURN(cfgCommandRoleNameParam(cfgCommand(), cfgCommandRole(), COLON_STR));
}

/**********************************************************************************************************************************/
const StringList *
cfgCommandParam(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);

    if (configLocal->paramList == NULL)
    {
        MEM_CONTEXT_BEGIN(configLocal->memContext)
        {
            configLocal->paramList = strLstNew();
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(configLocal->paramList);
}

/**********************************************************************************************************************************/
STRING_STATIC(CONFIG_COMMAND_ROLE_ASYNC_STR,                        CONFIG_COMMAND_ROLE_ASYNC);
STRING_STATIC(CONFIG_COMMAND_ROLE_LOCAL_STR,                        CONFIG_COMMAND_ROLE_LOCAL);
STRING_STATIC(CONFIG_COMMAND_ROLE_REMOTE_STR,                       CONFIG_COMMAND_ROLE_REMOTE);

ConfigCommandRole
cfgCommandRoleEnum(const String *commandRole)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, commandRole);
    FUNCTION_TEST_END();

    if (commandRole == NULL)
        FUNCTION_TEST_RETURN(cfgCmdRoleDefault);
    else if (strEq(commandRole, CONFIG_COMMAND_ROLE_ASYNC_STR))
        FUNCTION_TEST_RETURN(cfgCmdRoleAsync);
    else if (strEq(commandRole, CONFIG_COMMAND_ROLE_LOCAL_STR))
        FUNCTION_TEST_RETURN(cfgCmdRoleLocal);
    else if (strEq(commandRole, CONFIG_COMMAND_ROLE_REMOTE_STR))
        FUNCTION_TEST_RETURN(cfgCmdRoleRemote);

    THROW_FMT(CommandInvalidError, "invalid command role '%s'", strZ(commandRole));
}

const String *
cfgCommandRoleStr(ConfigCommandRole commandRole)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandRole);
    FUNCTION_TEST_END();

    const String *result = NULL;

    switch (commandRole)
    {
        case cfgCmdRoleDefault:
            break;

        case cfgCmdRoleAsync:
            result = CONFIG_COMMAND_ROLE_ASYNC_STR;
            break;

        case cfgCmdRoleLocal:
            result = CONFIG_COMMAND_ROLE_LOCAL_STR;
            break;

        case cfgCmdRoleRemote:
            result = CONFIG_COMMAND_ROLE_REMOTE_STR;
            break;
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
const String *
cfgExe(void)
{
    FUNCTION_TEST_VOID();
    ASSERT(configLocal != NULL);
    FUNCTION_TEST_RETURN(configLocal->exe);
}

/**********************************************************************************************************************************/
bool
cfgLockRequired(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);
    ASSERT(configLocal->command != cfgCmdNone);

    // Local roles never take a lock and the remote role has special logic for locking
    FUNCTION_TEST_RETURN(
        // If a lock is required for the command and the role is default
        (configCommandData[cfgCommand()].lockRequired && cfgCommandRole() == cfgCmdRoleDefault) ||
        // Or any command when the role is async
        cfgCommandRole() == cfgCmdRoleAsync);
}

/**********************************************************************************************************************************/
bool
cfgLockRemoteRequired(void)
{
    FUNCTION_TEST_VOID();

    FUNCTION_TEST_RETURN(configCommandData[cfgCommand()].lockRemoteRequired);
}

/**********************************************************************************************************************************/
LockType
cfgLockType(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);
    ASSERT(configLocal->command != cfgCmdNone);

    FUNCTION_TEST_RETURN((LockType)configCommandData[cfgCommand()].lockType);
}

/**********************************************************************************************************************************/
bool
cfgLogFile(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);
    ASSERT(configLocal->command != cfgCmdNone);

    FUNCTION_TEST_RETURN(
        // If the command always logs to a file
        configCommandData[cfgCommand()].logFile ||
        // Or log-level-file was explicitly set as a param/env var
        (cfgOptionValid(cfgOptLogLevelFile) && cfgOptionSource(cfgOptLogLevelFile) == cfgSourceParam) ||
        // Or the role is async
        cfgCommandRole() == cfgCmdRoleAsync);
}

/**********************************************************************************************************************************/
LogLevel
cfgLogLevelDefault(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal != NULL);
    ASSERT(configLocal->command != cfgCmdNone);

    FUNCTION_TEST_RETURN((LogLevel)configCommandData[cfgCommand()].logLevelDefault);
}

/**********************************************************************************************************************************/
bool
cfgOptionGroup(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].group);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupId(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal->option[optionId].group);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].groupId);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupIdxDefault(ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);
    ASSERT(configLocal->optionGroup[groupId].indexDefaultExists);

    FUNCTION_TEST_RETURN(configLocal->optionGroup[groupId].indexDefault);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupIdxToKey(ConfigOptionGroup groupId, unsigned int groupIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
        FUNCTION_TEST_PARAM(UINT, groupIdx);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);
    ASSERT(groupIdx < configLocal->optionGroup[groupId].indexTotal);

    FUNCTION_TEST_RETURN(configLocal->optionGroup[groupId].indexMap[groupIdx] + 1);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionKeyToIdx(ConfigOption optionId, unsigned int key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, key);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    unsigned int result = 0;

    // If then option is in a group then search for the key, else the index is 0
    if (cfgOptionGroup(optionId))
    {
        unsigned int groupId = cfgOptionGroupId(optionId);

        // Seach the group for the key
        for (; result < cfgOptionGroupIdxTotal(groupId); result++)
        {
            if (configLocal->optionGroup[groupId].indexMap[result] == key - 1)
                break;
        }

        // Error when the key is not found
        if (result == cfgOptionGroupIdxTotal(groupId))
            THROW_FMT(AssertError, "key '%u' is not valid for '%s' option", key, configLocal->option[optionId].name);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupIdxTotal(ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);

    FUNCTION_TEST_RETURN(configLocal->optionGroup[groupId].indexTotal);
}

/**********************************************************************************************************************************/
bool
cfgOptionGroupValid(ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);

    FUNCTION_TEST_RETURN(configLocal->optionGroup[groupId].valid);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionIdxDefault(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(
        !configLocal->option[optionId].group || configLocal->optionGroup[configLocal->option[optionId].groupId].indexDefaultExists);

    FUNCTION_TEST_RETURN(
        configLocal->option[optionId].group ? configLocal->optionGroup[configLocal->option[optionId].groupId].indexDefault : 0);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionIdxTotal(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(
        configLocal->option[optionId].group ? configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal : 1);
}

/**********************************************************************************************************************************/
static Variant *
cfgOptionDefaultValue(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    Variant *result;
    Variant *defaultValue = varNewStrZ(cfgParseOptionDefault(cfgCommand(), optionId));

    switch (cfgParseOptionType(optionId))
    {
        case cfgOptTypeBoolean:
            result = varNewBool(varBoolForce(defaultValue));
            break;

        case cfgOptTypeInteger:
        case cfgOptTypeSize:
        case cfgOptTypeTime:
            result = varNewInt64(varInt64Force(defaultValue));
            break;

        case cfgOptTypePath:
        case cfgOptTypeString:
            result = varDup(defaultValue);
            break;

        default:
            THROW_FMT(AssertError, "default value not available for option type %u", cfgParseOptionType(optionId));
    }

    FUNCTION_TEST_RETURN(result);
}

const Variant *
cfgOptionDefault(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    if (configLocal->option[optionId].defaultValue == NULL)
    {
        if (cfgParseOptionDefault(cfgCommand(), optionId) != NULL)
        {
            MEM_CONTEXT_BEGIN(configLocal->memContext)
            {
                configLocal->option[optionId].defaultValue = cfgOptionDefaultValue(optionId);
            }
            MEM_CONTEXT_END();
        }
    }

    FUNCTION_TEST_RETURN(configLocal->option[optionId].defaultValue);
}

void
cfgOptionDefaultSet(ConfigOption optionId, const Variant *defaultValue)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(VARIANT, defaultValue);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(configLocal->option[optionId].valid);

    MEM_CONTEXT_BEGIN(configLocal->memContext)
    {
        // Set the default value
        configLocal->option[optionId].defaultValue = varDup(defaultValue);

        // Copy the value to option indexes that are marked as default so the default can be retrieved quickly
        for (unsigned int optionIdx = 0; optionIdx < cfgOptionIdxTotal(optionId); optionIdx++)
        {
            if (configLocal->option[optionId].index[optionIdx].source == cfgSourceDefault)
                configLocal->option[optionId].index[optionIdx].value = configLocal->option[optionId].defaultValue;
        }
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
cfgOptionHostPort(ConfigOption optionId, unsigned int *port)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM_P(UINT, port);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdxHostPort(optionId, cfgOptionIdxDefault(optionId), port));
}

String *
cfgOptionIdxHostPort(ConfigOption optionId, unsigned int optionIdx, unsigned int *port)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
        FUNCTION_TEST_PARAM_P(UINT, port);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));
    ASSERT(port != NULL);

    String *result = NULL;

    // Proceed if option is valid and has a value
    if (cfgOptionIdxTest(optionId, optionIdx))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const String *host = cfgOptionIdxStr(optionId, optionIdx);

            // If the host contains a colon then it has a port appended
            if (strChr(host, ':') != -1)
            {
                const StringList *hostPart = strLstNewSplitZ(host, ":");

                // More than one colon is invalid
                if (strLstSize(hostPart) > 2)
                {
                    THROW_FMT(
                        OptionInvalidError,
                        "'%s' is not valid for option '%s'"
                            "\nHINT: is more than one port specified?",
                        strZ(host), cfgOptionIdxName(optionId, optionIdx));
                }

                // Set the host
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = strDup(strLstGet(hostPart, 0));
                }
                MEM_CONTEXT_PRIOR_END();

                // Set the port and error if it is not a positive integer
                TRY_BEGIN()
                {
                    *port = cvtZToUInt(strZ(strLstGet(hostPart, 1)));
                }
                CATCH(FormatError)
                {
                    THROW_FMT(
                        OptionInvalidError,
                        "'%s' is not valid for option '%s'"
                            "\nHINT: port is not a positive integer.",
                        strZ(host), cfgOptionIdxName(optionId, optionIdx));
                }
                TRY_END();
            }
            // Else there is no port and just copy the host
            else
            {
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = strDup(host);
                }
                MEM_CONTEXT_PRIOR_END();
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get option name by id
***********************************************************************************************************************************/
const char *
cfgOptionName(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(cfgOptionIdxName(optionId, cfgOptionIdxDefault(optionId)));
}

const char *
cfgOptionIdxName(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    if (configLocal->option[optionId].group)
    {
        // This is somewhat less than ideal since memory is being allocated with each call, rather than caching prior results. In
        // practice the number of allocations should be quite small so we'll ignore this for now.
        String *name = strNewFmt(
            "%s%u%s", configLocal->optionGroup[configLocal->option[optionId].groupId].name,
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexMap[optionIdx] + 1,
            configLocal->option[optionId].name + strlen(configLocal->optionGroup[configLocal->option[optionId].groupId].name));

        FUNCTION_TEST_RETURN(strZ(name));
    }

    FUNCTION_TEST_RETURN(configLocal->option[optionId].name);
}

/**********************************************************************************************************************************/
bool
cfgOptionNegate(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdxNegate(optionId, cfgOptionIdxDefault(optionId)));
}

bool
cfgOptionIdxNegate(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[optionIdx].negate);
}

/**********************************************************************************************************************************/
bool
cfgOptionReset(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdxReset(optionId, cfgOptionIdxDefault(optionId)));
}

bool
cfgOptionIdxReset(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[optionIdx].reset);
}

/**********************************************************************************************************************************/
// Helper to enforce contraints when getting options
static const Variant *
cfgOptionIdxInternal(ConfigOption optionId, unsigned int optionIdx, VariantType typeRequested, bool nullAllowed)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
        FUNCTION_TEST_PARAM(ENUM, typeRequested);
        FUNCTION_TEST_PARAM(BOOL, nullAllowed);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    // Check that the option is valid for the current command
    if (!cfgOptionValid(optionId))
        THROW_FMT(AssertError, "option '%s' is not valid for the current command", cfgOptionIdxName(optionId, optionIdx));

    // If the option is not NULL then check it is the requested type
    const Variant *result = configLocal->option[optionId].index[optionIdx].value;

    if (result != NULL)
    {
        if (varType(result) != typeRequested)
        {
            THROW_FMT(
                AssertError, "option '%s' is type %u but %u was requested", cfgOptionIdxName(optionId, optionIdx), varType(result),
                typeRequested);
        }
    }
    // Else check the option is allowed to be NULL
    else if (!nullAllowed)
        THROW_FMT(AssertError, "option '%s' is null but non-null was requested", cfgOptionIdxName(optionId, optionIdx));

    FUNCTION_TEST_RETURN(result);
}

const Variant *
cfgOption(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdx(optionId, cfgOptionIdxDefault(optionId)));
}

const Variant *
cfgOptionIdx(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[optionIdx].value);
}

bool
cfgOptionBool(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, varBool(cfgOptionIdxInternal(optionId, cfgOptionIdxDefault(optionId), varTypeBool, false)));
}

bool
cfgOptionIdxBool(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, varBool(cfgOptionIdxInternal(optionId, optionIdx, varTypeBool, false)));
}

int
cfgOptionInt(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT, varIntForce(cfgOptionIdxInternal(optionId, cfgOptionIdxDefault(optionId), varTypeInt64, false)));
}

int
cfgOptionIdxInt(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT, varIntForce(cfgOptionIdxInternal(optionId, optionIdx, varTypeInt64, false)));
}

int64_t
cfgOptionInt64(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT64, varInt64(cfgOptionIdxInternal(optionId, cfgOptionIdxDefault(optionId), varTypeInt64, false)));
}

int64_t
cfgOptionIdxInt64(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT64, varInt64(cfgOptionIdxInternal(optionId, optionIdx, varTypeInt64, false)));
}

unsigned int
cfgOptionUInt(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT, varUIntForce(cfgOptionIdxInternal(optionId, cfgOptionIdxDefault(optionId), varTypeInt64, false)));
}

unsigned int
cfgOptionIdxUInt(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT, varUIntForce(cfgOptionIdxInternal(optionId, optionIdx, varTypeInt64, false)));
}

uint64_t
cfgOptionUInt64(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT64, varUInt64Force(cfgOptionIdxInternal(optionId, cfgOptionIdxDefault(optionId), varTypeInt64, false)));
}

uint64_t
cfgOptionIdxUInt64(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT64, varUInt64Force(cfgOptionIdxInternal(optionId, optionIdx, varTypeInt64, false)));
}

const KeyValue *
cfgOptionKv(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(KEY_VALUE, varKv(cfgOptionIdxInternal(optionId, cfgOptionIdxDefault(optionId), varTypeKeyValue, false)));
}

const KeyValue *
cfgOptionIdxKv(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(KEY_VALUE, varKv(cfgOptionIdxInternal(optionId, optionIdx, varTypeKeyValue, false)));
}

const VariantList *
cfgOptionLst(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(VARIANT_LIST, cfgOptionIdxLst(optionId, cfgOptionIdxDefault(optionId)));
}

const VariantList *
cfgOptionIdxLst(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    ASSERT(configLocal != NULL);

    const Variant *optionValue = cfgOptionIdxInternal(optionId, optionIdx, varTypeVariantList, true);

    if (optionValue == NULL)
    {
        MEM_CONTEXT_BEGIN(configLocal->memContext)
        {
            optionValue = varNewVarLst(varLstNew());
            configLocal->option[optionId].index[optionIdx].value = optionValue;
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(VARIANT_LIST, varVarLst(optionValue));
}

const String *
cfgOptionStr(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionIdxInternal(optionId, cfgOptionIdxDefault(optionId), varTypeString, false)));
}

const String *
cfgOptionIdxStr(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionIdxInternal(optionId, optionIdx, varTypeString, false)));
}

const String *
cfgOptionStrNull(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionIdxInternal(optionId, cfgOptionIdxDefault(optionId), varTypeString, true)));
}

const String *
cfgOptionIdxStrNull(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionIdxInternal(optionId, optionIdx, varTypeString, true)));
}

// Helper to convert option String values to StringIds. Some options need 6-bit encoding while most work fine with 5-bit encoding.
// At some point the config parser will work with StringIds directly and this code can be removed, but for now it protects the
// callers from this logic and hopefully means no changes to the callers when the parser is updated.
static StringId
cfgOptionStrIdInternal(
    const ConfigOption optionId, const unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    const String *const value = varStr(cfgOptionIdxInternal(optionId, optionIdx, varTypeString, false));

    if (optionId == cfgOptRepoType)
        FUNCTION_TEST_RETURN(strIdFromStr(stringIdBit6, value));

    FUNCTION_TEST_RETURN(strIdFromStr(stringIdBit5, value));
}

StringId
cfgOptionStrId(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(STRING_ID, cfgOptionStrIdInternal(optionId, cfgOptionIdxDefault(optionId)));
}

StringId
cfgOptionIdxStrId(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, optionIdx);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(STRING_ID, cfgOptionStrIdInternal(optionId, optionIdx));
}

/**********************************************************************************************************************************/
void
cfgOptionSet(ConfigOption optionId, ConfigSource source, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(ENUM, source);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    cfgOptionIdxSet(optionId, cfgOptionIdxDefault(optionId), source, value);

    FUNCTION_TEST_RETURN_VOID();
}

void
cfgOptionIdxSet(ConfigOption optionId, unsigned int optionIdx, ConfigSource source, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
        FUNCTION_TEST_PARAM(ENUM, source);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    MEM_CONTEXT_BEGIN(configLocal->memContext)
    {
        // Set the source
        configLocal->option[optionId].index[optionIdx].source = source;

        // Only set value if it is not null
        if (value != NULL)
        {
            switch (cfgParseOptionType(optionId))
            {
                case cfgOptTypeBoolean:
                {
                    if (varType(value) == varTypeBool)
                        configLocal->option[optionId].index[optionIdx].value = varDup(value);
                    else
                        configLocal->option[optionId].index[optionIdx].value = varNewBool(varBoolForce(value));

                    break;
                }

                case cfgOptTypeInteger:
                case cfgOptTypeSize:
                case cfgOptTypeTime:
                {
                    if (varType(value) == varTypeInt64)
                        configLocal->option[optionId].index[optionIdx].value = varDup(value);
                    else
                        configLocal->option[optionId].index[optionIdx].value = varNewInt64(varInt64Force(value));

                    break;
                }

                case cfgOptTypePath:
                case cfgOptTypeString:
                {
                    if (varType(value) == varTypeString)
                        configLocal->option[optionId].index[optionIdx].value = varDup(value);
                    else
                    {
                        THROW_FMT(
                            AssertError, "option '%s' must be set with String variant", cfgOptionIdxName(optionId, optionIdx));
                    }

                    break;
                }

                default:
                    THROW_FMT(AssertError, "set not available for option type %u", cfgParseOptionType(optionId));
            }
        }
        else
            configLocal->option[optionId].index[optionIdx].value = NULL;
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
ConfigSource
cfgOptionSource(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdxSource(optionId, cfgOptionIdxDefault(optionId)));
}

ConfigSource
cfgOptionIdxSource(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        (!configLocal->option[optionId].group && optionIdx == 0) ||
        (configLocal->option[optionId].group && optionIdx <
            configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[optionIdx].source);
}

/**********************************************************************************************************************************/
bool
cfgOptionTest(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(cfgOptionIdxTest(optionId, cfgOptionIdxDefault(optionId)));
}

bool
cfgOptionIdxTest(ConfigOption optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);
    ASSERT(
        !cfgOptionValid(optionId) ||
        ((!configLocal->option[optionId].group && optionIdx == 0) ||
         (configLocal->option[optionId].group && optionIdx <
          configLocal->optionGroup[configLocal->option[optionId].groupId].indexTotal)));

    FUNCTION_TEST_RETURN(cfgOptionValid(optionId) && configLocal->option[optionId].index[optionIdx].value != NULL);
}

/**********************************************************************************************************************************/
bool
cfgOptionValid(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].valid);
}

void
cfgOptionInvalidate(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configLocal != NULL);

    configLocal->option[optionId].valid = false;

    FUNCTION_TEST_RETURN_VOID();
}
