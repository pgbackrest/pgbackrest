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
Map command names to ids and vice versa.
***********************************************************************************************************************************/
typedef struct ConfigCommandData
{
    const char *name;

    bool lockRequired:1;
    bool lockRemoteRequired:1;
    unsigned int lockType:2;

    bool logFile:1;
    unsigned int logLevelDefault:4;
    unsigned int logLevelStdErrMax:4;

    bool parameterAllowed:1;
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
#define CONFIG_COMMAND_LOG_LEVEL_STDERR_MAX(logLevelStdErrMaxParam)                                                                \
    .logLevelStdErrMax = logLevelStdErrMaxParam,
#define CONFIG_COMMAND_NAME(nameParam)                                                                                             \
    .name = nameParam,
#define CONFIG_COMMAND_PARAMETER_ALLOWED(parameterAllowedParam)                                                                    \
    .parameterAllowed = parameterAllowedParam,

/***********************************************************************************************************************************
Map options names and indexes to option definitions.
***********************************************************************************************************************************/
typedef struct ConfigOptionData
{
    const char *name;

    unsigned int index:5;
    unsigned int defineId:7;
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

/***********************************************************************************************************************************
Include the automatically generated configuration data
***********************************************************************************************************************************/
#include "config/config.auto.c"

/***********************************************************************************************************************************
Store the config memory context
***********************************************************************************************************************************/
static MemContext *configMemContext = NULL;

/***********************************************************************************************************************************
Store the current command

This is generally set by the command parser but can also be set by during execute to change commands, i.e. backup -> expire.
***********************************************************************************************************************************/
static ConfigCommand command = cfgCmdNone;

/***********************************************************************************************************************************
Store the location of the executable
***********************************************************************************************************************************/
static String *exe = NULL;

/***********************************************************************************************************************************
Was help requested for the command?
***********************************************************************************************************************************/
static bool help = false;

/***********************************************************************************************************************************
Store the list of parameters passed to the command
***********************************************************************************************************************************/
static StringList *paramList = NULL;

/***********************************************************************************************************************************
Map options names and indexes to option definitions.
***********************************************************************************************************************************/
typedef struct ConfigOptionValue
{
    bool valid:1;
    bool negate:1;
    bool reset:1;
    unsigned int source:2;

    Variant *value;
    Variant *defaultValue;
} ConfigOptionValue;

static ConfigOptionValue configOptionValue[CFG_OPTION_TOTAL];

/***********************************************************************************************************************************
Initialize or reinitialize the configuration data
***********************************************************************************************************************************/
void
cfgInit(void)
{
    FUNCTION_TEST_VOID();

    // Reset configuration
    command = cfgCmdNone;
    exe = NULL;
    help = false;
    paramList = NULL;
    memset(&configOptionValue, 0, sizeof(configOptionValue));

    // Free the old context
    if (configMemContext != NULL)
    {
        memContextFree(configMemContext);
        configMemContext = NULL;
    }

    // Allocate configuration context as a child of the top context
    MEM_CONTEXT_BEGIN(memContextTop())
    {
        MEM_CONTEXT_NEW_BEGIN("configuration")
        {
            configMemContext = MEM_CONTEXT_NEW();
        }
        MEM_CONTEXT_NEW_END();
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get/set the current command
***********************************************************************************************************************************/
ConfigCommand
cfgCommand(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(command);
}

void
cfgCommandSet(ConfigCommand commandParam)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandParam);
    FUNCTION_TEST_END();

    ASSERT(commandParam <= cfgCmdNone);

    command = commandParam;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Was help requested?
***********************************************************************************************************************************/
bool
cfgCommandHelp(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(help);
}

void
cfgCommandHelpSet(bool helpParam)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, helpParam);
    FUNCTION_TEST_END();

    help = helpParam;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get the define id for this command

This can be done by just casting the id to the define id.  There may be a time when they are not one to one and this function can
be modified to do the mapping.
***********************************************************************************************************************************/
ConfigDefineCommand
cfgCommandDefIdFromId(ConfigCommand commandId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
    FUNCTION_TEST_END();

    ASSERT(commandId < cfgCmdNone);

    FUNCTION_TEST_RETURN((ConfigDefineCommand)commandId);
}

/***********************************************************************************************************************************
Get command id by name
***********************************************************************************************************************************/
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

    if (commandId == cfgCmdNone)
        THROW_FMT(AssertError, "invalid command '%s'", commandName);

    FUNCTION_TEST_RETURN(commandId);
}

/***********************************************************************************************************************************
Get command name by id
***********************************************************************************************************************************/
const char *
cfgCommandName(ConfigCommand commandId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
    FUNCTION_TEST_END();

    ASSERT(commandId < cfgCmdNone);

    FUNCTION_TEST_RETURN(configCommandData[commandId].name);
}

/***********************************************************************************************************************************
Command parameters, if any
***********************************************************************************************************************************/
const StringList *
cfgCommandParam(void)
{
    FUNCTION_TEST_VOID();

    if (paramList == NULL)
    {
        MEM_CONTEXT_BEGIN(configMemContext)
        {
            paramList = strLstNew();
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(paramList);
}

void
cfgCommandParamSet(const StringList *param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, param);
    FUNCTION_TEST_END();

    ASSERT(param != NULL);

    MEM_CONTEXT_BEGIN(configMemContext)
    {
        paramList = strLstDup(param);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Command parameters, if any
***********************************************************************************************************************************/
const String *
cfgExe(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(exe);
}

void
cfgExeSet(const String *exeParam)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, exeParam);
    FUNCTION_TEST_END();

    ASSERT(exeParam != NULL);

    MEM_CONTEXT_BEGIN(configMemContext)
    {
        exe = strDup(exeParam);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Does this command require an immediate lock?
***********************************************************************************************************************************/
bool
cfgLockRequired(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(command != cfgCmdNone);

    FUNCTION_TEST_RETURN(configCommandData[cfgCommand()].lockRequired);
}

/***********************************************************************************************************************************
Does the command require an immediate lock?
***********************************************************************************************************************************/
bool
cfgLockRemoteRequired(ConfigCommand commandId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
    FUNCTION_TEST_END();

    ASSERT(commandId < cfgCmdNone);

    FUNCTION_TEST_RETURN(configCommandData[commandId].lockRemoteRequired);
}

/***********************************************************************************************************************************
Get the lock type required for this command
***********************************************************************************************************************************/
LockType
cfgLockType(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(command != cfgCmdNone);

    FUNCTION_TEST_RETURN((LockType)configCommandData[cfgCommand()].lockType);
}

/***********************************************************************************************************************************
Get the remote lock type required for the command
***********************************************************************************************************************************/
LockType
cfgLockRemoteType(ConfigCommand commandId)
{
    FUNCTION_TEST_VOID();

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
    FUNCTION_TEST_END();

    ASSERT(commandId < cfgCmdNone);

    FUNCTION_TEST_RETURN((LockType)configCommandData[commandId].lockType);
}

/***********************************************************************************************************************************
Does this command log to a file?
***********************************************************************************************************************************/
bool
cfgLogFile(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(command != cfgCmdNone);

    FUNCTION_TEST_RETURN(configCommandData[cfgCommand()].logFile);
}

/***********************************************************************************************************************************
Get default log level -- used for log messages that are common to all commands
***********************************************************************************************************************************/
LogLevel
cfgLogLevelDefault(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(command != cfgCmdNone);

    FUNCTION_TEST_RETURN((LogLevel)configCommandData[cfgCommand()].logLevelDefault);
}

/***********************************************************************************************************************************
Get max stderr log level -- used to suppress error output for higher log levels, e.g. local and remote commands
***********************************************************************************************************************************/
LogLevel
cfgLogLevelStdErrMax(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(command != cfgCmdNone);

    FUNCTION_TEST_RETURN((LogLevel)configCommandData[cfgCommand()].logLevelStdErrMax);
}

/***********************************************************************************************************************************
Does this command allow parameters?
***********************************************************************************************************************************/
bool
cfgParameterAllowed(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(command != cfgCmdNone);

    FUNCTION_TEST_RETURN(configCommandData[cfgCommand()].parameterAllowed);
}

/***********************************************************************************************************************************
Get the option define for this option
***********************************************************************************************************************************/
ConfigDefineOption
cfgOptionDefIdFromId(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configOptionData[optionId].defineId);
}

/***********************************************************************************************************************************
Get/set option default

Option defaults are generally not set in advance because the vast majority of them are never used.  It is more efficient to generate
them when they are requested.

Some defaults are (e.g. the exe path) are set at runtime.
***********************************************************************************************************************************/
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

    if (configOptionValue[optionId].defaultValue == NULL)
    {
        ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

        if (cfgDefOptionDefault(cfgCommandDefIdFromId(cfgCommand()), optionDefId) != NULL)
        {
            MEM_CONTEXT_BEGIN(configMemContext)
            {
                configOptionValue[optionId].defaultValue = cfgOptionDefaultValue(optionDefId);
            }
            MEM_CONTEXT_END();
        }
    }

    FUNCTION_TEST_RETURN(configOptionValue[optionId].defaultValue);
}

void
cfgOptionDefaultSet(ConfigOption optionId, const Variant *defaultValue)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(VARIANT, defaultValue);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    MEM_CONTEXT_BEGIN(configMemContext)
    {
        if (configOptionValue[optionId].defaultValue != NULL)
            varFree(configOptionValue[optionId].defaultValue);

        configOptionValue[optionId].defaultValue = varDup(defaultValue);

        if (configOptionValue[optionId].source == cfgSourceDefault)
        {
            if (configOptionValue[optionId].value != NULL)
                varFree(configOptionValue[optionId].value);

            configOptionValue[optionId].value = varDup(defaultValue);
        }
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Parse a host option and extract the host and port (if it exists)
***********************************************************************************************************************************/
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
                        strPtr(host), cfgOptionName(optionId));
                }

                // Set the host
                memContextSwitch(MEM_CONTEXT_OLD());
                result = strDup(strLstGet(hostPart, 0));
                memContextSwitch(MEM_CONTEXT_TEMP());

                // Set the port and error if it is not a positive integer
                TRY_BEGIN()
                {
                    *port = cvtZToUInt(strPtr(strLstGet(hostPart, 1)));
                }
                CATCH(FormatError)
                {
                    THROW_FMT(
                        OptionInvalidError,
                        "'%s' is not valid for option '%s'"
                            "\nHINT: port is not a positive integer.",
                        strPtr(host), cfgOptionName(optionId));
                }
                TRY_END();
            }
            // Else there is no port and just copy the host
            else
            {
                memContextSwitch(MEM_CONTEXT_OLD());
                result = strDup(host);
                memContextSwitch(MEM_CONTEXT_TEMP());
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get index for option
***********************************************************************************************************************************/
unsigned int
cfgOptionIndex(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configOptionData[optionId].index);
}

