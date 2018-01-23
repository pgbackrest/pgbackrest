/***********************************************************************************************************************************
Command and Option Configuration Definition
***********************************************************************************************************************************/
#include <limits.h>
#include <string.h>

#include "common/error.h"
#include "config/define.h"

/***********************************************************************************************************************************
Map command names to ids and vice versa.
***********************************************************************************************************************************/
typedef struct ConfigDefineCommandData
{
    const char *name;                                               // Command name

    const char *helpSummary;                                        // Brief summary of the command
    const char *helpDescription;                                    // Full description of the command
} ConfigDefineCommandData;

// Command macros are intended to make the command definitions easy to read and to produce good diffs.
//----------------------------------------------------------------------------------------------------------------------------------
#define CFGDEFDATA_COMMAND_LIST(...)                                                                                               \
    {__VA_ARGS__};

#define CFGDEFDATA_COMMAND(...)                                                                                                    \
    {__VA_ARGS__},

#define CFGDEFDATA_COMMAND_NAME(nameParam)                                                                                         \
    .name = nameParam,

#define CFGDEFDATA_COMMAND_HELP_SUMMARY(helpSummaryParam)                                                                          \
    .helpSummary = helpSummaryParam,
#define CFGDEFDATA_COMMAND_HELP_DESCRIPTION(helpDescriptionParam)                                                                  \
    .helpDescription = helpDescriptionParam,

/***********************************************************************************************************************************
Define how an option is parsed and interacts with other options.
***********************************************************************************************************************************/
typedef struct ConfigDefineOptionData
{
    const char *name;                                               // Option name
    unsigned int type:3;                                            // Option type (e.g. string, int, boolean, etc.)
    unsigned int internal:1;                                        // Is the option only used internally?
    unsigned int indexTotal:4;                                      // 0 normally, > 0 if indexed option (e.g. db1-*)
    unsigned int section:2;                                         // Config section (e.g. global, stanza, cmd-line)
    bool negate:1;                                                  // Can the option be negated?
    bool required:1;                                                // Is the option required?
    bool secure:1;                                                  // Does the option need to be redacted on logs and cmd-line?
    unsigned int commandValid:15;                                   // Bitmap for commands that the option is valid for

    const char *helpSection;                                        // Classify the option
    const char *helpSummary;                                        // Brief summary of the option
    const char *helpDescription;                                    // Full description of the option

    const void **data;                                              // Optional data and command overrides
} ConfigDefineOptionData;

// Option macros are intended to make the command definitions easy to read and to produce good diffs.
//----------------------------------------------------------------------------------------------------------------------------------
#define CFGDEFDATA_OPTION_LIST(...)                                                                                                \
    {__VA_ARGS__};

#define CFGDEFDATA_OPTION(...)                                                                                                     \
    {__VA_ARGS__},

#define CFGDEFDATA_OPTION_NAME(nameParam)                                                                                          \
    .name = nameParam,
#define CFGDEFDATA_OPTION_INDEX_TOTAL(indexTotalParam)                                                                             \
    .indexTotal = indexTotalParam,
#define CFGDEFDATA_OPTION_INTERNAL(internalParam)                                                                                  \
    .internal = internalParam,
#define CFGDEFDATA_OPTION_NEGATE(negateParam)                                                                                      \
    .negate = negateParam,
#define CFGDEFDATA_OPTION_REQUIRED(requiredParam)                                                                                  \
    .required = requiredParam,
#define CFGDEFDATA_OPTION_SECTION(sectionParam)                                                                                    \
    .section = sectionParam,
#define CFGDEFDATA_OPTION_SECURE(secureParam)                                                                                      \
    .secure = secureParam,
#define CFGDEFDATA_OPTION_TYPE(typeParam)                                                                                          \
    .type = typeParam,

#define CFGDEFDATA_OPTION_HELP_SECTION(helpSectionParam)                                                                           \
    .helpSection = helpSectionParam,
#define CFGDEFDATA_OPTION_HELP_SUMMARY(helpSummaryParam)                                                                           \
    .helpSummary = helpSummaryParam,
