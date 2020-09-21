/***********************************************************************************************************************************
Command and Option Configuration
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/memContext.h"
#include "config/config.h"

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
Map options names and indexes to option definitions
***********************************************************************************************************************************/
typedef struct ConfigOptionData
{
    const char *name;

    unsigned int index:5;
    unsigned int defineId:7;
    bool group:1;                                                   // Is the option in a group?
    unsigned int groupId:1;                                         // Group id if option is in a group
} ConfigOptionData;

#define CONFIG_OPTION_LIST(...)                                                                                                    \
    {__VA_ARGS__};

#define CONFIG_OPTION(...)                                                                                                         \
    {__VA_ARGS__},

#define CONFIG_OPTION_INDEX(indexParam)                                                                                            \
    .index = indexParam,
#define CONFIG_OPTION_NAME(nameParam)                                                                                              \
    .name = nameParam,
#define CONFIG_OPTION_DEFINE_ID(defineIdParam)                                                                                     \
    .defineId = defineIdParam,
#define CONFIG_OPTION_GROUP(groupParam)                                                                                            \
    .group = groupParam,
#define CONFIG_OPTION_GROUP_ID(groupIdParam)                                                                                       \
    .groupId = groupIdParam,

/***********************************************************************************************************************************
Include the automatically generated configuration data
***********************************************************************************************************************************/
#include "config/config.auto.c"

/***********************************************************************************************************************************
Static data for the currently loaded configuration
***********************************************************************************************************************************/
static struct ConfigStatic
{
    MemContext *memContext;                                         // Mem context for config data (child of top context)

    // Generally set by the command parser but can also be set by during execute to change commands, i.e. backup -> expire
    ConfigCommand command;                                          // Current command
    ConfigCommandRole commandRole;                                  // Current command role

    String *exe;                                                    // Location of the executable
    bool help;                                                      // Was help requested for the command?
    StringList *paramList;                                          // Parameters passed to the command (if any)

    // Group options that are related together to allow valid and test checks across all options in the group
    struct
    {
        bool valid;                                                 // Is option group valid for the current command?
        unsigned int indexMax;                                      // Max index in option group
        unsigned int value;                                         // Does option group index have any values?
    } optionGroup[CFG_OPTION_GROUP_TOTAL];

    // Map options names and indexes to option definitions
    struct
    {
        bool valid:1;                                               // Is option valid for current command?
        bool negate:1;                                              // Is the option negated?
        bool reset:1;                                               // Is the option reset?
        unsigned int source:2;                                      // Where the option came from, i.e. ConfigSource enum

        Variant *value;                                             // Value
        Variant *defaultValue;                                      // Default value
    } option[CFG_OPTION_TOTAL];
} configStatic;

