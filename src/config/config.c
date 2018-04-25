/***********************************************************************************************************************************
Command and Option Configuration
***********************************************************************************************************************************/
#include <string.h>

#include "common/assert.h"
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
    unsigned int lockType:2;

    bool logFile:1;
    unsigned int logLevelDefault:4;
    unsigned int logLevelStdErrMax:4;
} ConfigCommandData;

#define CONFIG_COMMAND_LIST(...)                                                                                                   \
    {__VA_ARGS__};

#define CONFIG_COMMAND(...)                                                                                                        \
    {__VA_ARGS__},

#define CONFIG_COMMAND_LOCK_REQUIRED(lockRequiredParam)                                                                            \
    .lockRequired = lockRequiredParam,
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
#include "config.auto.c"

/***********************************************************************************************************************************
Debug Asserts
***********************************************************************************************************************************/
// The command must be set
#define ASSERT_DEBUG_COMMAND_SET()                                                                                                 \
    ASSERT_DEBUG(cfgCommand() != cfgCmdNone)

/***********************************************************************************************************************************
Store the config memory context
***********************************************************************************************************************************/
MemContext *configMemContext = NULL;

/***********************************************************************************************************************************
Store the current command

This is generally set by the command parser but can also be set by during execute to change commands, i.e. backup -> expire.
***********************************************************************************************************************************/
ConfigCommand command = cfgCmdNone;

/***********************************************************************************************************************************
Store the location of the executable
***********************************************************************************************************************************/
String *exe = NULL;

/***********************************************************************************************************************************
Was help requested for the command?
***********************************************************************************************************************************/
bool help = false;

/***********************************************************************************************************************************
Store the list of parameters passed to the command
***********************************************************************************************************************************/
StringList *paramList = NULL;

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
cfgInit()
{
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
}

/***********************************************************************************************************************************
Get the current command
***********************************************************************************************************************************/
ConfigCommand
cfgCommand()
{
    return command;
}

/***********************************************************************************************************************************
Set the current command
***********************************************************************************************************************************/
void
cfgCommandSet(ConfigCommand commandParam)
{
    command = commandParam;
}

/***********************************************************************************************************************************
Ensure that command id is valid
***********************************************************************************************************************************/
void
cfgCommandCheck(ConfigCommand commandId)
{
    if (commandId >= CFG_COMMAND_TOTAL)
        THROW(AssertError, "command id %d invalid - must be >= 0 and < %d", commandId, CFG_COMMAND_TOTAL);
}

/***********************************************************************************************************************************
Was help requested?
***********************************************************************************************************************************/
bool
cfgCommandHelp()
{
    return help;
}

void
cfgCommandHelpSet(bool helpParam)
{
    help = helpParam;
}

/***********************************************************************************************************************************
Get the define id for this command

This can be done by just casting the id to the define id.  There may be a time when they are not one to one and this function can
be modified to do the mapping.
***********************************************************************************************************************************/
ConfigDefineCommand
cfgCommandDefIdFromId(ConfigCommand commandId)
{
    cfgCommandCheck(commandId);
    return (ConfigDefineCommand)commandId;
}

/***********************************************************************************************************************************
Get command id by name
***********************************************************************************************************************************/
int
cfgCommandId(const char *commandName)
{
    ConfigCommand commandId;

    for (commandId = 0; commandId < cfgCmdNone; commandId++)
        if (strcmp(commandName, configCommandData[commandId].name) == 0)
            break;

    if (commandId == cfgCmdNone)
        THROW(AssertError, "invalid command '%s'", commandName);

    return commandId;
}

/***********************************************************************************************************************************
Get command name by id
***********************************************************************************************************************************/
const char *
cfgCommandName(ConfigCommand commandId)
{
    cfgCommandCheck(commandId);
    return configCommandData[commandId].name;
}

/***********************************************************************************************************************************
Command parameters, if any
***********************************************************************************************************************************/
const StringList *
cfgCommandParam()
{
    if (paramList == NULL)
    {
        MEM_CONTEXT_BEGIN(configMemContext)
        {
            paramList = strLstNew();
        }
        MEM_CONTEXT_END();
    }

    return paramList;
}