#define CFGDEFDATA_OPTION_HELP_DESCRIPTION(helpDescriptionParam)                                                                   \
    .helpDescription = helpDescriptionParam,

// Define additional types of data that can be associated with an option.  Because these types are rare they are not give dedicated
// fields and are instead packed into an array which is read at runtime.  This may seem inefficient but they are only accessed a
// single time during parse so space efficiency is more important than performance.
//----------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
    configDefDataTypeEnd,                                          // Indicates there is no more data
    configDefDataTypeAllowList,
    configDefDataTypeAllowRange,
    configDefDataTypeCommand,
    configDefDataTypeDefault,
    configDefDataTypeDepend,
    configDefDataTypeNameAlt,
    configDefDataTypePrefix,
    configDefDataTypeRequired,
    configDefDataTypeHelpSummary,
    configDefDataTypeHelpDescription,
} ConfigDefineDataType;

#define CFGDATA_OPTION_OPTIONAL_PUSH_LIST(type, size, data, ...)                                                                   \
    (const void *)((uint32)type << 24 | (uint32)size << 16 | (uint32)data), __VA_ARGS__

#define CFGDATA_OPTION_OPTIONAL_PUSH(type, size, data)                                                                             \
    (const void *)((uint32)type << 24 | (uint32)size << 16 | (uint32)data)

#define CFGDEFDATA_OPTION_COMMAND_LIST(...)                                                                                        \
    .commandValid = 0 __VA_ARGS__,

#define CFGDEFDATA_OPTION_COMMAND(commandParam)                                                                                    \
    | (1 << commandParam)

#define CFGDEFDATA_OPTION_OPTIONAL_LIST(...)                                                                                       \
    .data = (const void *[]){__VA_ARGS__ NULL},

#define CFGDEFDATA_OPTION_OPTIONAL_DEFAULT(defaultValue)                                                                           \
    CFGDATA_OPTION_OPTIONAL_PUSH_LIST(configDefDataTypeDefault, 1, 0, defaultValue),

#define CFGDEFDATA_OPTION_OPTIONAL_ALLOW_LIST(...)                                                                                 \
    CFGDATA_OPTION_OPTIONAL_PUSH_LIST(                                                                                             \
        configDefDataTypeAllowList, sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *), 0, __VA_ARGS__),

#define CFGDEFDATA_OPTION_OPTIONAL_ALLOW_RANGE(rangeMinParam, rangeMaxParam)                                                       \
    CFGDATA_OPTION_OPTIONAL_PUSH_LIST(                                                                                             \
        configDefDataTypeAllowRange, 2, 0, (const void *)(intptr_t)(int32)(rangeMinParam * 100),                                   \
        (const void *)(intptr_t)(int32)(rangeMaxParam * 100)),

#define CFGDEFDATA_OPTION_OPTIONAL_NAME_ALT(nameAltParam)                                                                          \
    CFGDATA_OPTION_OPTIONAL_PUSH_LIST(configDefDataTypeNameAlt, 1, 0, nameAltParam),

#define CFGDEFDATA_OPTION_OPTIONAL_PREFIX(prefixParam)                                                                             \
    CFGDATA_OPTION_OPTIONAL_PUSH_LIST(configDefDataTypePrefix, 1, 0, prefixParam),

#define CFGDEFDATA_OPTION_OPTIONAL_DEPEND(optionDepend)                                                                            \
    CFGDATA_OPTION_OPTIONAL_PUSH(configDefDataTypeDepend, 0, optionDepend),

#define CFGDEFDATA_OPTION_OPTIONAL_DEPEND_LIST(optionDepend, ...)                                                                  \
    CFGDATA_OPTION_OPTIONAL_PUSH_LIST(                                                                                             \
        configDefDataTypeDepend, sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *), optionDepend, __VA_ARGS__),

#define CFGDEFDATA_OPTION_OPTIONAL_COMMAND_OVERRRIDE(...)                                                                          \
    __VA_ARGS__