/***********************************************************************************************************************************
Get option id by name
***********************************************************************************************************************************/
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
Get total indexed values for option
***********************************************************************************************************************************/
unsigned int
cfgOptionIndexTotal(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(cfgDefOptionIndexTotal(configOptionData[optionId].defineId));
}

/***********************************************************************************************************************************
Get the id for this option define
***********************************************************************************************************************************/
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

/***********************************************************************************************************************************
Was the option negated?
***********************************************************************************************************************************/
bool
cfgOptionNegate(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configOptionValue[optionId].negate);
}

void
cfgOptionNegateSet(ConfigOption optionId, bool negate)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(BOOL, negate);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    configOptionValue[optionId].negate = negate;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Was the option reset?
***********************************************************************************************************************************/
bool
cfgOptionReset(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configOptionValue[optionId].reset);
}

void
cfgOptionResetSet(ConfigOption optionId, bool reset)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(BOOL, reset);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    configOptionValue[optionId].reset = reset;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get and set config options
***********************************************************************************************************************************/
const Variant *
cfgOption(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configOptionValue[optionId].value);
}

bool
cfgOptionBool(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(varType(configOptionValue[optionId].value) == varTypeBool);

    FUNCTION_LOG_RETURN(BOOL, varBool(configOptionValue[optionId].value));
}

