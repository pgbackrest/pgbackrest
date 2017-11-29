/***********************************************************************************************************************************
Command and Option Configuration
***********************************************************************************************************************************/
#include <string.h>

#include "common/error.h"
#include "config/config.h"

/***********************************************************************************************************************************
Map command names to ids and vice versa.
***********************************************************************************************************************************/
typedef struct ConfigCommandData
{
    const char *name;
} ConfigCommandData;

#define CONFIG_COMMAND_LIST(...)                                                                                                   \
    {__VA_ARGS__};

#define CONFIG_COMMAND(...)                                                                                                        \
    {__VA_ARGS__},

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
Store the current command

This is generally set by the command parser but can also be set by during execute to change commands, i.e. backup -> expire.
***********************************************************************************************************************************/
ConfigCommand command = cfgCmdNone;

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
    if (commandId >= cfgCommandTotal())
        THROW(AssertError, "command id %d invalid - must be >= 0 and < %d", commandId, cfgCommandTotal());
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

    for (commandId = 0; commandId < cfgCommandTotal(); commandId++)
        if (strcmp(commandName, configCommandData[commandId].name) == 0)
            break;

    if (commandId == cfgCommandTotal())
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
cfgCommandTotal - total number of commands
***********************************************************************************************************************************/
unsigned int
cfgCommandTotal()
{
    return sizeof(configCommandData) / sizeof(ConfigCommandData);
}

/***********************************************************************************************************************************
Ensure that option id is valid
***********************************************************************************************************************************/
void
cfgOptionCheck(ConfigOption optionId)
{
    if (optionId >= cfgOptionTotal())
        THROW(AssertError, "option id %d invalid - must be >= 0 and < %d", optionId, cfgOptionTotal());
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
Get total indexed values for option
***********************************************************************************************************************************/
int
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
    for (ConfigOption optionId = 0; optionId < cfgOptionTotal(); optionId++)
        if (strcmp(optionName, configOptionData[optionId].name) == 0)
            return optionId;

    return -1;
}

/***********************************************************************************************************************************
Get total indexed values for option
***********************************************************************************************************************************/
int
cfgOptionIndexTotal(ConfigOption optionId)
{
    cfgOptionCheck(optionId);
    return cfgDefOptionIndexTotal(configOptionData[optionId].defineId);
}

/***********************************************************************************************************************************
Get the id for this option define
***********************************************************************************************************************************/
ConfigOption
cfgOptionIdFromDefId(ConfigDefineOption optionDefId, int index)
{
    // Search for the option
    ConfigOption optionId;

    for (optionId = 0; optionId < cfgOptionTotal(); optionId++)
        if (configOptionData[optionId].defineId == optionDefId)
            break;

    // Error when not found
    if (optionId == cfgOptionTotal())
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
cfgOptionTotal - total number of configuration options
***********************************************************************************************************************************/
unsigned int
cfgOptionTotal()
{
    return sizeof(configOptionData) / sizeof(ConfigOptionData);
}