#define CFGDEFDATA_OPTION_OPTIONAL_COMMAND(command)                                                                                \
    CFGDATA_OPTION_OPTIONAL_PUSH(configDefDataTypeCommand, 0, command),

#define CFGDEFDATA_OPTION_OPTIONAL_REQUIRED(commandOptionRequired)                                                                 \
    CFGDATA_OPTION_OPTIONAL_PUSH(configDefDataTypeRequired, 0, commandOptionRequired),

#define CFGDEFDATA_OPTION_OPTIONAL_HELP_SUMMARY(helpSummaryParam)                                                                  \
    CFGDATA_OPTION_OPTIONAL_PUSH_LIST(configDefDataTypeHelpSummary, 1, 0, helpSummaryParam),
#define CFGDEFDATA_OPTION_OPTIONAL_HELP_DESCRIPTION(helpDescriptionParam)                                                          \
    CFGDATA_OPTION_OPTIONAL_PUSH_LIST(configDefDataTypeHelpDescription, 1, 0, helpDescriptionParam),

/***********************************************************************************************************************************
Include the automatically generated configuration data.
***********************************************************************************************************************************/
#include "define.auto.c"

/***********************************************************************************************************************************
Find optional data for a command and option.
***********************************************************************************************************************************/
static void
cfgDefDataFind(
    ConfigDefineDataType typeFind, ConfigDefineCommand commandDefId, const void **dataList, bool *dataDefFound, int *dataDef,
    const void ***dataDefList, int *dataDefListSize)
{
    *dataDefFound = false;

    // Only proceed if there is data
    if (dataList != NULL)
    {
        ConfigDefineDataType type;
        int offset = 0;
        int size;
        int data;
        unsigned int commandCurrent = UINT_MAX;

        // Loop through all data
        do
        {
            // Extract data
            type = (ConfigDefineDataType)(((uintptr_t)dataList[offset] >> 24) & 0xFF);
            size = ((uintptr_t)dataList[offset] >> 16) & 0xFF;
            data = (uintptr_t)dataList[offset] & 0xFFFF;

            // If a command block then set the current command
            if (type == configDefDataTypeCommand)
            {
                // If data was not found in the expected command then there's nothing more to look for
                if (commandCurrent == commandDefId)
                    break;

                // Set the current command
                commandCurrent = data;
            }
            // Only find type if not in a command block yet or in the expected command
            else if (type == typeFind && (commandCurrent == UINT_MAX || commandCurrent == commandDefId))
            {
                // Store the data found
                *dataDefFound = true;
                *dataDef = data;
                *dataDefList = &dataList[offset + 1];
                *dataDefListSize = size;

                // If found in the expected command block then nothing more to look for
                if (commandCurrent == commandDefId)
                    break;
            }

            offset += size + 1;
        }
        while(type != configDefDataTypeEnd);
    }
}

#define CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, type)                                                                   \
    bool dataDefFound = false;                                                                                                     \
    int dataDef = 0;                                                                                                               \
    int dataDefListSize = 0;                                                                                                       \
    const void **dataDefList = NULL;                                                                                                     \
                                                                                                                                   \
    cfgDefDataFind(                                                                                                                \
        type, commandDefId, configDefineOptionData[optionDefId].data, &dataDefFound, &dataDef, &dataDefList, &dataDefListSize);

/***********************************************************************************************************************************
Command and option define totals
***********************************************************************************************************************************/
unsigned int
cfgDefCommandTotal()
{
    return sizeof(configDefineCommandData) / sizeof(ConfigDefineCommandData);
}

unsigned int
cfgDefOptionTotal()
{
    return sizeof(configDefineOptionData) / sizeof(ConfigDefineOptionData);
}

/***********************************************************************************************************************************
Check that command and option ids are valid
***********************************************************************************************************************************/
void
cfgDefCommandCheck(ConfigDefineCommand commandDefId)
{
    if (commandDefId >= cfgDefCommandTotal())
        THROW(AssertError, "command def id %d invalid - must be >= 0 and < %d", commandDefId, cfgDefCommandTotal());
}

