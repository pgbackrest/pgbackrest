/***********************************************************************************************************************************
Command and Option Configuration Definition
***********************************************************************************************************************************/
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "common/debug.h"
#include "common/error.h"
#include "config/define.h"

/***********************************************************************************************************************************
Define global section name
***********************************************************************************************************************************/
STRING_EXTERN(CFGDEF_SECTION_GLOBAL_STR,                            CFGDEF_SECTION_GLOBAL);

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
    unsigned int indexTotal:4;                                      // 0 normally, > 0 if indexed option (e.g. pg1-*)
    unsigned int section:2;                                         // Config section (e.g. global, stanza, cmd-line)
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
    configDefDataTypeInternal,
    configDefDataTypePrefix,
    configDefDataTypeRequired,
    configDefDataTypeHelpNameAlt,
    configDefDataTypeHelpSummary,
    configDefDataTypeHelpDescription,
} ConfigDefineDataType;

#define CFGDATA_OPTION_OPTIONAL_PUSH_LIST(type, size, data, ...)                                                                   \
    (const void *)((uint32_t)type << 24 | (uint32_t)size << 16 | (uint32_t)data), __VA_ARGS__

#define CFGDATA_OPTION_OPTIONAL_PUSH(type, size, data)                                                                             \
    (const void *)((uint32_t)type << 24 | (uint32_t)size << 16 | (uint32_t)data)

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
        configDefDataTypeAllowRange, 4, 0,                                                                                         \
        (const void *)(intptr_t)(int32_t)(((int64_t)((double)rangeMinParam * 100)) % 1000000000L),                                 \
        (const void *)(intptr_t)(int32_t)(((int64_t)((double)rangeMinParam * 100)) / 1000000000L),                                 \
        (const void *)(intptr_t)(int32_t)(((int64_t)((double)rangeMaxParam * 100)) % 1000000000L),                                 \
        (const void *)(intptr_t)(int32_t)(((int64_t)((double)rangeMaxParam * 100)) / 1000000000L)),

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

#define CFGDEFDATA_OPTION_OPTIONAL_INTERNAL(commandOptionInternal)                                                                 \
    CFGDATA_OPTION_OPTIONAL_PUSH(configDefDataTypeInternal, 0, commandOptionInternal),

#define CFGDEFDATA_OPTION_OPTIONAL_REQUIRED(commandOptionRequired)                                                                 \
    CFGDATA_OPTION_OPTIONAL_PUSH(configDefDataTypeRequired, 0, commandOptionRequired),

#define CFGDEFDATA_OPTION_OPTIONAL_HELP_NAME_ALT(...)                                                                              \
    CFGDATA_OPTION_OPTIONAL_PUSH_LIST(                                                                                             \
        configDefDataTypeHelpNameAlt, sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *), 0, __VA_ARGS__),
#define CFGDEFDATA_OPTION_OPTIONAL_HELP_SUMMARY(helpSummaryParam)                                                                  \
    CFGDATA_OPTION_OPTIONAL_PUSH_LIST(configDefDataTypeHelpSummary, 1, 0, helpSummaryParam),
#define CFGDEFDATA_OPTION_OPTIONAL_HELP_DESCRIPTION(helpDescriptionParam)                                                          \
    CFGDATA_OPTION_OPTIONAL_PUSH_LIST(configDefDataTypeHelpDescription, 1, 0, helpDescriptionParam),

/***********************************************************************************************************************************
Include the automatically generated configuration data.
***********************************************************************************************************************************/
#include "config/define.auto.c"