double
cfgOptionDbl(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(varType(configOptionValue[optionId].value) == varTypeDouble);

    FUNCTION_LOG_RETURN(DOUBLE, varDbl(configOptionValue[optionId].value));
}

int
cfgOptionInt(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(varType(configOptionValue[optionId].value) == varTypeInt64);

    FUNCTION_LOG_RETURN(INT, varIntForce(configOptionValue[optionId].value));
}

int64_t
cfgOptionInt64(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(varType(configOptionValue[optionId].value) == varTypeInt64);

    FUNCTION_LOG_RETURN(INT64, varInt64(configOptionValue[optionId].value));
}

unsigned int
cfgOptionUInt(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(varType(configOptionValue[optionId].value) == varTypeInt64);

    FUNCTION_LOG_RETURN(UINT, varUIntForce(configOptionValue[optionId].value));
}

uint64_t
cfgOptionUInt64(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(varType(configOptionValue[optionId].value) == varTypeInt64);

    FUNCTION_LOG_RETURN(UINT64, varUInt64Force(configOptionValue[optionId].value));
}

const KeyValue *
cfgOptionKv(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(varType(configOptionValue[optionId].value) == varTypeKeyValue);

    FUNCTION_LOG_RETURN(KEY_VALUE, varKv(configOptionValue[optionId].value));
}