void
cfgDefOptionCheck(ConfigDefineOption optionDefId)
{
    if (optionDefId >= cfgDefOptionTotal())
        THROW(AssertError, "option def id %d invalid - must be >= 0 and < %d", optionDefId, cfgDefOptionTotal());
}

static void
cfgDefCommandOptionCheck(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandCheck(commandDefId);
    cfgDefOptionCheck(optionDefId);
}

/***********************************************************************************************************************************
Command help description
***********************************************************************************************************************************/
const char *
cfgDefCommandHelpDescription(ConfigDefineCommand commandDefId)
{
    cfgDefCommandCheck(commandDefId);
    return configDefineCommandData[commandDefId].helpDescription;
}

/***********************************************************************************************************************************
Command help summary
***********************************************************************************************************************************/
const char *
cfgDefCommandHelpSummary(ConfigDefineCommand commandDefId)
{
    cfgDefCommandCheck(commandDefId);
    return configDefineCommandData[commandDefId].helpSummary;
}

/***********************************************************************************************************************************
Option allow lists
***********************************************************************************************************************************/
bool
cfgDefOptionAllowList(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowList);

    return dataDefFound;
}

const char *
cfgDefOptionAllowListValue(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, int valueId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowList);

    if (valueId < 0 || valueId >= dataDefListSize)
        THROW(AssertError, "value id %d invalid - must be >= 0 and < %d", valueId, dataDefListSize);

    return (char *)dataDefList[valueId];
}

int
cfgDefOptionAllowListValueTotal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowList);

    return dataDefListSize;
}

// Check if the value matches a value in the allow list
bool
cfgDefOptionAllowListValueValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, const char *value)
{
    if (value != NULL)
    {
        for (int valueIdx = 0; valueIdx < cfgDefOptionAllowListValueTotal(commandDefId, optionDefId); valueIdx++)
            if (strcmp(value, cfgDefOptionAllowListValue(commandDefId, optionDefId, valueIdx)) == 0)
                return true;
    }

    return false;
}

/***********************************************************************************************************************************
Allow range
***********************************************************************************************************************************/
bool
cfgDefOptionAllowRange(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowRange);

    return dataDefFound;
}

double
cfgDefOptionAllowRangeMax(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowRange);

    return (double)(intptr_t)dataDefList[1] / 100;
}

double
cfgDefOptionAllowRangeMin(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowRange);

    return (double)(intptr_t)dataDefList[0] / 100;
}

/***********************************************************************************************************************************
Default value for the option
***********************************************************************************************************************************/
const char *
cfgDefOptionDefault(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeDefault);

    if (dataDefFound)
        return (char *)dataDefList[0];

    return NULL;
}

/***********************************************************************************************************************************
Dependencies and depend lists
***********************************************************************************************************************************/
bool
cfgDefOptionDepend(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeDepend);

    return dataDefFound;
}

ConfigDefineOption
cfgDefOptionDependOption(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeDepend);

    return (ConfigDefineOption)dataDef;
}

const char *
cfgDefOptionDependValue(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, int valueId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeDepend);

    if (valueId < 0 || valueId >= dataDefListSize)
        THROW(AssertError, "value id %d invalid - must be >= 0 and < %d", valueId, dataDefListSize);

    return (char *)dataDefList[valueId];
}

int
cfgDefOptionDependValueTotal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeDepend);

    return dataDefListSize;
}

// Check if the value matches a value in the allow list
bool
cfgDefOptionDependValueValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, const char *value)
{
    if (value != NULL)
    {
        for (int valueIdx = 0; valueIdx < cfgDefOptionDependValueTotal(commandDefId, optionDefId); valueIdx++)
            if (strcmp(value, cfgDefOptionDependValue(commandDefId, optionDefId, valueIdx)) == 0)
                return true;
    }

    return false;
}

/***********************************************************************************************************************************
Option help description
***********************************************************************************************************************************/
const char *
cfgDefOptionHelpDescription(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeHelpDescription);

    if (dataDefFound)
        return (char *)dataDefList[0];

    return configDefineOptionData[optionDefId].helpDescription;
}