/***********************************************************************************************************************************
Find optional data for a command and option.
***********************************************************************************************************************************/
static void
cfgDefDataFind(
    ConfigDefineDataType typeFind, ConfigDefineCommand commandDefId, const void **dataList, bool *dataDefFound, int *dataDef,
    const void ***dataDefList, unsigned int *dataDefListSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, typeFind);
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(VOIDPP, dataList);
        FUNCTION_TEST_PARAM(BOOLP, dataDefFound);
        FUNCTION_TEST_PARAM(INTP, dataDef);
        FUNCTION_TEST_PARAM(VOIDPP, dataDefList);
        FUNCTION_TEST_PARAM(UINTP, dataDefListSize);
    FUNCTION_TEST_END();

    ASSERT(dataDefFound != NULL);
    ASSERT(dataDef != NULL);
    ASSERT(dataDefList != NULL);
    ASSERT(dataDefListSize != NULL);

    *dataDefFound = false;

    // Only proceed if there is data
    if (dataList != NULL)
    {
        ConfigDefineDataType type;
        unsigned int offset = 0;
        unsigned int size;
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
                commandCurrent = (unsigned int)data;
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

    FUNCTION_TEST_RETURN_VOID();
}

#define CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, type)                                                                   \
    bool dataDefFound = false;                                                                                                     \
    int dataDef = 0;                                                                                                               \
    unsigned int dataDefListSize = 0;                                                                                              \
    const void **dataDefList = NULL;                                                                                               \
                                                                                                                                   \
    cfgDefDataFind(                                                                                                                \
        type, commandDefId, configDefineOptionData[optionDefId].data, &dataDefFound, &dataDef, &dataDefList, &dataDefListSize);

/***********************************************************************************************************************************
Command and option define totals
***********************************************************************************************************************************/
unsigned int
cfgDefCommandTotal(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(UINT, sizeof(configDefineCommandData) / sizeof(ConfigDefineCommandData));
}

unsigned int
cfgDefOptionTotal(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(UINT, sizeof(configDefineOptionData) / sizeof(ConfigDefineOptionData));
}

/***********************************************************************************************************************************
Command help description
***********************************************************************************************************************************/
const char *
cfgDefCommandHelpDescription(ConfigDefineCommand commandDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());

    FUNCTION_TEST_RETURN(STRINGZ, configDefineCommandData[commandDefId].helpDescription);
}

/***********************************************************************************************************************************
Command help summary
***********************************************************************************************************************************/
const char *
cfgDefCommandHelpSummary(ConfigDefineCommand commandDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());

    FUNCTION_TEST_RETURN(STRINGZ, configDefineCommandData[commandDefId].helpSummary);
}

/***********************************************************************************************************************************
Option allow lists
***********************************************************************************************************************************/
bool
cfgDefOptionAllowList(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowList);

    FUNCTION_TEST_RETURN(BOOL, dataDefFound);
}

const char *
cfgDefOptionAllowListValue(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, unsigned int valueId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
        FUNCTION_TEST_PARAM(UINT, valueId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());
    ASSERT(valueId < cfgDefOptionAllowListValueTotal(commandDefId, optionDefId));

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowList);

    FUNCTION_TEST_RETURN(STRINGZ, (char *)dataDefList[valueId]);
}

unsigned int
cfgDefOptionAllowListValueTotal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowList);

    FUNCTION_TEST_RETURN(UINT, dataDefListSize);
}

// Check if the value matches a value in the allow list
bool
cfgDefOptionAllowListValueValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());
    ASSERT(value != NULL);

    bool result = false;

    for (unsigned int valueIdx = 0; valueIdx < cfgDefOptionAllowListValueTotal(commandDefId, optionDefId); valueIdx++)
    {
        if (strcmp(value, cfgDefOptionAllowListValue(commandDefId, optionDefId, valueIdx)) == 0)
            result = true;
    }

    FUNCTION_TEST_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Allow range
***********************************************************************************************************************************/
bool
cfgDefOptionAllowRange(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowRange);

    FUNCTION_TEST_RETURN(BOOL, dataDefFound);
}

double
cfgDefOptionAllowRangeMax(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowRange);

    FUNCTION_TEST_RETURN(
        DOUBLE, ((double)(((int64_t)(intptr_t)dataDefList[2]) + (((int64_t)(intptr_t)dataDefList[3]) * 1000000000L))) / 100);
}