void
cfgCommandParamSet(const StringList *param)
{
    MEM_CONTEXT_BEGIN(configMemContext)
    {
        paramList = strLstDup(param);
    }
    MEM_CONTEXT_END();

}

/***********************************************************************************************************************************
Command parameters, if any
***********************************************************************************************************************************/
const String *
cfgExe()
{
    return exe;
}

void
cfgExeSet(const String *exeParam)
{
    MEM_CONTEXT_BEGIN(configMemContext)
    {
        exe = strDup(exeParam);
    }
    MEM_CONTEXT_END();
}

/***********************************************************************************************************************************
Does this command require an immediate lock?
***********************************************************************************************************************************/
bool
cfgLockRequired()
{
    ASSERT_DEBUG_COMMAND_SET();
    return configCommandData[cfgCommand()].lockRequired;
}

/***********************************************************************************************************************************
Get the lock type required for this command
***********************************************************************************************************************************/
LockType
cfgLockType()
{
    ASSERT_DEBUG_COMMAND_SET();
    return (LockType)configCommandData[cfgCommand()].lockType;
}

/***********************************************************************************************************************************
Does this command log to a file?
***********************************************************************************************************************************/
bool
cfgLogFile()
{
    ASSERT_DEBUG_COMMAND_SET();
    return configCommandData[cfgCommand()].logFile;
}

/***********************************************************************************************************************************
Get default log level -- used for log messages that are common to all commands
***********************************************************************************************************************************/
LogLevel
cfgLogLevelDefault()
{
    ASSERT_DEBUG_COMMAND_SET();
    return (LogLevel)configCommandData[cfgCommand()].logLevelDefault;
}

/***********************************************************************************************************************************
Get max stderr log level -- used to suppress error output for higher log levels, e.g. local and remote commands
***********************************************************************************************************************************/
LogLevel
cfgLogLevelStdErrMax()
{
    ASSERT_DEBUG_COMMAND_SET();
    return (LogLevel)configCommandData[cfgCommand()].logLevelStdErrMax;
}

/***********************************************************************************************************************************
Ensure that option id is valid
***********************************************************************************************************************************/
void
cfgOptionCheck(ConfigOption optionId)
{
    if (optionId >= CFG_OPTION_TOTAL)
        THROW(AssertError, "option id %d invalid - must be >= 0 and < %d", optionId, CFG_OPTION_TOTAL);
}

/***********************************************************************************************************************************
Get the option define for this option
***********************************************************************************************************************************/
ConfigDefineOption
cfgOptionDefIdFromId(ConfigOption optionId)
{
    cfgOptionCheck(optionId);
    return configOptionData[optionId].defineId;
}

/***********************************************************************************************************************************
Get/set option default
***********************************************************************************************************************************/
const Variant *
cfgOptionDefault(ConfigOption optionId)
{
    cfgOptionCheck(optionId);

    if (configOptionValue[optionId].defaultValue == NULL)
    {
        ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

        if (cfgDefOptionDefault(cfgCommandDefIdFromId(cfgCommand()), optionDefId) != NULL)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                Variant *defaultValue = varNewStrZ(cfgDefOptionDefault(cfgCommandDefIdFromId(cfgCommand()), optionDefId));

                MEM_CONTEXT_BEGIN(configMemContext)
                {
                    switch (cfgDefOptionType(optionDefId))
                    {
                        case cfgDefOptTypeBoolean:
                        {
                            configOptionValue[optionId].defaultValue = varNewBool(varBoolForce(defaultValue));
                            break;
                        }

                        case cfgDefOptTypeFloat:
                        {
                            configOptionValue[optionId].defaultValue = varNewDbl(varDblForce(defaultValue));
                            break;
                        }

                        case cfgDefOptTypeInteger:
                        case cfgDefOptTypeSize:
                        {
                            configOptionValue[optionId].defaultValue = varNewInt64(varInt64Force(defaultValue));
                            break;
                        }

                        case cfgDefOptTypeString:
                            configOptionValue[optionId].defaultValue = varDup(defaultValue);
                            break;

                        default:
                            THROW(                                  // {uncoverable - others types do not have defaults yet}
                                AssertError, "type for option '%s' does not support defaults", cfgOptionName(optionId));
                    }
                }
                MEM_CONTEXT_END();
            }
            MEM_CONTEXT_TEMP_END();
        }
    }

    return configOptionValue[optionId].defaultValue;
}