/***********************************************************************************************************************************
Option help section
***********************************************************************************************************************************/
const char *
cfgDefOptionHelpSection(ConfigDefineOption optionDefId)
{
    cfgDefOptionCheck(optionDefId);
    return configDefineOptionData[optionDefId].helpSection;
}

/***********************************************************************************************************************************
Option help summary
***********************************************************************************************************************************/
const char *
cfgDefOptionHelpSummary(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeHelpSummary);

    if (dataDefFound)
        return (char *)dataDefList[0];

    return configDefineOptionData[optionDefId].helpSummary;
}

/***********************************************************************************************************************************
Get total indexed values for option
***********************************************************************************************************************************/
int
cfgDefOptionIndexTotal(ConfigDefineOption optionDefId)
{
    cfgDefOptionCheck(optionDefId);
    return configDefineOptionData[optionDefId].indexTotal;
}

/***********************************************************************************************************************************
Is the option for internal use only?
***********************************************************************************************************************************/
bool
cfgDefOptionInternal(ConfigDefineOption optionDefId)
{
    cfgDefOptionCheck(optionDefId);
    return configDefineOptionData[optionDefId].internal;
}

/***********************************************************************************************************************************
Name of the option
***********************************************************************************************************************************/
const char *
cfgDefOptionName(ConfigDefineOption optionDefId)
{
    cfgDefOptionCheck(optionDefId);
    return configDefineOptionData[optionDefId].name;
}

/***********************************************************************************************************************************
Alternate name for the option -- generally used for deprecation
***********************************************************************************************************************************/
const char *
cfgDefOptionNameAlt(ConfigDefineOption optionDefId)
{
    cfgDefOptionCheck(optionDefId);

    CONFIG_DEFINE_DATA_FIND(-1, optionDefId, configDefDataTypeNameAlt);

    if (dataDefFound)
        return (char *)dataDefList[0];

    return NULL;
}

/***********************************************************************************************************************************
Can the option be negated?
***********************************************************************************************************************************/
bool
cfgDefOptionNegate(ConfigDefineOption optionDefId)
{
    cfgDefOptionCheck(optionDefId);
    return configDefineOptionData[optionDefId].negate;
}

/***********************************************************************************************************************************
Option prefix for indexed options
***********************************************************************************************************************************/
const char *
cfgDefOptionPrefix(ConfigDefineOption optionDefId)
{
    cfgDefOptionCheck(optionDefId);

    CONFIG_DEFINE_DATA_FIND(-1, optionDefId, configDefDataTypePrefix);

    if (dataDefFound)
        return (char *)dataDefList[0];

    return NULL;
}

/***********************************************************************************************************************************
Does the option need to be protected from showing up in logs, command lines, etc?
***********************************************************************************************************************************/
bool
cfgDefOptionSecure(ConfigDefineOption optionDefId)
{
    cfgDefOptionCheck(optionDefId);
    return configDefineOptionData[optionDefId].secure;
}

/***********************************************************************************************************************************
Is the option required
***********************************************************************************************************************************/
bool
cfgDefOptionRequired(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeRequired);

    if (dataDefFound)
        return (bool)dataDef;

    return configDefineOptionData[optionDefId].required;
}

/***********************************************************************************************************************************
Get option section
***********************************************************************************************************************************/
ConfigDefSection
cfgDefOptionSection(ConfigDefineOption optionDefId)
{
    cfgDefOptionCheck(optionDefId);
    return configDefineOptionData[optionDefId].section;
}

/***********************************************************************************************************************************
Get option data type
***********************************************************************************************************************************/
int
cfgDefOptionType(ConfigDefineOption optionDefId)
{
    cfgDefOptionCheck(optionDefId);
    return configDefineOptionData[optionDefId].type;
}

/***********************************************************************************************************************************
Is the option valid for the command?
***********************************************************************************************************************************/
bool
cfgDefOptionValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    cfgDefCommandOptionCheck(commandDefId, optionDefId);
    return configDefineOptionData[optionDefId].commandValid & (1 << commandDefId);
}