/**********************************************************************************************************************************/
void
cfgInit(void)
{
    FUNCTION_TEST_VOID();

    // Free the old context
    if (configStatic.memContext != NULL)
        memContextFree(configStatic.memContext);

    // Initialize config data
    configStatic = (struct ConfigStatic){.command = cfgCmdNone};

    // Allocate configuration context as a child of the top context
    MEM_CONTEXT_BEGIN(memContextTop())
    {
        MEM_CONTEXT_NEW_BEGIN("configuration")
        {
            configStatic.memContext = MEM_CONTEXT_NEW();
        }
        MEM_CONTEXT_NEW_END();
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
ConfigCommand
cfgCommand(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(configStatic.command);
}

ConfigCommandRole
cfgCommandRole(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(configStatic.commandRole);
}

void
cfgCommandSet(ConfigCommand commandId, ConfigCommandRole commandRoleId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
        FUNCTION_TEST_PARAM(ENUM, commandRoleId);
    FUNCTION_TEST_END();

    ASSERT(commandId <= cfgCmdNone);

    configStatic.command = commandId;
    configStatic.commandRole = commandRoleId;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
cfgCommandHelp(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(configStatic.help);
}

void
cfgCommandHelpSet(bool help)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, help);
    FUNCTION_TEST_END();

    configStatic.help = help;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
ConfigDefineCommand
cfgCommandDefIdFromId(ConfigCommand commandId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
    FUNCTION_TEST_END();

    ASSERT(commandId < cfgCmdNone);

    FUNCTION_TEST_RETURN((ConfigDefineCommand)commandId);
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

    if (configStatic.paramList == NULL)
    {
        MEM_CONTEXT_BEGIN(configStatic.memContext)
        {
            configStatic.paramList = strLstNew();
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(configStatic.paramList);
}

void
cfgCommandParamSet(const StringList *param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, param);
    FUNCTION_TEST_END();

    ASSERT(param != NULL);

    MEM_CONTEXT_BEGIN(configStatic.memContext)
    {
        configStatic.paramList = strLstDup(param);
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
    FUNCTION_TEST_RETURN(configStatic.exe);
}

void
cfgExeSet(const String *exe)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, exe);
    FUNCTION_TEST_END();

    ASSERT(exe != NULL);

    MEM_CONTEXT_BEGIN(configStatic.memContext)
    {
        configStatic.exe = strDup(exe);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
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

    ASSERT(configStatic.command != cfgCmdNone);

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

    ASSERT(configStatic.command != cfgCmdNone);

    FUNCTION_TEST_RETURN((LockType)configCommandData[cfgCommand()].lockType);
}

/**********************************************************************************************************************************/
bool
cfgLogFile(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configStatic.command != cfgCmdNone);

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

    ASSERT(configStatic.command != cfgCmdNone);

    FUNCTION_TEST_RETURN((LogLevel)configCommandData[cfgCommand()].logLevelDefault);
}

/**********************************************************************************************************************************/
bool
cfgParameterAllowed(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(configStatic.command != cfgCmdNone);

    FUNCTION_TEST_RETURN(configCommandData[cfgCommand()].parameterAllowed);
}

/**********************************************************************************************************************************/
bool
cfgOptionGroupIndexTest(ConfigOptionGroup groupId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);

    FUNCTION_TEST_RETURN(
        cfgOptionGroupValid(groupId) && (index == 0 || configStatic.optionGroup[groupId].value & ((unsigned int)1 << index)));
}

/**********************************************************************************************************************************/
unsigned int
cfgOptionGroupIndexTotal(ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);

    FUNCTION_TEST_RETURN(cfgOptionGroupValid(groupId) ? configStatic.optionGroup[groupId].indexMax + 1 : 0);
}

/**********************************************************************************************************************************/
bool
cfgOptionGroupValid(ConfigOptionGroup groupId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, groupId);
    FUNCTION_TEST_END();

    ASSERT(groupId < CFG_OPTION_GROUP_TOTAL);

    FUNCTION_TEST_RETURN(configStatic.optionGroup[groupId].valid);
}

/**********************************************************************************************************************************/
ConfigDefineOption
cfgOptionDefIdFromId(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configOptionData[optionId].defineId);
}

/**********************************************************************************************************************************/
static Variant *
cfgOptionDefaultValue(ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    Variant *result;
    Variant *defaultValue = varNewStrZ(cfgDefOptionDefault(cfgCommandDefIdFromId(cfgCommand()), optionDefId));

    switch (cfgDefOptionType(optionDefId))
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
            THROW_FMT(AssertError, "default value not available for option type %d", cfgDefOptionType(optionDefId));
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

    if (configStatic.option[optionId].defaultValue == NULL)
    {
        ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

        if (cfgDefOptionDefault(cfgCommandDefIdFromId(cfgCommand()), optionDefId) != NULL)
        {
            MEM_CONTEXT_BEGIN(configStatic.memContext)
            {
                configStatic.option[optionId].defaultValue = cfgOptionDefaultValue(optionDefId);
            }
            MEM_CONTEXT_END();
        }
    }

    FUNCTION_TEST_RETURN(configStatic.option[optionId].defaultValue);
}