double
cfgDefOptionAllowRangeMin(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeAllowRange);

    FUNCTION_TEST_RETURN(
        DOUBLE, ((double)(((int64_t)(intptr_t)dataDefList[0]) + (((int64_t)(intptr_t)dataDefList[1]) * 1000000000L))) / 100);
}

/***********************************************************************************************************************************
Default value for the option
***********************************************************************************************************************************/
const char *
cfgDefOptionDefault(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeDefault);

    char *result = NULL;

    if (dataDefFound)
        result = (char *)dataDefList[0];

    FUNCTION_TEST_RETURN(STRINGZ, result);
}

/***********************************************************************************************************************************
Dependencies and depend lists
***********************************************************************************************************************************/
bool
cfgDefOptionDepend(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeDepend);

    FUNCTION_TEST_RETURN(BOOL, dataDefFound);
}

ConfigDefineOption
cfgDefOptionDependOption(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeDepend);

    FUNCTION_TEST_RETURN(ENUM, (ConfigDefineOption)dataDef);
}

const char *
cfgDefOptionDependValue(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, unsigned int valueId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
        FUNCTION_TEST_PARAM(UINT, valueId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());
    ASSERT(valueId < cfgDefOptionDependValueTotal(commandDefId, optionDefId));

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeDepend);

    FUNCTION_TEST_RETURN(STRINGZ, (char *)dataDefList[valueId]);
}

unsigned int
cfgDefOptionDependValueTotal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeDepend);

    FUNCTION_TEST_RETURN(UINT, dataDefListSize);
}

// Check if the value matches a value in the allow list
bool
cfgDefOptionDependValueValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());
    ASSERT(value != NULL);

    bool result = false;

    for (unsigned int valueIdx = 0; valueIdx < cfgDefOptionDependValueTotal(commandDefId, optionDefId); valueIdx++)
    {
        if (strcmp(value, cfgDefOptionDependValue(commandDefId, optionDefId, valueIdx)) == 0)
            result = true;
    }

    FUNCTION_TEST_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Option help description
***********************************************************************************************************************************/
const char *
cfgDefOptionHelpDescription(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeHelpDescription);

    const char *result = configDefineOptionData[optionDefId].helpDescription;

    if (dataDefFound)
        result = (char *)dataDefList[0];

    FUNCTION_TEST_RETURN(STRINGZ, result);
}

/***********************************************************************************************************************************
Option help name alt
***********************************************************************************************************************************/
bool
cfgDefOptionHelpNameAlt(ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(-1, optionDefId, configDefDataTypeHelpNameAlt);

    FUNCTION_TEST_RETURN(BOOL, dataDefFound);
}

const char *
cfgDefOptionHelpNameAltValue(ConfigDefineOption optionDefId, unsigned int valueId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
        FUNCTION_TEST_PARAM(UINT, valueId);
    FUNCTION_TEST_END();

    ASSERT(optionDefId < cfgDefOptionTotal());
    ASSERT(valueId < cfgDefOptionHelpNameAltValueTotal(optionDefId));

    CONFIG_DEFINE_DATA_FIND(-1, optionDefId, configDefDataTypeHelpNameAlt);

    FUNCTION_TEST_RETURN(STRINGZ, (char *)dataDefList[valueId]);
}

unsigned int
cfgDefOptionHelpNameAltValueTotal(ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(-1, optionDefId, configDefDataTypeHelpNameAlt);

    FUNCTION_TEST_RETURN(UINT, dataDefListSize);
}

/***********************************************************************************************************************************
Option help section
***********************************************************************************************************************************/
const char *
cfgDefOptionHelpSection(ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(optionDefId < cfgDefOptionTotal());

    FUNCTION_TEST_RETURN(STRINGZ, configDefineOptionData[optionDefId].helpSection);
}

/***********************************************************************************************************************************
Option help summary
***********************************************************************************************************************************/
const char *
cfgDefOptionHelpSummary(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeHelpSummary);

    const char *result = configDefineOptionData[optionDefId].helpSummary;

    if (dataDefFound)
        result = (char *)dataDefList[0];

    FUNCTION_TEST_RETURN(STRINGZ, result);
}

