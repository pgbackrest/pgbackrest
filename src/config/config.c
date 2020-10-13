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
    FUNCTION_TEST_RETURN(configLocal->command);
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
cfgCommandId(const char *commandName, bool error)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, commandName);
        FUNCTION_TEST_PARAM(BOOL, error);
    FUNCTION_TEST_END();

    ASSERT(commandName != NULL);

    ConfigCommand commandId;

    for (commandId = 0; commandId < cfgCmdNone; commandId++)
        if (strcmp(commandName, configCommandData[commandId].name) == 0)
            break;

    if (commandId == cfgCmdNone && error)
        THROW_FMT(AssertError, "invalid command '%s'", commandName);

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

void
cfgCommandParamSet(const StringList *param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, param);
    FUNCTION_TEST_END();

    ASSERT(param != NULL);

    MEM_CONTEXT_BEGIN(configLocal->memContext)
    {
        configLocal->paramList = strLstDup(param);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
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
        cfgOptionSource(cfgOptLogLevelFile) == cfgSourceParam ||
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
cfgParameterAllowed(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configLocal->command != cfgCmdNone);

    FUNCTION_TEST_RETURN(configCommandData[cfgCommand()].parameterAllowed);
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
bool
cfgOptionGroupIdxTest(ConfigOptionGroup groupId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);

    FUNCTION_TEST_RETURN(index < configLocal->optionGroup[groupId].indexTotal);
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
        if (configLocal->option[optionId].defaultValue != NULL)
            varFree(configLocal->option[optionId].defaultValue);

        configLocal->option[optionId].defaultValue = varDup(defaultValue);
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

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(port != NULL);

    String *result = NULL;

    // Proceed if option is valid and has a value
    if (cfgOptionTest(optionId))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const String *host = cfgOptionStr(optionId);

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
                        strZ(host), cfgOptionName(optionId));
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
                        strZ(host), cfgOptionName(optionId));
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

/**********************************************************************************************************************************/
int
cfgOptionId(const char *optionName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, optionName);
    FUNCTION_TEST_END();

    ASSERT(optionName != NULL);

    int result = -1;

    for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
        if (strcmp(optionName, configOptionData[optionId].name) == 0)
            result = optionId;

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

    if (configOptionData[optionId].group)
    {
        // !!! NOT RIGHT SINCE THIS RETURNS CONST BUT ALLOCATES MEMORY IN THE CALLING CONTEXT
        String *name = strNewFmt(
            "%s%u%s", configOptionGroupData[configOptionData[optionId].groupId].name, index + 1,
            configOptionData[optionId].name + strlen(configOptionGroupData[configOptionData[optionId].groupId].name));

        FUNCTION_TEST_RETURN(strZ(name));
    }

    ASSERT(index == 0);
    FUNCTION_TEST_RETURN(cfgOptionName(optionId));
}

/**********************************************************************************************************************************/
bool
cfgOptionNegate(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(cfgOptionIdxNegate(optionId, 0));
}

bool
cfgOptionIdxNegate(ConfigOption optionId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configOptionData[optionId].group || index == 0);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[index].negate);
}

/**********************************************************************************************************************************/
bool
cfgOptionReset(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(cfgOptionIdxReset(optionId, 0));
}

bool
cfgOptionIdxReset(ConfigOption optionId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configOptionData[optionId].group || index == 0);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[index].reset);
}

/**********************************************************************************************************************************/
// Helper to enforce contraints when getting options
static Variant *
cfgOptionInternal(ConfigOption optionId, unsigned int index, VariantType typeRequested, bool nullAllowed, bool indexed)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
        FUNCTION_TEST_PARAM(ENUM, typeRequested);
        FUNCTION_TEST_PARAM(BOOL, nullAllowed);
        FUNCTION_TEST_PARAM(BOOL, indexed);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    CHECK(!indexed || configOptionData[optionId].group);
    ASSERT(
        (indexed && index < configLocal->optionGroup[configOptionData[optionId].groupId].indexTotal) ||
        (!indexed && index == 0));

    // Calculate the option id with index
    unsigned int optionIdx = optionId + index;

    // Check that the option is valid for the current command
    if (!cfgOptionValid(optionIdx))
        THROW_FMT(AssertError, "option '%s' is not valid for the current command", cfgOptionName(optionIdx));

    // If the option is not NULL then check it is the requested type
    Variant *result = configLocal->option[optionIdx].index[index].value;

    if (result != NULL)
    {
        if (varType(result) != typeRequested)
        {
            THROW_FMT(
                AssertError, "option '%s' is type %u but %u was requested", cfgOptionName(optionIdx), varType(result),
                typeRequested);
        }
    }
    // Else check the option is allowed to be NULL
    else if (!nullAllowed)
        THROW_FMT(AssertError, "option '%s' is null but non-null was requested", cfgOptionName(optionIdx));

    FUNCTION_TEST_RETURN(result);
}

const Variant *
cfgOption(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].index[0].value);
}

bool
cfgOptionBool(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, varBool(cfgOptionInternal(optionId, 0, varTypeBool, false, false)));
}

bool
cfgOptionIdxBool(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, varBool(cfgOptionInternal(optionId, index, varTypeBool, false, true)));
}

double
cfgOptionDbl(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(DOUBLE, varDbl(cfgOptionInternal(optionId, 0, varTypeDouble, false, false)));
}

int
cfgOptionInt(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT, varIntForce(cfgOptionInternal(optionId, 0, varTypeInt64, false, false)));
}