void
cfgOptionDefaultSet(ConfigOption optionId, const Variant *defaultValue)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(VARIANT, defaultValue);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    MEM_CONTEXT_BEGIN(configStatic.memContext)
    {
        if (configStatic.option[optionId].defaultValue != NULL)
            varFree(configStatic.option[optionId].defaultValue);

        configStatic.option[optionId].defaultValue = varDup(defaultValue);

        if (configStatic.option[optionId].source == cfgSourceDefault)
        {
            if (configStatic.option[optionId].value != NULL)
                varFree(configStatic.option[optionId].value);

            configStatic.option[optionId].value = varDup(defaultValue);
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
unsigned int
cfgOptionIndex(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configOptionData[optionId].index);
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

/**********************************************************************************************************************************/
ConfigOption
cfgOptionIdFromDefId(ConfigDefineOption optionDefId, unsigned int index)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
        FUNCTION_TEST_PARAM(UINT, index);
    FUNCTION_TEST_END();

    // Search for the option
    ConfigOption optionId;

    for (optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
    {
        if (configOptionData[optionId].defineId == optionDefId)
            break;
    }

    // If the mapping is not found then there is a bug in the code generator
    ASSERT(optionId != CFG_OPTION_TOTAL);

    // Make sure the index is valid
    ASSERT(index < cfgDefOptionIndexTotal(optionDefId));

    // Return with original index
    FUNCTION_TEST_RETURN(optionId + index);
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

/**********************************************************************************************************************************/
bool
cfgOptionNegate(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configStatic.option[optionId].negate);
}

void
cfgOptionNegateSet(ConfigOption optionId, bool negate)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(BOOL, negate);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    configStatic.option[optionId].negate = negate;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
cfgOptionReset(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configStatic.option[optionId].reset);
}

void
cfgOptionResetSet(ConfigOption optionId, bool reset)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(BOOL, reset);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    configStatic.option[optionId].reset = reset;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
// Helper to enforce contraints when getting options
static Variant *
cfgOptionInternal(ConfigOption optionId, VariantType typeRequested, bool nullAllowed)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(ENUM, typeRequested);
        FUNCTION_TEST_PARAM(BOOL, nullAllowed);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    // Check that the option is valid for the current command
    if (!cfgOptionValid(optionId))
        THROW_FMT(AssertError, "option '%s' is not valid for the current command", cfgOptionName(optionId));

    // If the option is not NULL then check it is the requested type
    Variant *result = configStatic.option[optionId].value;

    if (result != NULL)
    {
        if (varType(result) != typeRequested)
        {
            THROW_FMT(
                AssertError, "option '%s' is type %u but %u was requested", cfgOptionName(optionId), varType(result),
                typeRequested);
        }
    }
    // Else check the option is allowed to be NULL
    else if (!nullAllowed)
        THROW_FMT(AssertError, "option '%s' is null but non-null was requested", cfgOptionName(optionId));

    FUNCTION_TEST_RETURN(result);
}

const Variant *
cfgOption(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configStatic.option[optionId].value);
}

bool
cfgOptionBool(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, varBool(cfgOptionInternal(optionId, varTypeBool, false)));
}

double
cfgOptionDbl(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(DOUBLE, varDbl(cfgOptionInternal(optionId, varTypeDouble, false)));
}

int
cfgOptionInt(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT, varIntForce(cfgOptionInternal(optionId, varTypeInt64, false)));
}

int64_t
cfgOptionInt64(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(INT64, varInt64(cfgOptionInternal(optionId, varTypeInt64, false)));
}

unsigned int
cfgOptionUInt(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT, varUIntForce(cfgOptionInternal(optionId, varTypeInt64, false)));
}

uint64_t
cfgOptionUInt64(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(UINT64, varUInt64Force(cfgOptionInternal(optionId, varTypeInt64, false)));
}

const KeyValue *
cfgOptionKv(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(KEY_VALUE, varKv(cfgOptionInternal(optionId, varTypeKeyValue, false)));
}