/***********************************************************************************************************************************
Get option id by name
***********************************************************************************************************************************/
int
cfgDefOptionId(const char *optionName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, optionName);
    FUNCTION_TEST_END();

    ASSERT(optionName != NULL);

    int result = -1;

    for (ConfigDefineOption optionDefId = 0; optionDefId < cfgDefOptionTotal(); optionDefId++)
        if (strcmp(optionName, configDefineOptionData[optionDefId].name) == 0)
            result = optionDefId;

    FUNCTION_TEST_RETURN(INT, result);
}

/***********************************************************************************************************************************
Get total indexed values for option
***********************************************************************************************************************************/
unsigned int
cfgDefOptionIndexTotal(ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(optionDefId < cfgDefOptionTotal());

    FUNCTION_TEST_RETURN(UINT, configDefineOptionData[optionDefId].indexTotal);
}

/***********************************************************************************************************************************
Is the option for internal use only?
***********************************************************************************************************************************/
bool
cfgDefOptionInternal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeInternal);

    bool result = configDefineOptionData[optionDefId].internal;

    if (dataDefFound)
        result = (bool)dataDef;

    FUNCTION_TEST_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Does the option accept multiple values?
***********************************************************************************************************************************/
bool
cfgDefOptionMulti(ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(optionDefId < cfgDefOptionTotal());

    FUNCTION_TEST_RETURN(
        BOOL,
        configDefineOptionData[optionDefId].type == cfgDefOptTypeHash ||
            configDefineOptionData[optionDefId].type == cfgDefOptTypeList);
}

/***********************************************************************************************************************************
Name of the option
***********************************************************************************************************************************/
const char *
cfgDefOptionName(ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(optionDefId < cfgDefOptionTotal());

    FUNCTION_TEST_RETURN(STRINGZ, configDefineOptionData[optionDefId].name);
}

/***********************************************************************************************************************************
Option prefix for indexed options
***********************************************************************************************************************************/
const char *
cfgDefOptionPrefix(ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(-1, optionDefId, configDefDataTypePrefix);

    char *result = NULL;

    if (dataDefFound)
        result = (char *)dataDefList[0];

    FUNCTION_TEST_RETURN(STRINGZ, result);
}

/***********************************************************************************************************************************
Does the option need to be protected from showing up in logs, command lines, etc?
***********************************************************************************************************************************/
bool
cfgDefOptionSecure(ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(optionDefId < cfgDefOptionTotal());

    FUNCTION_TEST_RETURN(BOOL, configDefineOptionData[optionDefId].secure);
}

/***********************************************************************************************************************************
Is the option required
***********************************************************************************************************************************/
bool
cfgDefOptionRequired(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    CONFIG_DEFINE_DATA_FIND(commandDefId, optionDefId, configDefDataTypeRequired);

    bool result = configDefineOptionData[optionDefId].required;

    if (dataDefFound)
        result = (bool)dataDef;

    FUNCTION_TEST_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Get option section
***********************************************************************************************************************************/
ConfigDefSection
cfgDefOptionSection(ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(optionDefId < cfgDefOptionTotal());

    FUNCTION_TEST_RETURN(ENUM, configDefineOptionData[optionDefId].section);
}

/***********************************************************************************************************************************
Get option data type
***********************************************************************************************************************************/
int
cfgDefOptionType(ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(optionDefId < cfgDefOptionTotal());

    FUNCTION_TEST_RETURN(INT, configDefineOptionData[optionDefId].type);
}

/***********************************************************************************************************************************
Is the option valid for the command?
***********************************************************************************************************************************/
bool
cfgDefOptionValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandDefId);
        FUNCTION_TEST_PARAM(ENUM, optionDefId);
    FUNCTION_TEST_END();

    ASSERT(commandDefId < cfgDefCommandTotal());
    ASSERT(optionDefId < cfgDefOptionTotal());

    FUNCTION_TEST_RETURN(BOOL, configDefineOptionData[optionDefId].commandValid & (1 << commandDefId));
}
