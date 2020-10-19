/***********************************************************************************************************************************
Command and Option Configuration
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/memContext.h"
#include "config/config.intern.h"
#include "config/define.h"

/***********************************************************************************************************************************
Map command names to ids and vice versa
***********************************************************************************************************************************/
typedef struct ConfigCommandData
{
    const char *name;

    bool internal:1;
    bool lockRequired:1;
    bool lockRemoteRequired:1;
    unsigned int lockType:2;

    bool logFile:1;
    unsigned int logLevelDefault:4;

    bool parameterAllowed:1;
} ConfigCommandData;

#define CONFIG_COMMAND_LIST(...)                                                                                                   \
    {__VA_ARGS__};

#define CONFIG_COMMAND(...)                                                                                                        \
    {__VA_ARGS__},

#define CONFIG_COMMAND_INTERNAL(internalParam)                                                                                     \
    .internal = internalParam,
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
#define CONFIG_COMMAND_PARAMETER_ALLOWED(parameterAllowedParam)                                                                    \
    .parameterAllowed = parameterAllowedParam,

/***********************************************************************************************************************************
Option group data
***********************************************************************************************************************************/
typedef struct ConfigOptionGroupData
{
    const char *name;                                               // All options in the group must be prefixed with this name
} ConfigOptionGroupData;

/***********************************************************************************************************************************
Map options names and indexes to option definitions
***********************************************************************************************************************************/
typedef struct ConfigOptionData
{
    const char *name;                                               // Option name
    bool group:1;                                                   // Is the option in a group?
    unsigned int groupId:1;                                         // Group id if option is in a group
} ConfigOptionData;

#define CONFIG_OPTION_LIST(...)                                                                                                    \
    {__VA_ARGS__};

#define CONFIG_OPTION(...)                                                                                                         \
    {__VA_ARGS__},

#define CONFIG_OPTION_NAME(nameParam)                                                                                              \
    .name = nameParam,
#define CONFIG_OPTION_GROUP(groupParam)                                                                                            \
    .group = groupParam,
#define CONFIG_OPTION_GROUP_ID(groupIdParam)                                                                                       \
    .groupId = groupIdParam,

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
    FUNCTION_TEST_RETURN(configLocal->commandRole);
}