const VariantList *
cfgOptionLst(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    Variant *optionValue = cfgOptionInternal(optionId, varTypeVariantList, true);

    if (optionValue == NULL)
    {
        MEM_CONTEXT_BEGIN(configStatic.memContext)
        {
            optionValue = varNewVarLst(varLstNew());
            configStatic.option[optionId].value = optionValue;
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

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionInternal(optionId, varTypeString, false)));
}

const String *
cfgOptionStrNull(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN_CONST(STRING, varStr(cfgOptionInternal(optionId, varTypeString, true)));
}

void
cfgOptionSet(ConfigOption optionId, ConfigSource source, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(ENUM, source);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    MEM_CONTEXT_BEGIN(configStatic.memContext)
    {
        // Set the source
        configStatic.option[optionId].source = source;

        // Store old value
        Variant *valueOld = configStatic.option[optionId].value;

        // Only set value if it is not null
        if (value != NULL)
        {
            // If this option is in a group then set the value in the group for the option's index. This indicates that there is at
            // least one option set in the group for that index.

            if (configOptionData[optionId].group && source != cfgSourceDefault)
            {
                unsigned int groupId = configOptionData[optionId].groupId;

                configStatic.optionGroup[groupId].value |= (unsigned int)1 << configOptionData[optionId].index;

                if (configOptionData[optionId].index > configStatic.optionGroup[groupId].indexMax)
                    configStatic.optionGroup[groupId].indexMax = configOptionData[optionId].index;
            }

            switch (cfgDefOptionType(cfgOptionDefIdFromId(optionId)))
            {
                case cfgDefOptTypeBoolean:
                {
                    if (varType(value) == varTypeBool)
                        configStatic.option[optionId].value = varDup(value);
                    else
                        configStatic.option[optionId].value = varNewBool(varBoolForce(value));

                    break;
                }

                case cfgDefOptTypeFloat:
                {
                    if (varType(value) == varTypeDouble)
                        configStatic.option[optionId].value = varDup(value);
                    else
                        configStatic.option[optionId].value = varNewDbl(varDblForce(value));

                    break;
                }

                case cfgDefOptTypeInteger:
                case cfgDefOptTypeSize:
                {
                    if (varType(value) == varTypeInt64)
                        configStatic.option[optionId].value = varDup(value);
                    else
                        configStatic.option[optionId].value = varNewInt64(varInt64Force(value));

                    break;
                }

                case cfgDefOptTypeHash:
                {
                    if (varType(value) == varTypeKeyValue)
                        configStatic.option[optionId].value = varDup(value);
                    else
                        THROW_FMT(AssertError, "option '%s' must be set with KeyValue variant", cfgOptionName(optionId));

                    break;
                }

                case cfgDefOptTypeList:
                {
                    if (varType(value) == varTypeVariantList)
                        configStatic.option[optionId].value = varDup(value);
                    else
                        THROW_FMT(AssertError, "option '%s' must be set with VariantList variant", cfgOptionName(optionId));

                    break;
                }

                case cfgDefOptTypePath:
                case cfgDefOptTypeString:
                {
                    if (varType(value) == varTypeString)
                        configStatic.option[optionId].value = varDup(value);
                    else
                        THROW_FMT(AssertError, "option '%s' must be set with String variant", cfgOptionName(optionId));

                    break;
                }
            }
        }
        else
            configStatic.option[optionId].value = NULL;

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

    FUNCTION_TEST_RETURN(configStatic.option[optionId].source);
}

/**********************************************************************************************************************************/
bool
cfgOptionTest(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(cfgOptionValid(optionId) && configStatic.option[optionId].value != NULL);
}

/**********************************************************************************************************************************/
bool
cfgOptionValid(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configStatic.option[optionId].valid);
}

void
cfgOptionValidSet(ConfigOption optionId, bool valid)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(BOOL, valid);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    configStatic.option[optionId].valid = valid;

    // If this option is in a group then the group is also valid
    if (configOptionData[optionId].group)
        configStatic.optionGroup[configOptionData[optionId].groupId].valid = true;

    FUNCTION_TEST_RETURN_VOID();
}