int
cfgOptionIdxInt(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT, varIntForce(cfgOptionInternal(optionId, index, varTypeInt64, false, true)));
}

int64_t
cfgOptionInt64(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT64, varInt64(cfgOptionInternal(optionId, 0, varTypeInt64, false, false)));
}

int64_t
cfgOptionIdxInt64(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT64, varInt64(cfgOptionInternal(optionId, index, varTypeInt64, false, true)));
}

unsigned int
cfgOptionUInt(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT, varUIntForce(cfgOptionInternal(optionId, 0, varTypeInt64, false, false)));
}

unsigned int
cfgOptionIdxUInt(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT, varUIntForce(cfgOptionInternal(optionId, index, varTypeInt64, false, true)));
}

uint64_t
cfgOptionUInt64(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT64, varUInt64Force(cfgOptionInternal(optionId, 0, varTypeInt64, false, false)));
}

uint64_t
cfgOptionIdxUInt64(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT64, varUInt64Force(cfgOptionInternal(optionId, index, varTypeInt64, false, true)));
}

const KeyValue *
cfgOptionKv(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(KEY_VALUE, varKv(cfgOptionInternal(optionId, 0, varTypeKeyValue, false, false)));
}

const VariantList *
cfgOptionLst(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    Variant *optionValue = cfgOptionInternal(optionId, 0, varTypeVariantList, true, false);

    if (optionValue == NULL)
    {
        MEM_CONTEXT_BEGIN(configLocal->memContext)
        {
            optionValue = varNewVarLst(varLstNew());
            configLocal->option[optionId].index[0].value = optionValue;
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

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionInternal(optionId, 0, varTypeString, false, false)));
}

const String *
cfgOptionIdxStr(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionInternal(optionId, index, varTypeString, false, true)));
}

const String *
cfgOptionStrNull(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionInternal(optionId, 0, varTypeString, true, false)));
}

const String *
cfgOptionIdxStrNull(ConfigOption optionId, unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionInternal(optionId, index, varTypeString, true, true)));
}

/**********************************************************************************************************************************/
void
cfgOptionIdxSet(ConfigOption optionId, ConfigSource source, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(ENUM, source);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    cfgOptionIdxSet(optionId, 0, source, value);

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

    MEM_CONTEXT_BEGIN(configLocal->memContext)
    {
        // Set the source
        configLocal->option[optionId].index.source = source;

        // Store old value
        Variant *valueOld = configLocal->option[optionId].value;

        // Only set value if it is not null
        if (value != NULL)
        {
            // If this option is in a group then set the value in the group for the option's index. This indicates that there is at
            // least one option set in the group for that index.
            if (configOptionData[optionId].group && source != cfgSourceDefault)
            {
                // FIX THIS
                // unsigned int groupId = configOptionData[optionId].groupId;

                // configLocal->optionGroup[groupId].value |= (unsigned int)1 << configOptionData[optionId].index;
                //
                // if (configOptionData[optionId].index > configLocal->optionGroup[groupId].indexMax)
                //     configLocal->optionGroup[groupId].indexMax = configOptionData[optionId].index;
            }

            switch (cfgDefOptionType(optionId))
            {
                case cfgDefOptTypeBoolean:
                {
                    if (varType(value) == varTypeBool)
                        configLocal->option[optionId].value = varDup(value);
                    else
                        configLocal->option[optionId].value = varNewBool(varBoolForce(value));

                    break;
                }

                case cfgDefOptTypeFloat:
                {
                    if (varType(value) == varTypeDouble)
                        configLocal->option[optionId].value = varDup(value);
                    else
                        configLocal->option[optionId].value = varNewDbl(varDblForce(value));

                    break;
                }

                case cfgDefOptTypeInteger:
                case cfgDefOptTypeSize:
                {
                    if (varType(value) == varTypeInt64)
                        configLocal->option[optionId].value = varDup(value);
                    else
                        configLocal->option[optionId].value = varNewInt64(varInt64Force(value));

                    break;
                }

                case cfgDefOptTypeHash:
                {
                    if (varType(value) == varTypeKeyValue)
                        configLocal->option[optionId].value = varDup(value);
                    else
                        THROW_FMT(AssertError, "option '%s' must be set with KeyValue variant", cfgOptionName(optionId));

                    break;
                }

                case cfgDefOptTypeList:
                {
                    if (varType(value) == varTypeVariantList)
                        configLocal->option[optionId].value = varDup(value);
                    else
                        THROW_FMT(AssertError, "option '%s' must be set with VariantList variant", cfgOptionName(optionId));

                    break;
                }

                case cfgDefOptTypePath:
                case cfgDefOptTypeString:
                {
                    if (varType(value) == varTypeString)
                        configLocal->option[optionId].value = varDup(value);
                    else
                        THROW_FMT(AssertError, "option '%s' must be set with String variant", cfgOptionName(optionId));

                    break;
                }
            }
        }
        else
            configLocal->option[optionId].value = NULL;

        // Free old value
        if (valueOld != NULL)
            varFree(valueOld);
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

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configLocal->option[optionId].source);
}

/**********************************************************************************************************************************/
bool
cfgOptionTest(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(cfgOptionValid(optionId) && configLocal->option[optionId].value != NULL);
}

bool
cfgOptionIdxTest(ConfigOption optionId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configOptionData[optionId].group);
    ASSERT(index <= configLocal->optionGroup[configOptionData[optionId].groupId].indexMax);

    FUNCTION_TEST_RETURN(cfgOptionValid(optionId) && configLocal->option[optionId + index].value != NULL);
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