const VariantList *
cfgOptionLst(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configOptionValue[optionId].value == NULL || varType(configOptionValue[optionId].value) == varTypeVariantList);

    if (configOptionValue[optionId].value == NULL)
    {
        MEM_CONTEXT_BEGIN(configMemContext)
        {
            configOptionValue[optionId].value = varNewVarLst(varLstNew());
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(VARIANT_LIST, varVarLst(configOptionValue[optionId].value));
}

const String *
cfgOptionStr(ConfigOption optionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, optionId);
    FUNCTION_LOG_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT(configOptionValue[optionId].value == NULL || varType(configOptionValue[optionId].value) == varTypeString);

    const String *result = NULL;

    if (configOptionValue[optionId].value != NULL)
        result = varStr(configOptionValue[optionId].value);

    FUNCTION_LOG_RETURN_CONST(STRING, result);
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

    MEM_CONTEXT_BEGIN(configMemContext)
    {
        // Set the source
        configOptionValue[optionId].source = source;

        // Store old value
        Variant *valueOld = configOptionValue[optionId].value;

        // Only set value if it is not null
        if (value != NULL)
        {
            switch (cfgDefOptionType(cfgOptionDefIdFromId(optionId)))
            {
                case cfgDefOptTypeBoolean:
                {
                    if (varType(value) == varTypeBool)
                        configOptionValue[optionId].value = varDup(value);
                    else
                        configOptionValue[optionId].value = varNewBool(varBoolForce(value));

                    break;
                }

                case cfgDefOptTypeFloat:
                {
                    if (varType(value) == varTypeDouble)
                        configOptionValue[optionId].value = varDup(value);
                    else
                        configOptionValue[optionId].value = varNewDbl(varDblForce(value));

                    break;
                }

                case cfgDefOptTypeInteger:
                case cfgDefOptTypeSize:
                {
                    if (varType(value) == varTypeInt64)
                        configOptionValue[optionId].value = varDup(value);
                    else
                        configOptionValue[optionId].value = varNewInt64(varInt64Force(value));

                    break;
                }

                case cfgDefOptTypeHash:
                {
                    if (varType(value) == varTypeKeyValue)
                        configOptionValue[optionId].value = varDup(value);
                    else
                        THROW_FMT(AssertError, "option '%s' must be set with KeyValue variant", cfgOptionName(optionId));

                    break;
                }

                case cfgDefOptTypeList:
                {
                    if (varType(value) == varTypeVariantList)
                        configOptionValue[optionId].value = varDup(value);
                    else
                        THROW_FMT(AssertError, "option '%s' must be set with VariantList variant", cfgOptionName(optionId));

                    break;
                }

                case cfgDefOptTypePath:
                case cfgDefOptTypeString:
                {
                    if (varType(value) == varTypeString)
                        configOptionValue[optionId].value = varDup(value);
                    else
                        THROW_FMT(AssertError, "option '%s' must be set with String variant", cfgOptionName(optionId));

                    break;
                }
            }
        }
        else
            configOptionValue[optionId].value = NULL;

        // Free old value
        if (valueOld != NULL)
            varFree(valueOld);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
How was the option set (default, param, config)?
***********************************************************************************************************************************/
ConfigSource
cfgOptionSource(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configOptionValue[optionId].source);
}

/***********************************************************************************************************************************
Is the option valid for the command and set?
***********************************************************************************************************************************/
bool
cfgOptionTest(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(cfgOptionValid(optionId) && configOptionValue[optionId].value != NULL);
}

/***********************************************************************************************************************************
Is the option valid for this command?
***********************************************************************************************************************************/
bool
cfgOptionValid(ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(configOptionValue[optionId].valid);
}

void
cfgOptionValidSet(ConfigOption optionId, bool valid)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(BOOL, valid);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    configOptionValue[optionId].valid = valid;

    FUNCTION_TEST_RETURN_VOID();
}