void
cfgCommandSet(ConfigCommand commandId, ConfigCommandRole commandRoleId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
        FUNCTION_TEST_PARAM(ENUM, commandRoleId);
    FUNCTION_TEST_END();

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
        {
            result = CONFIG_COMMAND_ROLE_ASYNC_STR;
            break;
        }

        case cfgCmdRoleLocal:
        {
            result = CONFIG_COMMAND_ROLE_LOCAL_STR;
            break;
        }

        case cfgCmdRoleRemote:
        {
            result = CONFIG_COMMAND_ROLE_REMOTE_STR;
            break;
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
const String *
cfgExe(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(configLocal->exe);
}

/**********************************************************************************************************************************/
bool
cfgCommandInternal(ConfigCommand commandId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
    FUNCTION_TEST_END();

    ASSERT(commandId < cfgCmdNone);

    FUNCTION_TEST_RETURN(configCommandData[commandId].internal);
}

/**********************************************************************************************************************************/
bool
cfgLockRequired(void)
{
    FUNCTION_TEST_VOID();

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

    ASSERT(configLocal->command != cfgCmdNone);

    FUNCTION_TEST_RETURN((LockType)configCommandData[cfgCommand()].lockType);
}

/**********************************************************************************************************************************/
bool
cfgLogFile(void)
{
    FUNCTION_TEST_VOID();

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

    ASSERT(configLocal->command != cfgCmdNone);

    FUNCTION_TEST_RETURN((LogLevel)configCommandData[cfgCommand()].logLevelDefault);
}

/**********************************************************************************************************************************/
bool
cfgCommandParameterAllowed(ConfigCommand commandId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
    FUNCTION_TEST_END();

    ASSERT(commandId < cfgCmdNone);

    FUNCTION_TEST_RETURN(configCommandData[commandId].parameterAllowed);
}

/**********************************************************************************************************************************/
bool
cfgOptionGroup(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configOptionData[optionId].group);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupId(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configOptionData[optionId].group);

    FUNCTION_TEST_RETURN(configOptionData[optionId].groupId);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupIdxDefault(ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);

    FUNCTION_TEST_RETURN(configLocal->optionGroup[groupId].indexDefault);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupIdxToRawIdx(ConfigOptionGroup groupId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);
    ASSERT(index < configLocal->optionGroup[groupId].indexTotal);

    FUNCTION_TEST_RETURN(configLocal->optionGroup[groupId].index[index] + 1);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupIdxTotal(ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

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

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(
        configOptionData[optionId].group ? configLocal->optionGroup[configOptionData[optionId].groupId].indexDefault : 0);
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionIdxTotal(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(
        configOptionData[optionId].group ? configLocal->optionGroup[configOptionData[optionId].groupId].indexTotal : 1);
}

/**********************************************************************************************************************************/
static Variant *
cfgOptionDefaultValue(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    Variant *result;
    Variant *defaultValue = varNewStrZ(cfgDefOptionDefault(cfgCommand(), optionId));

    switch (cfgDefOptionType(optionId))
    {
        case cfgDefOptTypeBoolean:
        {
            result = varNewBool(varBoolForce(defaultValue));
            break;
        }

        case cfgDefOptTypeFloat:
        {
            result = varNewDbl(varDblForce(defaultValue));
            break;
        }

        case cfgDefOptTypeInteger:
        case cfgDefOptTypeSize:
        {
            result = varNewInt64(varInt64Force(defaultValue));
            break;
        }

        case cfgDefOptTypePath:
        case cfgDefOptTypeString:
            result = varDup(defaultValue);
            break;

        default:
            THROW_FMT(AssertError, "default value not available for option type %d", cfgDefOptionType(optionId));
    }

    FUNCTION_TEST_RETURN(result);
}

const Variant *
cfgOptionDefault(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    if (configLocal->option[optionId].defaultValue == NULL)
    {
        if (cfgDefOptionDefault(cfgCommand(), optionId) != NULL)
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
    ASSERT(configLocal->option[optionId].valid);

    MEM_CONTEXT_BEGIN(configLocal->memContext)
    {
        configLocal->option[optionId].defaultValue = varDup(defaultValue);

        for (unsigned int index = 0; index < cfgOptionIdxTotal(optionId); index++)
        {
            if (configLocal->option[optionId].index[index].source == cfgSourceDefault)
                configLocal->option[optionId].index[index].value = configLocal->option[optionId].defaultValue;
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
cfgOptionIdxHostPort(ConfigOption optionId, unsigned int index, unsigned int *port)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
        FUNCTION_TEST_PARAM_P(UINT, port);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(
        (!configOptionData[optionId].group && index == 0) ||
        (configOptionData[optionId].group && index < configLocal->optionGroup[configOptionData[optionId].groupId].indexTotal));
    ASSERT(port != NULL);

    String *result = NULL;

    // Proceed if option is valid and has a value
    if (cfgOptionTest(optionId))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const String *host = cfgOptionIdxStr(optionId, index);

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
                        strZ(host), cfgOptionIdxName(optionId, index));
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
                        strZ(host), cfgOptionIdxName(optionId, index));
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
cfgOptionRawIdxName(ConfigOption optionId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT((!configOptionData[optionId].group && index == 0) || configOptionData[optionId].group);

    if (configOptionData[optionId].group)
    {
        // !!! NOT RIGHT SINCE THIS RETURNS CONST BUT ALLOCATES MEMORY IN THE CALLING CONTEXT
        String *name = strNewFmt(
            "%s%u%s", configOptionGroupData[configOptionData[optionId].groupId].name, index + 1,
            configOptionData[optionId].name + strlen(configOptionGroupData[configOptionData[optionId].groupId].name));

        FUNCTION_TEST_RETURN(strZ(name));
    }

    FUNCTION_TEST_RETURN(configOptionData[optionId].name);
}

const char *
cfgOptionIdxName(ConfigOption optionId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(
        (!configOptionData[optionId].group && index == 0) ||
        (configOptionData[optionId].group && index < configLocal->optionGroup[configOptionData[optionId].groupId].indexTotal));

    if (configOptionData[optionId].group)
    {
        FUNCTION_TEST_RETURN(
            cfgOptionRawIdxName(optionId, configLocal->optionGroup[configOptionData[optionId].groupId].index[index]));
    }

    FUNCTION_TEST_RETURN(configOptionData[optionId].name);
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
cfgOptionIdxNegate(ConfigOption optionId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(
        (!configOptionData[optionId].group && index == 0) ||
        (configOptionData[optionId].group && index < configLocal->optionGroup[configOptionData[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[index].negate);
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
cfgOptionIdxReset(ConfigOption optionId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(
        (!configOptionData[optionId].group && index == 0) ||
        (configOptionData[optionId].group && index < configLocal->optionGroup[configOptionData[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[index].reset);
}

/**********************************************************************************************************************************/
// Helper to enforce contraints when getting options
static const Variant *
cfgOptionIdxInternal(ConfigOption optionId, unsigned int index, VariantType typeRequested, bool nullAllowed)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
        FUNCTION_TEST_PARAM(ENUM, typeRequested);
        FUNCTION_TEST_PARAM(BOOL, nullAllowed);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(
        (!configOptionData[optionId].group && index == 0) ||
        (configOptionData[optionId].group && index < configLocal->optionGroup[configOptionData[optionId].groupId].indexTotal));

    // Check that the option is valid for the current command
    if (!cfgOptionValid(optionId))
        THROW_FMT(AssertError, "option '%s' is not valid for the current command", cfgOptionIdxName(optionId, index));

    // If the option is not NULL then check it is the requested type
    const Variant *result = configLocal->option[optionId].index[index].value;

    if (result != NULL)
    {
        if (varType(result) != typeRequested)
        {
            THROW_FMT(
                AssertError, "option '%s' is type %u but %u was requested", cfgOptionIdxName(optionId, index), varType(result),
                typeRequested);
        }
    }
    // Else check the option is allowed to be NULL
    else if (!nullAllowed)
        THROW_FMT(AssertError, "option '%s' is null but non-null was requested", cfgOptionIdxName(optionId, index));

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
cfgOptionIdx(ConfigOption optionId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(
        (!configOptionData[optionId].group && index == 0) ||
        (configOptionData[optionId].group && index < configLocal->optionGroup[configOptionData[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[index].value);
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
cfgOptionIdxBool(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, varBool(cfgOptionIdxInternal(optionId, index, varTypeBool, false)));
}

double
cfgOptionDbl(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(DOUBLE, varDbl(cfgOptionIdxInternal(optionId, cfgOptionIdxDefault(optionId), varTypeDouble, false)));
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
cfgOptionIdxInt(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT, varIntForce(cfgOptionIdxInternal(optionId, index, varTypeInt64, false)));
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
cfgOptionIdxInt64(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT64, varInt64(cfgOptionIdxInternal(optionId, index, varTypeInt64, false)));
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
cfgOptionIdxUInt(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT, varUIntForce(cfgOptionIdxInternal(optionId, index, varTypeInt64, false)));
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
cfgOptionIdxUInt64(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT64, varUInt64Force(cfgOptionIdxInternal(optionId, index, varTypeInt64, false)));
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
cfgOptionIdxKv(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(KEY_VALUE, varKv(cfgOptionIdxInternal(optionId, index, varTypeKeyValue, false)));
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
cfgOptionIdxLst(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    const Variant *optionValue = cfgOptionIdxInternal(optionId, index, varTypeVariantList, true);

    if (optionValue == NULL)
    {
        MEM_CONTEXT_BEGIN(configLocal->memContext)
        {
            optionValue = varNewVarLst(varLstNew());
            configLocal->option[optionId].index[index].value = optionValue;
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
cfgOptionIdxStr(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionIdxInternal(optionId, index, varTypeString, false)));
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
cfgOptionIdxStrNull(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionIdxInternal(optionId, index, varTypeString, true)));
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
cfgOptionIdxSet(ConfigOption optionId, unsigned int index, ConfigSource source, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
        FUNCTION_TEST_PARAM(ENUM, source);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(
        (!configOptionData[optionId].group && index == 0) ||
        (configOptionData[optionId].group && index < configLocal->optionGroup[configOptionData[optionId].groupId].indexTotal));

    MEM_CONTEXT_BEGIN(configLocal->memContext)
    {
        // Set the source
        configLocal->option[optionId].index[index].source = source;

        // Only set value if it is not null
        if (value != NULL)
        {
            switch (cfgDefOptionType(optionId))
            {
                case cfgDefOptTypeBoolean:
                {
                    if (varType(value) == varTypeBool)
                        configLocal->option[optionId].index[index].value = varDup(value);
                    else
                        configLocal->option[optionId].index[index].value = varNewBool(varBoolForce(value));

                    break;
                }

                case cfgDefOptTypeFloat:
                {
                    if (varType(value) == varTypeDouble)
                        configLocal->option[optionId].index[index].value = varDup(value);
                    else
                        configLocal->option[optionId].index[index].value = varNewDbl(varDblForce(value));

                    break;
                }

                case cfgDefOptTypeInteger:
                case cfgDefOptTypeSize:
                {
                    if (varType(value) == varTypeInt64)
                        configLocal->option[optionId].index[index].value = varDup(value);
                    else
                        configLocal->option[optionId].index[index].value = varNewInt64(varInt64Force(value));

                    break;
                }

                case cfgDefOptTypeHash:
                {
                    if (varType(value) == varTypeKeyValue)
                        configLocal->option[optionId].index[index].value = varDup(value);
                    else
                        THROW_FMT(AssertError, "option '%s' must be set with KeyValue variant", cfgOptionIdxName(optionId, index));

                    break;
                }

                case cfgDefOptTypeList:
                {
                    if (varType(value) == varTypeVariantList)
                        configLocal->option[optionId].index[index].value = varDup(value);
                    else
                    {
                        THROW_FMT(
                            AssertError, "option '%s' must be set with VariantList variant", cfgOptionIdxName(optionId, index));
                    }

                    break;
                }

                case cfgDefOptTypePath:
                case cfgDefOptTypeString:
                {
                    if (varType(value) == varTypeString)
                        configLocal->option[optionId].index[index].value = varDup(value);
                    else
                        THROW_FMT(AssertError, "option '%s' must be set with String variant", cfgOptionIdxName(optionId, index));

                    break;
                }
            }
        }
        else
            configLocal->option[optionId].index[index].value = NULL;
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
cfgOptionIdxSource(ConfigOption optionId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(
        (!configOptionData[optionId].group && index == 0) ||
        (configOptionData[optionId].group && index < configLocal->optionGroup[configOptionData[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[index].source);
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
cfgOptionIdxTest(ConfigOption optionId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(
        (!configOptionData[optionId].group && index == 0) ||
        (configOptionData[optionId].group && index < configLocal->optionGroup[configOptionData[optionId].groupId].indexTotal));

    FUNCTION_TEST_RETURN(cfgOptionValid(optionId) && configLocal->option[optionId].index[index].value != NULL);
}

/**********************************************************************************************************************************/
bool
cfgOptionValid(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].valid);
}

void
cfgOptionInvalidate(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    configLocal->option[optionId].valid = false;

    FUNCTION_TEST_RETURN_VOID();
}