void
cfgOptionDefaultSet(ConfigOption optionId, const Variant *defaultValue)
{
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
}

/***********************************************************************************************************************************
Get index for option
***********************************************************************************************************************************/
unsigned int
cfgOptionIndex(ConfigOption optionId)
{
    cfgOptionCheck(optionId);
    return configOptionData[optionId].index;
}

/***********************************************************************************************************************************
Get option id by name
***********************************************************************************************************************************/
int
cfgOptionId(const char *optionName)
{
    for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
        if (strcmp(optionName, configOptionData[optionId].name) == 0)
            return optionId;

    return -1;
}

/***********************************************************************************************************************************
Get total indexed values for option
***********************************************************************************************************************************/
unsigned int
cfgOptionIndexTotal(ConfigOption optionId)
{
    cfgOptionCheck(optionId);
    return cfgDefOptionIndexTotal(configOptionData[optionId].defineId);
}

/***********************************************************************************************************************************
Get the id for this option define
***********************************************************************************************************************************/
ConfigOption
cfgOptionIdFromDefId(ConfigDefineOption optionDefId, unsigned int index)
{
    // Search for the option
    ConfigOption optionId;

    for (optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
        if (configOptionData[optionId].defineId == optionDefId)
            break;

    // Error when not found
    if (optionId == CFG_OPTION_TOTAL)
        cfgDefOptionCheck(optionDefId);

    // Return with original index
    return optionId + index;
}

/***********************************************************************************************************************************
Get option name by id
***********************************************************************************************************************************/
const char *
cfgOptionName(ConfigOption optionId)
{
    cfgOptionCheck(optionId);
    return configOptionData[optionId].name;
}

/***********************************************************************************************************************************
Was the option negated?
***********************************************************************************************************************************/
bool
cfgOptionNegate(ConfigOption optionId)
{
    cfgOptionCheck(optionId);
    return configOptionValue[optionId].negate;
}

void
cfgOptionNegateSet(ConfigOption optionId, bool negate)
{
    cfgOptionCheck(optionId);
    configOptionValue[optionId].negate = negate;
}

/***********************************************************************************************************************************
Was the option reset?
***********************************************************************************************************************************/
bool
cfgOptionReset(ConfigOption optionId)
{
    cfgOptionCheck(optionId);
    return configOptionValue[optionId].reset;
}

void
cfgOptionResetSet(ConfigOption optionId, bool reset)
{
    cfgOptionCheck(optionId);
    configOptionValue[optionId].reset = reset;
}

/***********************************************************************************************************************************
Get and set config options
***********************************************************************************************************************************/
const Variant *
cfgOption(ConfigOption optionId)
{
    cfgOptionCheck(optionId);
    return configOptionValue[optionId].value;
}

bool
cfgOptionBool(ConfigOption optionId)
{
    cfgOptionCheck(optionId);

    if (varType(configOptionValue[optionId].value) != varTypeBool)
        THROW(AssertError, "option '%s' is not type 'bool'", cfgOptionName(optionId));

    return varBool(configOptionValue[optionId].value);
}

double
cfgOptionDbl(ConfigOption optionId)
{
    cfgOptionCheck(optionId);

    if (varType(configOptionValue[optionId].value) != varTypeDouble)
        THROW(AssertError, "option '%s' is not type 'double'", cfgOptionName(optionId));

    return varDbl(configOptionValue[optionId].value);
}

int
cfgOptionInt(ConfigOption optionId)
{
    cfgOptionCheck(optionId);

    if (varType(configOptionValue[optionId].value) != varTypeInt64)
        THROW(AssertError, "option '%s' is not type 'int64'", cfgOptionName(optionId));

    return varIntForce(configOptionValue[optionId].value);
}

int64_t
cfgOptionInt64(ConfigOption optionId)
{
    cfgOptionCheck(optionId);

    if (varType(configOptionValue[optionId].value) != varTypeInt64)
        THROW(AssertError, "option '%s' is not type 'int64'", cfgOptionName(optionId));

    return varInt64(configOptionValue[optionId].value);
}

const KeyValue *
cfgOptionKv(ConfigOption optionId)
{
    cfgOptionCheck(optionId);

    if (varType(configOptionValue[optionId].value) != varTypeKeyValue)
        THROW(AssertError, "option '%s' is not type 'KeyValue'", cfgOptionName(optionId));

    return varKv(configOptionValue[optionId].value);
}

const VariantList *
cfgOptionLst(ConfigOption optionId)
{
    cfgOptionCheck(optionId);

    if (configOptionValue[optionId].value == NULL)
    {
        MEM_CONTEXT_BEGIN(configMemContext)
        {
            configOptionValue[optionId].value = varNewVarLst(varLstNew());
        }
        MEM_CONTEXT_END();
    }
    else if (varType(configOptionValue[optionId].value) != varTypeVariantList)
        THROW(AssertError, "option '%s' is not type 'VariantList'", cfgOptionName(optionId));

    return varVarLst(configOptionValue[optionId].value);
}

const String *
cfgOptionStr(ConfigOption optionId)
{
    cfgOptionCheck(optionId);

    const String *result = NULL;

    if (configOptionValue[optionId].value != NULL)
    {
        if (varType(configOptionValue[optionId].value) != varTypeString)
            THROW(AssertError, "option '%s' is not type 'String'", cfgOptionName(optionId));

        result = varStr(configOptionValue[optionId].value);
    }

    return result;
}

void
cfgOptionSet(ConfigOption optionId, ConfigSource source, const Variant *value)
{
    cfgOptionCheck(optionId);

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
                        THROW(AssertError, "option '%s' must be set with KeyValue variant", cfgOptionName(optionId));

                    break;
                }

                case cfgDefOptTypeList:
                {
                    if (varType(value) == varTypeVariantList)
                        configOptionValue[optionId].value = varDup(value);
                    else
                        THROW(AssertError, "option '%s' must be set with VariantList variant", cfgOptionName(optionId));

                    break;
                }

                case cfgDefOptTypeString:
                    if (varType(value) == varTypeString)
                        configOptionValue[optionId].value = varDup(value);
                    else
                        THROW(AssertError, "option '%s' must be set with String variant", cfgOptionName(optionId));

                    break;
            }
        }
        else
            configOptionValue[optionId].value = NULL;

        // Free old value
        if (valueOld != NULL)
            varFree(valueOld);
    }
    MEM_CONTEXT_END();
}

/***********************************************************************************************************************************
How was the option set (default, param, config)?
***********************************************************************************************************************************/
ConfigSource
cfgOptionSource(ConfigOption optionId)
{
    cfgOptionCheck(optionId);
    return configOptionValue[optionId].source;
}

/***********************************************************************************************************************************
Is the option valid for the command and set?
***********************************************************************************************************************************/
bool
cfgOptionTest(ConfigOption optionId)
{
    cfgOptionCheck(optionId);
    return cfgOptionValid(optionId) && configOptionValue[optionId].value != NULL;
}

/***********************************************************************************************************************************
Is the option valid for this command?
***********************************************************************************************************************************/
bool
cfgOptionValid(ConfigOption optionId)
{
    cfgOptionCheck(optionId);
    return configOptionValue[optionId].valid;
}

void
cfgOptionValidSet(ConfigOption optionId, bool valid)
{
    cfgOptionCheck(optionId);
    configOptionValue[optionId].valid = valid;
}
