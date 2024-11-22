/***********************************************************************************************************************************
Command and Option Parse
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/bufferRead.h"
#include "common/log.h"
#include "common/macro.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "config/common.h"
#include "config/config.intern.h"
#include "config/parse.h"
#include "version.h"

/***********************************************************************************************************************************
The maximum number of keys that an indexed option can have, e.g. pg256-path would be the maximum pg-path option
***********************************************************************************************************************************/
#define CFG_OPTION_KEY_MAX                                          256

/***********************************************************************************************************************************
Section enum - defines which sections of the config an option can appear in
***********************************************************************************************************************************/
typedef enum
{
    cfgSectionCommandLine,                                          // Command-line only
    cfgSectionGlobal,                                               // Command-line or in any config section
    cfgSectionStanza,                                               // Command-line or in any config stanza section
} ConfigSection;

/***********************************************************************************************************************************
Standard config file name and old default path and name
***********************************************************************************************************************************/
#define PGBACKREST_CONFIG_ORIG_PATH_FILE                            "/etc/" PROJECT_CONFIG_FILE
STRING_STATIC(PGBACKREST_CONFIG_ORIG_PATH_FILE_STR,                 PGBACKREST_CONFIG_ORIG_PATH_FILE);

/***********************************************************************************************************************************
Mem context and local variables
***********************************************************************************************************************************/
static struct ConfigParseLocal
{
    MemContext *memContext;                                         // Mem context
    Ini *ini;                                                       // Parsed ini data
} configParseLocal;

/***********************************************************************************************************************************
Define how a command is parsed
***********************************************************************************************************************************/
typedef struct ParseRuleCommand
{
    const char *name;                                               // Name
    unsigned int commandRoleValid : CFG_COMMAND_ROLE_TOTAL;         // Valid for the command role?
    bool lockRequired : 1;                                          // Is an immediate lock required?
    bool lockRemoteRequired : 1;                                    // Is a lock required on the remote?
    unsigned int lockType : 2;                                      // Lock type required
    bool logFile : 1;                                               // Will the command log to a file?
    unsigned int logLevelDefault : 4;                               // Default log level
    bool parameterAllowed : 1;                                      // Command-line parameters are allowed
} ParseRuleCommand;

// Macros used to define parse rules in parse.auto.c.inc in a format that diffs well
#define PARSE_RULE_COMMAND(...)                                                                                                    \
    {__VA_ARGS__}

#define PARSE_RULE_COMMAND_NAME(nameParam)                                                                                         \
    .name = nameParam

#define PARSE_RULE_COMMAND_ROLE_VALID_LIST(...)                                                                                    \
    .commandRoleValid = 0 __VA_ARGS__

#define PARSE_RULE_COMMAND_ROLE(commandRoleParam)                                                                                  \
    | (1 << cfgCmdRole##commandRoleParam)

#define PARSE_RULE_COMMAND_LOCK_REQUIRED(lockRequiredParam)                                                                        \
    .lockRequired = lockRequiredParam

#define PARSE_RULE_COMMAND_LOCK_REMOTE_REQUIRED(lockRemoteRequiredParam)                                                           \
    .lockRemoteRequired = lockRemoteRequiredParam

#define PARSE_RULE_COMMAND_LOCK_TYPE(lockTypeParam)                                                                                \
    .lockType = lockType##lockTypeParam

#define PARSE_RULE_COMMAND_LOG_FILE(logFileParam)                                                                                  \
    .logFile = logFileParam

#define PARSE_RULE_COMMAND_LOG_LEVEL_DEFAULT(logLevelDefaultParam)                                                                 \
    .logLevelDefault = logLevel##logLevelDefaultParam

#define PARSE_RULE_COMMAND_PARAMETER_ALLOWED(parameterAllowedParam)                                                                \
    .parameterAllowed = parameterAllowedParam

/***********************************************************************************************************************************
Define how an option group is parsed
***********************************************************************************************************************************/
typedef struct ParseRuleOptionGroup
{
    const char *name;                                               // All options in the group must be prefixed with this name
} ParseRuleOptionGroup;

// Macros used to define parse rules in parse.auto.c.inc in a format that diffs well
#define PARSE_RULE_OPTION_GROUP(...)                                                                                               \
    {__VA_ARGS__}

#define PARSE_RULE_OPTION_GROUP_NAME(nameParam)                                                                                    \
    .name = nameParam

/***********************************************************************************************************************************
Define how an option is parsed and interacts with other options
***********************************************************************************************************************************/
typedef struct ParseRuleOption
{
    const char *name;                                               // Name
    unsigned int type : 4;                                          // e.g. string, int, boolean
    bool boolLike : 1;                                              // Option accepts y/n and can be treated as bool?
    bool beta : 1;                                                  // Is the option a beta feature?
    bool negate : 1;                                                // Can the option be negated on the command line?
    bool reset : 1;                                                 // Can the option be reset on the command line?
    bool required : 1;                                              // Is the option required?
    unsigned int section : 2;                                       // e.g. global, stanza, cmd-line
    bool secure : 1;                                                // Needs to be redacted in logs and cmd-line?
    bool multi : 1;                                                 // Can be specified multiple times?
    bool group : 1;                                                 // In a group?
    unsigned int groupId : 1;                                       // Id if in a group
    bool deprecateMatch : 1;                                        // Does a deprecated name exactly match the option name?
    unsigned int packSize : 7;                                      // Size of optional data in pack format
    uint32_t commandRoleValid[CFG_COMMAND_ROLE_TOTAL];              // Valid for the command role?

    const unsigned char *pack;                                      // Optional data in pack format
} ParseRuleOption;

// Define additional types of data that can be associated with an option. Because these types are rare they are not given dedicated
// fields and are instead packed and read at runtime. This may seem inefficient but they are only accessed a single time during
// parse so space efficiency is more important than performance.
typedef enum
{
    parseRuleOptionalTypeValid = 1,
    parseRuleOptionalTypeAllowRange,
    parseRuleOptionalTypeAllowList,
    parseRuleOptionalTypeDefault,
    parseRuleOptionalTypeRequired,
} ParseRuleOptionalType;

// Optional rule filter types
typedef enum
{
    parseRuleFilterTypeCommand = 1,
} ParseRuleFilterType;

// Macros used to define parse rules in parse.auto.c.inc in a format that diffs well
#define PARSE_RULE_OPTION(...)                                                                                                     \
    {__VA_ARGS__}

#define PARSE_RULE_OPTION_NAME(nameParam)                                                                                          \
    .name = nameParam

#define PARSE_RULE_OPTION_TYPE(typeParam)                                                                                          \
    .type = cfgOptType##typeParam

#define PARSE_RULE_OPTION_BOOL_LIKE(boolLikeParam)                                                                                 \
    .boolLike = boolLikeParam

#define PARSE_RULE_OPTION_BETA(betaParam)                                                                                          \
    .beta = betaParam

#define PARSE_RULE_OPTION_NEGATE(negateParam)                                                                                      \
    .negate = negateParam

#define PARSE_RULE_OPTION_RESET(resetParam)                                                                                        \
    .reset = resetParam

#define PARSE_RULE_OPTION_REQUIRED(requiredParam)                                                                                  \
    .required = requiredParam

#define PARSE_RULE_OPTION_SECTION(sectionParam)                                                                                    \
    .section = cfgSection##sectionParam

#define PARSE_RULE_OPTION_SECURE(secureParam)                                                                                      \
    .secure = secureParam

#define PARSE_RULE_OPTION_MULTI(typeMulti)                                                                                         \
    .multi = typeMulti

#define PARSE_RULE_OPTION_GROUP_ID(groupIdParam)                                                                                   \
    .group = true, .groupId = cfgOptGrp##groupIdParam

#define PARSE_RULE_OPTION_DEPRECATE_MATCH(deprecateMatchParam)                                                                     \
    .deprecateMatch = deprecateMatchParam

#define PARSE_RULE_OPTION_COMMAND_ROLE_MAIN_VALID_LIST(...)                                                                        \
    .commandRoleValid[cfgCmdRoleMain] = 0 __VA_ARGS__

#define PARSE_RULE_OPTION_COMMAND_ROLE_ASYNC_VALID_LIST(...)                                                                       \
    .commandRoleValid[cfgCmdRoleAsync] = 0 __VA_ARGS__

#define PARSE_RULE_OPTION_COMMAND_ROLE_LOCAL_VALID_LIST(...)                                                                       \
    .commandRoleValid[cfgCmdRoleLocal] = 0 __VA_ARGS__

#define PARSE_RULE_OPTION_COMMAND_ROLE_REMOTE_VALID_LIST(...)                                                                      \
    .commandRoleValid[cfgCmdRoleRemote] = 0 __VA_ARGS__

#define PARSE_RULE_OPTION_COMMAND(commandParam)                                                                                    \
    | (1 << cfgCmd##commandParam)

#define PARSE_RULE_STRPUB(value)                                    {.buffer = (const char *)value, .size = sizeof(value) - 1}

// Macros used to define optional parse rules in pack format
#define PARSE_RULE_VARINT_01(value)                                                                                                \
    value
#define PARSE_RULE_VARINT_02(value)                                                                                                \
    0x80 | (value & 0x7f), (value >> 7) & 0x7f

#define PARSE_RULE_BOOL_TRUE                                        0x28
#define PARSE_RULE_BOOL_FALSE                                       0x20
#define PARSE_RULE_U32_1(value)                                     0x88, PARSE_RULE_VARINT_01(value)
#define PARSE_RULE_U32_2(value)                                     0x88, PARSE_RULE_VARINT_02(value)

#define PARSE_RULE_PACK(...)                                        __VA_ARGS__ 0x00
#define PARSE_RULE_PACK_SIZE(...)                                                                                                  \
    0xf0, 0x02, sizeof((const unsigned char []){PARSE_RULE_PACK(__VA_ARGS__)}),                                                    \
    PARSE_RULE_PACK(__VA_ARGS__)

#define PARSE_RULE_VAL_BOOL_TRUE                                    PARSE_RULE_BOOL_TRUE
#define PARSE_RULE_VAL_BOOL_FALSE                                   PARSE_RULE_BOOL_FALSE

#define PARSE_RULE_OPTIONAL(...)                                                                                                   \
    .packSize = sizeof((const unsigned char []){PARSE_RULE_PACK(__VA_ARGS__)}),                                                    \
    .pack = (const unsigned char []){PARSE_RULE_PACK(__VA_ARGS__)}
#define PARSE_RULE_OPTIONAL_GROUP(...)                              PARSE_RULE_PACK_SIZE(__VA_ARGS__)

#define PARSE_RULE_FILTER_CMD(...)                                                                                                 \
    PARSE_RULE_PACK_SIZE(PARSE_RULE_U32_1(parseRuleFilterTypeCommand), __VA_ARGS__)

#define PARSE_RULE_OPTIONAL_DEPEND(...)                                                                                            \
    PARSE_RULE_U32_1(parseRuleOptionalTypeValid), PARSE_RULE_PACK_SIZE(__VA_ARGS__)
#define PARSE_RULE_OPTIONAL_DEPEND_DEFAULT(value)                   value
#define PARSE_RULE_OPTIONAL_ALLOW_LIST(...)                                                                                        \
    PARSE_RULE_U32_1(parseRuleOptionalTypeAllowList), PARSE_RULE_PACK_SIZE(__VA_ARGS__)
#define PARSE_RULE_OPTIONAL_ALLOW_RANGE(...)                                                                                       \
    PARSE_RULE_U32_1(parseRuleOptionalTypeAllowRange), PARSE_RULE_PACK_SIZE(__VA_ARGS__)
#define PARSE_RULE_OPTIONAL_DEFAULT(...)                                                                                           \
    PARSE_RULE_U32_1(parseRuleOptionalTypeDefault), PARSE_RULE_PACK_SIZE(__VA_ARGS__)
#define PARSE_RULE_OPTIONAL_REQUIRED(...)                                                                                          \
    PARSE_RULE_U32_1(parseRuleOptionalTypeRequired), PARSE_RULE_PACK_SIZE(__VA_ARGS__)
#define PARSE_RULE_OPTIONAL_NOT_REQUIRED(...)                       PARSE_RULE_OPTIONAL_REQUIRED(__VA_ARGS__)

/***********************************************************************************************************************************
Define option deprecations
***********************************************************************************************************************************/
typedef struct ParseRuleOptionDeprecate
{
    const char *name;                                               // Deprecated name
    ConfigOption id;                                                // Option Id
    bool indexed;                                                   // Can the deprecation be indexed?
    bool unindexed;                                                 // Can the deprecation be unindexed?
} ParseRuleOptionDeprecate;

/***********************************************************************************************************************************
Include automatically generated parse data
***********************************************************************************************************************************/
#include "config/parse.auto.c.inc"

/***********************************************************************************************************************************
Struct to hold options parsed from the command line
***********************************************************************************************************************************/
typedef struct ParseOptionValue
{
    bool found : 1;                                                 // Was the option found?
    bool negate : 1;                                                // Was the option negated on the command line?
    bool reset : 1;                                                 // Was the option reset on the command line?
    unsigned int source : 2;                                        // Where was the option found?
    StringList *valueList;                                          // List of values found
} ParseOptionValue;

typedef struct ParseOption
{
    unsigned int indexListTotal;                                    // Total options in indexed list
    ParseOptionValue *indexList;                                    // List of indexed option values
} ParseOption;

#define FUNCTION_LOG_PARSE_OPTION_FORMAT(value, buffer, bufferSize)                                                                \
    typeToLog("ParseOption", buffer, bufferSize)

/***********************************************************************************************************************************
Get the indexed value, creating the array to contain it if needed
***********************************************************************************************************************************/
static ParseOptionValue *
parseOptionIdxValue(ParseOption *const optionList, const unsigned int optionId, const unsigned int optionKeyIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(PARSE_OPTION, optionList);            // Structure containing all options being parsed
        FUNCTION_TEST_PARAM(UINT, optionId);                        // Unique ID which also identifies the option in the parse list
        FUNCTION_TEST_PARAM(UINT, optionKeyIdx);                    // Zero-based key index (e.g. pg3-path => 2), 0 for non-indexed
    FUNCTION_TEST_END();

    // If the requested index is beyond what has already been allocated
    if (optionKeyIdx >= optionList[optionId].indexListTotal)
    {
        // If the option is in a group
        if (parseRuleOption[optionId].group)
        {
            unsigned int optionOffset = 0;

            // Allocate enough memory to include the requested indexed or a fixed amount to avoid too many allocations
            if (optionList[optionId].indexListTotal == 0)
            {
                optionList[optionId].indexListTotal =
                    optionKeyIdx >= (LIST_INITIAL_SIZE / 2) ? optionKeyIdx + 1 : (LIST_INITIAL_SIZE / 2);
                optionList[optionId].indexList = memNew(sizeof(ParseOptionValue) * optionList[optionId].indexListTotal);
            }
            // Allocate more memory when needed. This could be more efficient but the limited number of indexes currently allowed
            // makes it difficult to get coverage on a better implementation.
            else
            {
                optionOffset = optionList[optionId].indexListTotal;
                optionList[optionId].indexListTotal = optionKeyIdx + 1;
                optionList[optionId].indexList = memResize(
                    optionList[optionId].indexList, sizeof(ParseOptionValue) * optionList[optionId].indexListTotal);
            }

            // Initialize the newly allocated memory
            for (unsigned int optKeyIdx = optionOffset; optKeyIdx < optionList[optionId].indexListTotal; optKeyIdx++)
                optionList[optionId].indexList[optKeyIdx] = (ParseOptionValue){0};
        }
        // Else the option is not in a group so there can only be one value
        else
        {
            optionList[optionId].indexList = memNew(sizeof(ParseOptionValue));
            optionList[optionId].indexListTotal = 1;
            optionList[optionId].indexList[0] = (ParseOptionValue){0};
        }
    }

    // Return the indexed value
    FUNCTION_TEST_RETURN_TYPE_P(ParseOptionValue, &optionList[optionId].indexList[optionKeyIdx]);
}

/***********************************************************************************************************************************
Check that --beta is set if a beta option is used
***********************************************************************************************************************************/
static void
parseOptionBeta(
    const unsigned int optionId, const unsigned int optionKeyIdx, const bool beta, const ParseOption *const parseOptionBeta)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, optionId);
        FUNCTION_TEST_PARAM(UINT, optionKeyIdx);
        FUNCTION_TEST_PARAM(BOOL, beta);
        FUNCTION_TEST_PARAM_P(PARSE_OPTION, parseOptionBeta);
    FUNCTION_TEST_END();

    if (beta && optionId != cfgOptBeta && (parseOptionBeta->indexListTotal == 0 || parseOptionBeta->indexList[0].negate))
    {
        THROW_FMT(
            OptionInvalidError,
            "option '%s' is not valid without option '" CFGOPT_BETA "'\n"
            "HINT: beta features require the --" CFGOPT_BETA " option to prevent accidental usage.",
            cfgParseOptionKeyIdxName(optionId, optionKeyIdx));
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get command id by name
***********************************************************************************************************************************/
static ConfigCommand
cfgParseCommandId(const char *const commandName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, commandName);
    FUNCTION_TEST_END();

    ASSERT(commandName != NULL);

    // If command has a colon only search part before the colon
    const char *const colonPtr = strchr(commandName, ':');
    const size_t commandSize = colonPtr == NULL ? (size_t)strlen(commandName) : (size_t)(colonPtr - commandName);

    // Get command
    ConfigCommand commandId;

    for (commandId = 0; commandId < CFG_COMMAND_TOTAL; commandId++)
    {
        if (strncmp(commandName, parseRuleCommand[commandId].name, commandSize) == 0)
            break;
    }

    if (commandId == CFG_COMMAND_TOTAL)
        THROW_FMT(CommandInvalidError, "invalid command '%s'", commandName);

    FUNCTION_TEST_RETURN(ENUM, commandId);
}

/**********************************************************************************************************************************/
FN_EXTERN const char *
cfgParseCommandName(const ConfigCommand commandId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
    FUNCTION_TEST_END();

    ASSERT(commandId < CFG_COMMAND_TOTAL);

    FUNCTION_TEST_RETURN_CONST(STRINGZ, parseRuleCommand[commandId].name);
}

/***********************************************************************************************************************************
Convert command role from String to enum and vice versa
***********************************************************************************************************************************/
STRING_STATIC(CONFIG_COMMAND_ROLE_ASYNC_STR,                        CONFIG_COMMAND_ROLE_ASYNC);
STRING_STATIC(CONFIG_COMMAND_ROLE_LOCAL_STR,                        CONFIG_COMMAND_ROLE_LOCAL);
STRING_STATIC(CONFIG_COMMAND_ROLE_REMOTE_STR,                       CONFIG_COMMAND_ROLE_REMOTE);

static ConfigCommandRole
cfgParseCommandRoleEnum(const char *const commandRole)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, commandRole);
    FUNCTION_TEST_END();

    if (commandRole == NULL)
        FUNCTION_TEST_RETURN(ENUM, cfgCmdRoleMain);
    else if (strEqZ(CONFIG_COMMAND_ROLE_ASYNC_STR, commandRole))
        FUNCTION_TEST_RETURN(ENUM, cfgCmdRoleAsync);
    else if (strEqZ(CONFIG_COMMAND_ROLE_LOCAL_STR, commandRole))
        FUNCTION_TEST_RETURN(ENUM, cfgCmdRoleLocal);
    else if (strEqZ(CONFIG_COMMAND_ROLE_REMOTE_STR, commandRole))
        FUNCTION_TEST_RETURN(ENUM, cfgCmdRoleRemote);

    THROW_FMT(CommandInvalidError, "invalid command role '%s'", commandRole);
}

FN_EXTERN const String *
cfgParseCommandRoleStr(const ConfigCommandRole commandRole)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandRole);
    FUNCTION_TEST_END();

    const String *result = NULL;

    switch (commandRole)
    {
        case cfgCmdRoleMain:
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

    FUNCTION_TEST_RETURN_CONST(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
cfgParseCommandRoleName(const ConfigCommand commandId, const ConfigCommandRole commandRoleId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
        FUNCTION_TEST_PARAM(ENUM, commandRoleId);
    FUNCTION_TEST_END();

    String *const result = strCatZ(strNew(), cfgParseCommandName(commandId));

    if (commandRoleId != cfgCmdRoleMain)
        strCatFmt(result, ":%s", strZ(cfgParseCommandRoleStr(commandRoleId)));

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN const Ini *
cfgParseIni(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(INI, configParseLocal.ini);
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
cfgParseStanzaList(void)
{
    FUNCTION_TEST_VOID();

    StringList *const result = strLstNew();

    if (configParseLocal.ini != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const StringList *const sectionList = strLstSort(iniSectionList(configParseLocal.ini), sortOrderAsc);

            for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
            {
                const String *const section = strLstGet(sectionList, sectionIdx);

                // Skip global sections
                if (strEqZ(section, CFGDEF_SECTION_GLOBAL) || strBeginsWithZ(section, CFGDEF_SECTION_GLOBAL ":"))
                    continue;

                // Extract stanza
                const StringList *const sectionPart = strLstNewSplitZ(section, ":");
                ASSERT(strLstSize(sectionPart) <= 2);

                strLstAddIfMissing(result, strLstGet(sectionPart, 0));
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    strLstSort(result, sortOrderAsc);

    FUNCTION_TEST_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Find an option by name in the option list
***********************************************************************************************************************************/
#define OPTION_PREFIX_NEGATE                                        "no-"
#define OPTION_PREFIX_RESET                                         "reset-"
#define OPTION_NAME_SIZE_MAX                                        64

FN_EXTERN CfgParseOptionResult
cfgParseOption(const String *const optionCandidate, const CfgParseOptionParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, optionCandidate);
        FUNCTION_TEST_PARAM(BOOL, param.prefixMatch);
        FUNCTION_TEST_PARAM(BOOL, param.ignoreMissingIndex);
    FUNCTION_TEST_END();

    ASSERT(optionCandidate != NULL);

    CfgParseOptionResult result = {0};

    // Copy the option to a buffer so it can be efficiently manipulated
    char optionName[OPTION_NAME_SIZE_MAX + 1];
    size_t optionNameSize = strSize(optionCandidate);

    if (optionNameSize > sizeof(optionName) - 1)
    {
        THROW_FMT(
            OptionInvalidError, "option '%s' exceeds maximum size of " STRINGIFY(OPTION_NAME_SIZE_MAX), strZ(optionCandidate));
    }

    strncpy(optionName, strZ(optionCandidate), sizeof(optionName) - 1);
    optionName[OPTION_NAME_SIZE_MAX] = '\0';

    // If this looks like negate
    if (strncmp(optionName, OPTION_PREFIX_NEGATE, sizeof(OPTION_PREFIX_NEGATE) - 1) == 0)
    {
        result.negate = true;

        // Strip the negate prefix
        optionNameSize -= sizeof(OPTION_PREFIX_NEGATE) - 1;
        memmove(optionName, optionName + (sizeof(OPTION_PREFIX_NEGATE) - 1), optionNameSize + 1);
    }
    // Else if looks like reset
    else if (strncmp(optionName, OPTION_PREFIX_RESET, sizeof(OPTION_PREFIX_RESET) - 1) == 0)
    {
        result.reset = true;

        // Strip the reset prefix
        optionNameSize -= sizeof(OPTION_PREFIX_RESET) - 1;
        memmove(optionName, optionName + (sizeof(OPTION_PREFIX_RESET) - 1), optionNameSize + 1);
    }

    // Indexed options must have at least one dash
    char *const dashPtr = strchr(optionName, '-');
    bool indexed = false;

    if (dashPtr != NULL)
    {
        if (dashPtr == optionName)
            THROW_FMT(OptionInvalidError, "option '%s' cannot begin with a dash", strZ(optionCandidate));

        // Check if the first dash is preceded by a numeric key and keep a tally of the key
        char *numberPtr = dashPtr;
        unsigned int multiplier = 1;

        while (numberPtr > optionName && isdigit(*(numberPtr - 1)))
        {
            numberPtr--;

            result.keyIdx += (unsigned int)(*numberPtr - '0') * multiplier;
            multiplier *= 10;
        }

        if (numberPtr == optionName)
            THROW_FMT(OptionInvalidError, "option '%s' cannot begin with a number", strZ(optionCandidate));

        // If there was a number then the option is indexed
        if (numberPtr != dashPtr)
        {
            indexed = true;

            // Strip the key to get the base option name
            optionNameSize -= (size_t)(dashPtr - numberPtr);
            memmove(numberPtr, dashPtr, optionNameSize + 1);

            // Check that the index does not exceed the maximum
            if (result.keyIdx > CFG_OPTION_KEY_MAX)
            {
                THROW_FMT(
                    OptionInvalidError, "option '%s' key exceeds maximum of " STRINGIFY(CFG_OPTION_KEY_MAX), strZ(optionCandidate));
            }

            // Subtract one to represent a key index
            result.keyIdx--;
        }
    }

    // Search for an exact match. A copy of the option name must be made because bsearch() requires a reference.
    const char *const optionNamePtr = optionName;

    const ParseRuleOption *optionFound = bsearch(
        &optionNamePtr, parseRuleOption, CFG_OPTION_TOTAL, sizeof(ParseRuleOption), lstComparatorZ);

    // If the option was not found
    if (optionFound == NULL)
    {
        // Search for a single partial match (if requested)
        if (param.prefixMatch)
        {
            unsigned int findPartialIdx = 0;
            unsigned int findPartialTotal = 0;

            for (unsigned int findIdx = 0; findIdx < CFG_OPTION_TOTAL; findIdx++)
            {
                if (strncmp(parseRuleOption[findIdx].name, optionName, optionNameSize) == 0)
                {
                    findPartialIdx = findIdx;
                    findPartialTotal++;

                    if (findPartialTotal > 1)
                        break;
                }
            }

            // If a single partial match was found
            if (findPartialTotal == 1)
                optionFound = &parseRuleOption[findPartialIdx];
        }

        // If the option was not found search deprecations
        if (optionFound == NULL)
        {
            // Search deprecations for an exact match
            const ParseRuleOptionDeprecate *deprecate = bsearch(
                &optionNamePtr, parseRuleOptionDeprecate, CFG_OPTION_DEPRECATE_TOTAL, sizeof(ParseRuleOptionDeprecate),
                lstComparatorZ);

            // If the option was not found then search deprecations for a single partial match (if requested)
            if (deprecate == NULL && param.prefixMatch)
            {
                unsigned int findPartialIdx = 0;
                unsigned int findPartialTotal = 0;

                for (unsigned int deprecateIdx = 0; deprecateIdx < CFG_OPTION_DEPRECATE_TOTAL; deprecateIdx++)
                {
                    if (strncmp(parseRuleOptionDeprecate[deprecateIdx].name, optionName, optionNameSize) == 0)
                    {
                        findPartialIdx = deprecateIdx;
                        findPartialTotal++;

                        if (findPartialTotal > 1)
                            break;
                    }
                }

                // If a single partial match was found
                if (findPartialTotal == 1)
                    deprecate = &parseRuleOptionDeprecate[findPartialIdx];
            }

            // Deprecation was found
            if (deprecate != NULL)
            {
                // Error if the option is indexed but the deprecation is not
                if (indexed && !deprecate->indexed)
                    THROW_FMT(OptionInvalidError, "deprecated option '%s' cannot have an index", strZ(optionCandidate));

                // Error if the option is unindexed but the deprecation is not
                optionFound = &parseRuleOption[deprecate->id];

                if (!indexed && !deprecate->unindexed && !param.ignoreMissingIndex)
                {
                    ASSERT(dashPtr != NULL);

                    const char *const groupName = parseRuleOptionGroup[optionFound->groupId].name;

                    THROW_FMT(
                        OptionInvalidError, "deprecated option '%s' requires an index\n"
                        "HINT: add the required index, e.g. %.*s1%s.\n"
                        "HINT: consider using the non-deprecated name, e.g. %s1%s.",
                        strZ(optionCandidate), (int)(dashPtr - optionName), optionName, dashPtr, groupName,
                        optionFound->name + strlen(groupName));
                }

                result.deprecated = true;
            }
        }
    }

    // Option was found
    if (optionFound != NULL)
    {
        result.found = true;
        result.id = (unsigned int)(optionFound - parseRuleOption);

        // Error if negate is not allowed
        if (result.negate && !optionFound->negate)
            THROW_FMT(OptionInvalidError, "option '%s' cannot be negated", strZ(optionCandidate));

        // Error if reset is not allowed
        if (result.reset && !optionFound->reset)
            THROW_FMT(OptionInvalidError, "option '%s' cannot be reset", strZ(optionCandidate));

        // It is possible for an unindexed deprecation to match an indexed option name (without the index) exactly. For example, the
        // deprecated repo-path option now maps to repo-path index 0, which will yield an exact match. In this case we still need to
        // mark the option as deprecated.
        if (indexed == false && optionFound->deprecateMatch)
            result.deprecated = true;

        // If not deprecated make sure indexing matches. Deprecation indexing has already been checked because the rules are per
        // deprecation.
        if (!result.deprecated)
        {
            // Error if the option is indexed but should not be
            if (indexed && !optionFound->group)
                THROW_FMT(OptionInvalidError, "option '%s' cannot have an index", strZ(optionCandidate));

            // Error if the option is unindexed but an index is required
            if (!indexed && optionFound->group && !param.ignoreMissingIndex)
            {
                const char *const groupName = parseRuleOptionGroup[optionFound->groupId].name;

                THROW_FMT(
                    OptionInvalidError,
                    "option '%s' requires an index\n"
                    "HINT: add the required index, e.g. %s1%s.",
                    strZ(optionCandidate), groupName, optionFound->name + strlen(groupName));
            }
        }

        // Set flags
        result.beta = optionFound->beta;
        result.multi = optionFound->multi;

        FUNCTION_TEST_RETURN_TYPE(CfgParseOptionResult, result);
    }

    FUNCTION_TEST_RETURN_TYPE(CfgParseOptionResult, (CfgParseOptionResult){0});
}

/**********************************************************************************************************************************/
FN_EXTERN ConfigOptionDataType
cfgParseOptionDataType(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    switch (parseRuleOption[optionId].type)
    {
        case cfgOptTypeBoolean:
            FUNCTION_TEST_RETURN(ENUM, cfgOptDataTypeBoolean);

        case cfgOptTypeHash:
            FUNCTION_TEST_RETURN(ENUM, cfgOptDataTypeHash);

        case cfgOptTypeInteger:
        case cfgOptTypeSize:
        case cfgOptTypeTime:
            FUNCTION_TEST_RETURN(ENUM, cfgOptDataTypeInteger);

        case cfgOptTypeList:
            FUNCTION_TEST_RETURN(ENUM, cfgOptDataTypeList);

        case cfgOptTypeStringId:
            FUNCTION_TEST_RETURN(ENUM, cfgOptDataTypeStringId);

        default:
            break;
    }

    ASSERT(parseRuleOption[optionId].type == cfgOptTypePath || parseRuleOption[optionId].type == cfgOptTypeString);

    FUNCTION_TEST_RETURN(ENUM, cfgOptDataTypeString);
}

/***********************************************************************************************************************************
Get a value string by index
***********************************************************************************************************************************/
static const String *
cfgParseOptionValueStr(const ConfigOptionType type, unsigned int valueIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(UINT, valueIdx);
    FUNCTION_TEST_END();

    switch (type)
    {
        case cfgOptTypeInteger:
            valueIdx = parseRuleValueIntStrMap[valueIdx];
            break;

        case cfgOptTypeSize:
            valueIdx = parseRuleValueSizeStrMap[valueIdx];
            break;

        case cfgOptTypeTime:
            valueIdx = parseRuleValueTimeStrMap[valueIdx];
            break;

        case cfgOptTypePath:
        case cfgOptTypeString:
            break;

        default:
        {
            ASSERT(type == cfgOptTypeStringId);

            valueIdx = parseRuleValueStrIdStrMap[valueIdx];
            break;
        }
    }

    FUNCTION_TEST_RETURN_CONST(STRING, (const String *)&parseRuleValueStr[valueIdx]);
}

/***********************************************************************************************************************************
Get an integer value by index
***********************************************************************************************************************************/
static int64_t
cfgParseOptionValue(const ConfigOptionType type, unsigned int valueIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(UINT, valueIdx);
    FUNCTION_TEST_END();

    switch (type)
    {
        case cfgOptTypeInteger:
            FUNCTION_TEST_RETURN(INT64, parseRuleValueInt[valueIdx]);

        case cfgOptTypeSize:
            FUNCTION_TEST_RETURN(INT64, parseRuleValueSize[valueIdx]);

        default:
            ASSERT(type == cfgOptTypeTime);
            break;
    }

    FUNCTION_TEST_RETURN(INT64, parseRuleValueTime[valueIdx]);
}

/***********************************************************************************************************************************
Find an optional rule
***********************************************************************************************************************************/
typedef struct CfgParseOptionalRuleState
{
    PackRead *pack;
    unsigned int typeNext;
    bool done;

    // Valid
    const unsigned char *valid;
    size_t validSize;

    // Allow range
    int64_t allowRangeMin;
    int64_t allowRangeMax;
    unsigned int allowRangeMinIdx;
    unsigned int allowRangeMaxIdx;

    // Allow list
    const unsigned char *allowList;
    size_t allowListSize;

    // Default
    const String *defaultRaw;
    ConfigOptionValueType defaultValue;

    // Required
    bool required;
} CfgParseOptionalRuleState;

static bool
cfgParseOptionalRule(
    CfgParseOptionalRuleState *const optionalRules, const ParseRuleOptionalType optionalRuleType, const ConfigCommand commandId,
    const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, optionalRules);
        FUNCTION_TEST_PARAM(ENUM, optionalRuleType);
        FUNCTION_TEST_PARAM(ENUM, commandId);
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(optionalRuleType != 0);
    ASSERT(commandId < CFG_COMMAND_TOTAL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    bool result = false;

    // Check for optional rules
    const ParseRuleOption *const ruleOption = &parseRuleOption[optionId];

    if (!optionalRules->done && ruleOption->pack != NULL)
    {
        // Initialize optional rules
        if (optionalRules->pack == NULL)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                PackRead *const groupList = pckReadNewC(ruleOption->pack, ruleOption->packSize);

                // Search for a matching group
                do
                {
                    // Get the group pack
                    PackRead *group = pckReadPackReadConstP(groupList);

                    // Process filters if any
                    pckReadNext(group);

                    if (pckReadType(group) == pckTypePack)
                    {
                        // Check for filter match
                        bool match = false;
                        PackRead *const filter = pckReadPackReadConstP(group);
                        const ParseRuleFilterType filterType = (ParseRuleFilterType)pckReadU32P(filter);

                        switch (filterType)
                        {
                            default:
                            {
                                ASSERT(filterType == parseRuleFilterTypeCommand);

                                while (pckReadNext(filter))
                                {
                                    if ((ConfigCommand)pckReadU32P(filter) == commandId)
                                    {
                                        match = true;
                                        break;
                                    }
                                }

                                break;
                            }
                        }

                        // Filter did not match
                        if (!match)
                            group = NULL;
                    }

                    // If the group matched
                    if (group != NULL)
                    {
                        // Get first optional rule type. This is needed since pckReadNext() has already been called and it cannot
                        // be called again until data is read.
                        optionalRules->typeNext = pckReadU32P(group);
                        ASSERT(optionalRules->typeNext != 0);

                        // Move group to prior context and stop searching
                        optionalRules->pack = pckReadMove(group, memContextPrior());
                        break;
                    }
                }
                while (pckReadNext(groupList));
            }
            MEM_CONTEXT_TEMP_END();

            // If no group matched then done
            if (optionalRules->pack == NULL)
            {
                optionalRules->done = true;
                FUNCTION_TEST_RETURN(BOOL, false);
            }
        }

        // Search for the specified optional rule
        do
        {
            // Read the next optional rule type if it has not already been read
            if (optionalRules->typeNext == 0)
            {
                // If there are no more rules then done
                if (!pckReadNext(optionalRules->pack))
                {
                    optionalRules->done = true;
                    FUNCTION_TEST_RETURN(BOOL, false);
                }

                optionalRules->typeNext = pckReadU32P(optionalRules->pack);
                ASSERT(optionalRules->typeNext != 0);
            }

            // If this is the requested optional rule
            if (optionalRules->typeNext == optionalRuleType)
            {
                // Optional rule was found
                result = true;

                // Process optional rule
                switch (optionalRuleType)
                {
                    case parseRuleOptionalTypeValid:
                    {
                        pckReadNext(optionalRules->pack);

                        optionalRules->valid = pckReadBufPtr(optionalRules->pack);
                        optionalRules->validSize = pckReadSize(optionalRules->pack);

                        pckReadConsume(optionalRules->pack);
                        break;
                    }

                    case parseRuleOptionalTypeAllowList:
                    {
                        pckReadNext(optionalRules->pack);

                        optionalRules->allowList = pckReadBufPtr(optionalRules->pack);
                        optionalRules->allowListSize = pckReadSize(optionalRules->pack);

                        pckReadConsume(optionalRules->pack);
                        break;
                    }

                    case parseRuleOptionalTypeAllowRange:
                    {
                        PackRead *const ruleData = pckReadPackReadConstP(optionalRules->pack);

                        optionalRules->allowRangeMinIdx = pckReadU32P(ruleData);
                        optionalRules->allowRangeMaxIdx = pckReadU32P(ruleData);
                        optionalRules->allowRangeMin = cfgParseOptionValue(ruleOption->type, optionalRules->allowRangeMinIdx);
                        optionalRules->allowRangeMax = cfgParseOptionValue(ruleOption->type, optionalRules->allowRangeMaxIdx);

                        break;
                    }

                    case parseRuleOptionalTypeDefault:
                    {
                        PackRead *const ruleData = pckReadPackReadConstP(optionalRules->pack);
                        pckReadNext(ruleData);

                        switch (pckReadType(ruleData))
                        {
                            case pckTypeBool:
                                optionalRules->defaultValue.boolean = pckReadBoolP(ruleData);
                                optionalRules->defaultRaw = optionalRules->defaultValue.boolean ? Y_STR : N_STR;
                                break;

                            default:
                            {
                                const unsigned int valueIdx = pckReadU32P(ruleData);

                                switch (ruleOption->type)
                                {
                                    case cfgOptTypeInteger:
                                    case cfgOptTypeSize:
                                    case cfgOptTypeTime:
                                        optionalRules->defaultValue.integer = cfgParseOptionValue(ruleOption->type, valueIdx);
                                        break;

                                    case cfgOptTypePath:
                                    case cfgOptTypeString:
                                        optionalRules->defaultValue.string = cfgParseOptionValueStr(ruleOption->type, valueIdx);
                                        break;

                                    case cfgOptTypeStringId:
                                        optionalRules->defaultValue.stringId = parseRuleValueStrId[valueIdx];
                                        break;
                                }

                                optionalRules->defaultRaw = cfgParseOptionValueStr(ruleOption->type, valueIdx);
                            }
                        }

                        break;
                    }

                    default:
                    {
                        ASSERT(optionalRuleType == parseRuleOptionalTypeRequired);

                        optionalRules->required = !ruleOption->required;
                    }
                }
            }
            // Else not the requested optional rule
            else
            {
                // If the optional rule type is greater than requested then return. The optional rule may be requested later.
                if (optionalRules->typeNext > optionalRuleType)
                    FUNCTION_TEST_RETURN(BOOL, false);

                // Consume the unused optional rule
                pckReadConsume(optionalRules->pack);
            }

            // Type will need to be read again on next iteration
            optionalRules->typeNext = 0;
        }
        while (!result);
    }

    FUNCTION_TEST_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Resolve an option dependency
***********************************************************************************************************************************/
typedef struct CfgParseOptionalFilterDependResult
{
    bool valid;
    bool defaultExists;
    bool defaultValue;
} CfgParseOptionalFilterDependResult;

static CfgParseOptionalFilterDependResult
cfgParseOptionalFilterDepend(PackRead *const filter, const Config *const config, const unsigned int optionListIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, filter);
        FUNCTION_TEST_PARAM_P(VOID, config);
        FUNCTION_TEST_PARAM(UINT, optionListIdx);
    FUNCTION_TEST_END();

    CfgParseOptionalFilterDependResult result = {.valid = false};

    // Default when the dependency is not resolved, if it exists
    pckReadNext(filter);

    if (pckReadType(filter) == pckTypeBool)
    {
        result.defaultExists = true;
        result.defaultValue = pckReadBoolP(filter);
    }

    // Get the depend option value
    const ConfigOption dependId = (ConfigOption)pckReadU32P(filter);
    ASSERT(config->option[dependId].index != NULL);
    const ConfigOptionValue *const dependValue =
        &config->option[dependId].index[parseRuleOption[dependId].group ? optionListIdx : 0];

    // Is the dependency resolved?
    if (dependValue->set)
    {
        // If a depend list exists, make sure the value is in the list
        if (pckReadNext(filter))
        {
            do
            {
                switch (cfgParseOptionDataType(dependId))
                {
                    case cfgOptDataTypeBoolean:
                        result.valid = pckReadBoolP(filter) == dependValue->value.boolean;
                        break;

                    default:
                    {
                        ASSERT(cfgParseOptionDataType(dependId) == cfgOptDataTypeStringId);

                        if (parseRuleValueStrId[pckReadU32P(filter)] == dependValue->value.stringId)
                            result.valid = true;
                        break;
                    }
                }
            }
            while (pckReadNext(filter));
        }
        else
            result.valid = true;
    }

    FUNCTION_TEST_RETURN_TYPE(CfgParseOptionalFilterDependResult, result);
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
cfgParseOptionDefault(const ConfigCommand commandId, const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(commandId < CFG_COMMAND_TOTAL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    const String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        CfgParseOptionalRuleState optionalRules = {0};

        if (cfgParseOptionalRule(&optionalRules, parseRuleOptionalTypeDefault, commandId, optionId))
            result = optionalRules.defaultRaw;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_CONST(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN const char *
cfgParseOptionName(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN_CONST(STRINGZ, parseRuleOption[optionId].name);
}

/**********************************************************************************************************************************/
FN_EXTERN const char *
cfgParseOptionKeyIdxName(const ConfigOption optionId, const unsigned int keyIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, keyIdx);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);
    ASSERT((!parseRuleOption[optionId].group && keyIdx == 0) || parseRuleOption[optionId].group);

    // If the option is in a group then construct the name
    const ParseRuleOption *const ruleOption = &parseRuleOption[optionId];

    if (ruleOption->group)
    {
        FUNCTION_TEST_RETURN_CONST(
            STRINGZ,
            zNewFmt(
                "%s%u%s", parseRuleOptionGroup[ruleOption->groupId].name, keyIdx + 1,
                ruleOption->name + strlen(parseRuleOptionGroup[ruleOption->groupId].name)));
    }

    // Else return the stored name
    FUNCTION_TEST_RETURN_CONST(STRINGZ, ruleOption->name);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgParseOptionRequired(const ConfigCommand commandId, const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(commandId < CFG_COMMAND_TOTAL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    bool found = false;
    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        CfgParseOptionalRuleState optionalRules = {0};

        if (cfgParseOptionalRule(&optionalRules, parseRuleOptionalTypeRequired, commandId, optionId))
        {
            found = true;
            result = optionalRules.required;
        }
    }
    MEM_CONTEXT_TEMP_END();

    if (found)
        FUNCTION_TEST_RETURN(BOOL, result);

    FUNCTION_TEST_RETURN(BOOL, parseRuleOption[optionId].required);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgParseOptionSecure(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(BOOL, parseRuleOption[optionId].secure);
}

/**********************************************************************************************************************************/
FN_EXTERN ConfigOptionType
cfgParseOptionType(const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(ENUM, parseRuleOption[optionId].type);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cfgParseOptionValid(const ConfigCommand commandId, const ConfigCommandRole commandRoleId, const ConfigOption optionId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, commandId);
        FUNCTION_TEST_PARAM(ENUM, commandRoleId);
        FUNCTION_TEST_PARAM(ENUM, optionId);
    FUNCTION_TEST_END();

    ASSERT(commandId < CFG_COMMAND_TOTAL);
    ASSERT(optionId < CFG_OPTION_TOTAL);

    FUNCTION_TEST_RETURN(BOOL, parseRuleOption[optionId].commandRoleValid[commandRoleId] & ((uint32_t)1 << commandId));
}

/***********************************************************************************************************************************
Load the configuration file(s)

The parent mem context is used. Defaults are passed to make testing easier.

Rules:
- config and config-include-path are default. In this case, the config file will be loaded, if it exists, and *.conf files in the
  config-include-path will be appended, if they exist. A missing/empty dir will be ignored except that the original default
  for the config file will be attempted to be loaded if the current default is not found.
- config only is specified. Only the specified config file will be loaded and is required. The default config-include-path will be
  ignored.
- config and config-path are specified. The specified config file will be loaded and is required. The overridden default of the
  config-include-path (<config-path>/conf.d) will be loaded if exists but is not required.
- config-include-path only is specified. *.conf files in the config-include-path will be loaded and the path is required to exist.
  The default config will be be loaded if it exists.
- config-include-path and config-path are specified. The *.conf files in the config-include-path will be loaded and the directory
  passed must exist. The overridden default of the config file path (<config-path>/pgbackrest.conf) will be loaded if exists but is
  not required.
- If the config and config-include-path are specified. The config file will be loaded and is expected to exist and *.conf files in
  the config-include-path will be appended and at least one is expected to exist.
- If --no-config is specified and --config-include-path is specified then only *.conf files in the config-include-path will be
  loaded; the directory is required.
- If --no-config is specified and --config-path is specified then only *.conf files in the overridden default config-include-path
  (<config-path>/conf.d) will be loaded if exist but not required.
- If --no-config is specified and neither --config-include-path nor --config-path are specified then no configs will be loaded.
- If --config-path only, the defaults for config and config-include-path will be changed to use that as a base path but the files
  will not be required to exist since this is a default override.
***********************************************************************************************************************************/
static void
cfgFileLoadPart(String **const config, const Buffer *const configPart)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(STRING, config);
        FUNCTION_LOG_PARAM(BUFFER, configPart);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    if (configPart != NULL)
    {
        // Validate the file by parsing it as an Ini object. If the file is not properly formed, an error will occur.
        if (bufUsed(configPart) > 0)
        {
            iniValid(iniNewP(ioBufferReadNew(configPart)));

            // Create the result config file
            if (*config == NULL)
            {
                *config = strNew();
            }
            // Else add an LF in case the previous file did not end with one
            else
                strCat(*config, LF_STR);

            // Add the config part to the result config file
            strCat(*config, strNewBuf(configPart));
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

static String *
cfgFileLoad(
    const Storage *const storage,                                   // Storage to load configs
    const ParseOption *const optionList,                            // All options and their current settings
    // NOTE: Passing defaults to enable more complete test coverage
    const String *optConfigDefault,                                 // Current default for --config option
    const String *optConfigIncludePathDefault,                      // Current default for --config-include-path option
    const String *const origConfigDefault)                          // Original --config option default (/etc/pgbackrest.conf)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM_P(PARSE_OPTION, optionList);
        FUNCTION_LOG_PARAM(STRING, optConfigDefault);
        FUNCTION_LOG_PARAM(STRING, optConfigIncludePathDefault);
        FUNCTION_LOG_PARAM(STRING, origConfigDefault);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(optionList != NULL);
    ASSERT(optConfigDefault != NULL);
    ASSERT(optConfigIncludePathDefault != NULL);
    ASSERT(origConfigDefault != NULL);

    bool loadConfig = true;
    bool loadConfigInclude = true;

    // If the option is specified on the command line, then found will be true meaning the file is required to exist, else it is
    // optional
    const bool configFound = optionList[cfgOptConfig].indexList != NULL && optionList[cfgOptConfig].indexList[0].found;
    bool configRequired = configFound;
    const bool configPathRequired =
        optionList[cfgOptConfigPath].indexList != NULL && optionList[cfgOptConfigPath].indexList[0].found;
    bool configIncludeRequired =
        optionList[cfgOptConfigIncludePath].indexList != NULL && optionList[cfgOptConfigIncludePath].indexList[0].found;

    // Save default for later determining if must check old original default config path
    const String *const optConfigDefaultCurrent = optConfigDefault;

    // If the config-path option is found on the command line, then its value will override the base path defaults for config and
    // config-include-path
    if (configPathRequired)
    {
        optConfigDefault = strNewFmt(
            "%s/%s", strZ(strLstGet(optionList[cfgOptConfigPath].indexList[0].valueList, 0)), strBaseZ(optConfigDefault));
        optConfigIncludePathDefault = strNewFmt(
            "%s/%s", strZ(strLstGet(optionList[cfgOptConfigPath].indexList[0].valueList, 0)), PROJECT_CONFIG_INCLUDE_PATH);
    }

    // If the --no-config option was passed then do not load the config file
    if (optionList[cfgOptConfig].indexList != NULL && optionList[cfgOptConfig].indexList[0].negate)
    {
        loadConfig = false;
        configRequired = false;
    }

    // If --config option is specified on the command line but neither the --config-include-path nor the config-path are passed,
    // then do not attempt to load the include files
    if (configFound && !(configPathRequired || configIncludeRequired))
    {
        loadConfigInclude = false;
        configIncludeRequired = false;
    }

    String *result = NULL;

    // Load the main config file
    if (loadConfig)
    {
        const String *configFileName = NULL;

        // Get the config file name from the command-line if it exists else default
        if (configRequired)
            configFileName = strLstGet(optionList[cfgOptConfig].indexList[0].valueList, 0);
        else
            configFileName = optConfigDefault;

        // Load the config file
        const Buffer *buffer = storageGetP(storageNewReadP(storage, configFileName, .ignoreMissing = !configRequired));

        // Convert the contents of the file buffer to the config string object
        if (buffer != NULL)
            result = strCatBuf(strNew(), buffer);
        else if (strEq(configFileName, optConfigDefaultCurrent))
        {
            // If config is current default and it was not found, attempt to load the config file from the old default location
            buffer = storageGetP(storageNewReadP(storage, origConfigDefault, .ignoreMissing = !configRequired));

            if (buffer != NULL)
                result = strCatBuf(strNew(), buffer);
        }
    }

    // Load *.conf files from the include directory
    if (loadConfigInclude)
    {
        // Validate the file by parsing it as an Ini object. If the file is not properly formed, an error will occur.
        if (result != NULL)
            iniValid(iniNewP(ioBufferReadNew(BUFSTR(result))));

        const String *configIncludePath = NULL;

        // Get the config include path from the command-line if it exists else default
        if (configIncludeRequired)
            configIncludePath = strLstGet(optionList[cfgOptConfigIncludePath].indexList[0].valueList, 0);
        else
            configIncludePath = optConfigIncludePathDefault;

        // Get a list of conf files from the specified path -error on missing directory if the option was passed on the command line
        StringList *const list = storageListP(
            storage, configIncludePath, .expression = STRDEF(".+\\.conf$"), .errorOnMissing = configIncludeRequired,
            .nullOnMissing = !configIncludeRequired);

        // If conf files are found, then add them to the config string
        if (list != NULL && !strLstEmpty(list))
        {
            // Sort the list for reproducibility only -- order does not matter
            strLstSort(list, sortOrderAsc);

            for (unsigned int listIdx = 0; listIdx < strLstSize(list); listIdx++)
            {
                cfgFileLoadPart(
                    &result,
                    storageGetP(
                        storageNewReadP(
                            storage, strNewFmt("%s/%s", strZ(configIncludePath), strZ(strLstGet(list, listIdx))),
                            .ignoreMissing = true)));
            }
        }
    }

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
??? Add validation of section names and check all sections for invalid options in the check command. It's too expensive to add the
logic to this critical path code.
***********************************************************************************************************************************/
// Helper to check that option values are valid for conditional builds. This is a bit tricky since the distros used for unit testing
// have all possible features enabled, so this is split out to allow it to be tested independently. The loop variable is
// intentionally integrated into this function to make it obvious if it is omitted from the caller.
static bool
cfgParseOptionValueCondition(
    bool more, PackRead *const allowList, const bool allowListFound, const unsigned int optionId, const unsigned int optionKeyIdx,
    const String *const valueAllow)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, more);
        FUNCTION_TEST_PARAM(PACK_READ, allowList);
        FUNCTION_TEST_PARAM(BOOL, allowListFound);
        FUNCTION_TEST_PARAM(UINT, optionId);
        FUNCTION_TEST_PARAM(UINT, optionKeyIdx);
        FUNCTION_TEST_PARAM(STRING, valueAllow);
    FUNCTION_TEST_END();

    ASSERT(allowList != NULL);
    ASSERT(valueAllow != NULL);

    if (more && pckReadType(allowList) == pckTypeBool)
    {
        pckReadBoolP(allowList);

        if (allowListFound)
        {
            THROW_FMT(
                OptionInvalidValueError,
                PROJECT_NAME " not built with '%s=%s' support\n"
                "HINT: if " PROJECT_NAME " was installed from a package, does the package support this feature?\n"
                "HINT: if " PROJECT_NAME " was built from source, were the required development packages installed?",
                cfgParseOptionKeyIdxName(optionId, optionKeyIdx), strZ(valueAllow));
        }

        more = pckReadNext(allowList);
    }

    FUNCTION_TEST_RETURN(BOOL, more);
}

FN_EXTERN void
cfgParse(const Storage *const storage, const unsigned int argListSize, const char *argList[], const CfgParseParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(UINT, argListSize);
        FUNCTION_LOG_PARAM(CHARPY, argList);
        FUNCTION_LOG_PARAM(BOOL, param.noResetLogLevel);
        FUNCTION_LOG_PARAM(BOOL, param.noConfigLoad);
    FUNCTION_LOG_END();

    // Initialize local mem context
    if (configParseLocal.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN(ConfigParse, .childQty = MEM_CONTEXT_QTY_MAX)
            {
                configParseLocal.memContext = MEM_CONTEXT_NEW();
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create the config struct
        Config *config;

        OBJ_NEW_BASE_BEGIN(Config, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
        {
            config = OBJ_NEW_ALLOC();

            *config = (Config)
            {
                .memContext = MEM_CONTEXT_NEW(),
                .command = cfgCmdHelp,
                .exe = strNewZ(argList[0]),
            };
        }
        OBJ_NEW_END();

        // Phase 1: parse command line parameters
        // -------------------------------------------------------------------------------------------------------------------------
        // List of parsed options
        ParseOption parseOptionList[CFG_OPTION_TOTAL] = {{0}};

        // Only the first non-option parameter should be treated as a command so track if the command has been set
        bool commandSet = false;
        bool commandHelpSet = false;

        for (unsigned int argListIdx = 1; argListIdx < argListSize; argListIdx++)
        {
            const char *arg = argList[argListIdx];

            // If an option
            if (arg[0] == '-')
            {
                // Options must start with --
                if (arg[1] != '-')
                    THROW_FMT(OptionInvalidError, "option '%s' must begin with --", arg);

                // Consume --
                arg += 2;

                // Get the option name and the value when separated by =
                const String *optionName;
                const String *optionArg = NULL;

                const char *const equalPtr = strchr(arg, '=');

                if (equalPtr)
                {
                    optionName = strNewZN(arg, (size_t)(equalPtr - arg));
                    optionArg = strNewZ(equalPtr + 1);
                }
                else
                    optionName = strNewZ(arg);

                // Lookup the option name
                CfgParseOptionResult option = cfgParseOptionP(optionName, .prefixMatch = true);

                if (!option.found)
                    THROW_FMT(OptionInvalidError, "invalid option '--%s'", arg);

                // If the option may have an argument (arguments are optional for boolean options)
                const ParseRuleOption *const ruleOption = &parseRuleOption[option.id];

                if (!option.negate && !option.reset)
                {
                    // Handle boolean (only y/n allowed as argument)
                    if (ruleOption->type == cfgOptTypeBoolean)
                    {
                        // Validate argument/set negate when argument present
                        if (optionArg != NULL)
                        {
                            if (strEqZ(optionArg, "n"))
                                option.negate = true;
                            else if (!strEqZ(optionArg, "y"))
                            {
                                THROW_FMT(
                                    OptionInvalidValueError, "boolean option '--%s' argument must be 'y' or 'n'", strZ(optionName));
                            }
                        }
                    }
                    // If no argument was found with the option then try the next argument (unless bool-like)
                    else if (optionArg == NULL)
                    {
                        // If bool-like then set arg to y
                        if (ruleOption->boolLike)
                        {
                            optionArg = Y_STR;
                        }
                        // Else if there are no more arguments in the list
                        else if (argListIdx == argListSize - 1)
                        {
                            THROW_FMT(OptionInvalidError, "option '--%s' requires an argument", strZ(optionName));
                        }
                        // Else get arg
                        else
                            optionArg = strNewZ(argList[++argListIdx]);
                    }
                }
                // Else error if an argument was found with the option
                else if (optionArg != NULL)
                    THROW_FMT(OptionInvalidError, "option '%s' does not allow an argument", strZ(optionName));

                // if negated and bool-like then set to n
                if (option.negate && ruleOption->boolLike)
                {
                    option.negate = false;
                    optionArg = N_STR;
                }

                // Error if this option is secure and cannot be passed on the command line
                if (cfgParseOptionSecure(option.id))
                {
                    THROW_FMT(
                        OptionInvalidError,
                        "option '%s' is not allowed on the command-line\n"
                        "HINT: this option could expose secrets in the process list.\n"
                        "HINT: specify the option in a configuration file or an environment variable instead.",
                        cfgParseOptionKeyIdxName(option.id, option.keyIdx));
                }

                // If the option has not been found yet then set it
                ParseOptionValue *const optionValue = parseOptionIdxValue(parseOptionList, option.id, option.keyIdx);

                if (!optionValue->found)
                {
                    *optionValue = (ParseOptionValue)
                    {
                        .found = true,
                        .negate = option.negate,
                        .reset = option.reset,
                        .source = cfgSourceParam,
                    };

                    // Only set the argument if the option has one
                    if (optionArg != NULL)
                    {
                        optionValue->valueList = strLstNew();
                        strLstAdd(optionValue->valueList, optionArg);
                    }
                }
                else
                {
                    // Make sure option is not negated more than once. It probably wouldn't hurt anything to accept this case but
                    // there's no point in allowing the user to be sloppy.
                    if (optionValue->negate && option.negate)
                    {
                        THROW_FMT(
                            OptionInvalidError, "option '%s' is negated multiple times",
                            cfgParseOptionKeyIdxName(option.id, option.keyIdx));
                    }

                    // Make sure option is not reset more than once. Same justification as negate.
                    if (optionValue->reset && option.reset)
                    {
                        THROW_FMT(
                            OptionInvalidError, "option '%s' is reset multiple times",
                            cfgParseOptionKeyIdxName(option.id, option.keyIdx));
                    }

                    // Don't allow an option to be both negated and reset
                    if ((optionValue->reset && option.negate) || (optionValue->negate && option.reset))
                    {
                        THROW_FMT(
                            OptionInvalidError, "option '%s' cannot be negated and reset",
                            cfgParseOptionKeyIdxName(option.id, option.keyIdx));
                    }

                    // Don't allow an option to be both set and negated
                    if (optionValue->negate != option.negate)
                    {
                        THROW_FMT(
                            OptionInvalidError, "option '%s' cannot be set and negated",
                            cfgParseOptionKeyIdxName(option.id, option.keyIdx));
                    }

                    // Don't allow an option to be both set and reset
                    if (optionValue->reset != option.reset)
                    {
                        THROW_FMT(
                            OptionInvalidError, "option '%s' cannot be set and reset",
                            cfgParseOptionKeyIdxName(option.id, option.keyIdx));
                    }

                    // Add the argument
                    if (optionArg != NULL && option.multi)
                    {
                        strLstAdd(optionValue->valueList, optionArg);
                    }
                    // Error if the option does not accept multiple arguments
                    else
                    {
                        THROW_FMT(
                            OptionInvalidError, "option '%s' cannot be set multiple times",
                            cfgParseOptionKeyIdxName(option.id, option.keyIdx));
                    }
                }
            }
            // Else command or parameter
            else
            {
                // The first argument should be the command
                if (!commandSet)
                {
                    // Try getting the command from the valid command list
                    config->command = cfgParseCommandId(arg);

                    // Set help if help command
                    if (config->command == cfgCmdHelp && !commandHelpSet)
                    {
                        commandHelpSet = true;
                    }
                    // Else parse command role if appended
                    else
                    {
                        const char *const colonPtr = strchr(arg, ':');

                        if (colonPtr != NULL)
                            config->commandRole = cfgParseCommandRoleEnum(colonPtr + 1);
                        else
                            config->commandRole = cfgCmdRoleMain;

                        // Error when role is not valid for the command
                        if (!(parseRuleCommand[config->command].commandRoleValid & ((unsigned int)1 << config->commandRole)))
                            THROW_FMT(CommandInvalidError, "invalid command/role combination '%s'", arg);

                        // Error if help requested and role is not main
                        if (commandHelpSet && config->commandRole != cfgCmdRoleMain)
                            THROW_FMT(CommandInvalidError, "help is not available for the '%s' role", colonPtr + 1);

                        config->help = commandHelpSet;
                        commandSet = true;
                    }
                }
                // Additional arguments are command arguments
                else
                {
                    if (config->paramList == NULL)
                    {
                        MEM_CONTEXT_BEGIN(config->memContext)
                        {
                            config->paramList = strLstNew();
                        }
                        MEM_CONTEXT_END();
                    }

                    strLstAddZ(config->paramList, arg);
                }
            }
        }

        // Handle command not found
        if (!commandSet && !commandHelpSet)
        {
            ASSERT(config->command == cfgCmdHelp);

            for (unsigned int optionIdx = 0; optionIdx < CFG_OPTION_TOTAL; optionIdx++)
            {
                if (parseOptionList[optionIdx].indexListTotal != 0 && !cfgParseOptionValid(cfgCmdHelp, cfgCmdRoleMain, optionIdx))
                    THROW_FMT(CommandRequiredError, "no command found");
            }
        }

        // Set command options
        const ParseRuleCommand *const ruleCommand = &parseRuleCommand[config->command];

        config->lockRequired = ruleCommand->lockRequired;
        config->lockRemoteRequired = ruleCommand->lockRemoteRequired;
        config->lockType = (LockType)ruleCommand->lockType;
        config->logFile = ruleCommand->logFile;
        config->logLevelDefault = (LogLevel)ruleCommand->logLevelDefault;

        // Error when parameters found but the command does not allow parameters
        if (config->paramList != NULL && !config->help && !ruleCommand->parameterAllowed)
            THROW(ParamInvalidError, "command does not allow parameters");

        // Enable logging for main role so config file warnings will be output
        if (!param.noResetLogLevel && config->commandRole == cfgCmdRoleMain)
            logInit(logLevelWarn, logLevelOff, logLevelOff, false, 0, 1, false);

        // Determine whether any options should be loaded from the environment or config files. The help and version commands do
        // not use any non command-line options so they can skip this step. This prevents basic help or version from failing if,
        // e.g. the process does not have access to load the config file or there is an invalid option in the environment.
        const bool optionLoad = config->command != cfgCmdHelp && config->command != cfgCmdVersion;

        // Phase 2: parse environment variables
        // -------------------------------------------------------------------------------------------------------------------------
        if (optionLoad)
        {
            unsigned int environIdx = 0;

            // Loop through all environment variables and look for our env vars by matching the prefix
            while (environ[environIdx] != NULL)
            {
                const char *keyValue = environ[environIdx];
                environIdx++;

                if (strstr(keyValue, PGBACKREST_ENV) == keyValue)
                {
                    // Find the first = char
                    const char *const equalPtr = strchr(keyValue, '=');
                    ASSERT(equalPtr != NULL);

                    // Get key and value
                    const String *const key = strReplaceChr(
                        strLower(strNewZN(keyValue + PGBACKREST_ENV_SIZE, (size_t)(equalPtr - (keyValue + PGBACKREST_ENV_SIZE)))),
                        '_', '-');
                    const String *const value = STR(equalPtr + 1);

                    // Find the option
                    CfgParseOptionResult option = cfgParseOptionP(key);

                    // Warn if the option not found
                    if (!option.found)
                    {
                        LOG_WARN_FMT("environment contains invalid option '%s'", strZ(key));
                        continue;
                    }
                    // Warn if negate option found in env
                    else if (option.negate)
                    {
                        LOG_WARN_FMT("environment contains invalid negate option '%s'", strZ(key));
                        continue;
                    }
                    // Warn if reset option found in env
                    else if (option.reset)
                    {
                        LOG_WARN_FMT("environment contains invalid reset option '%s'", strZ(key));
                        continue;
                    }

                    // Continue if the option is not valid for this command
                    if (!cfgParseOptionValid(config->command, config->commandRole, option.id))
                        continue;

                    if (strSize(value) == 0)
                        THROW_FMT(OptionInvalidValueError, "environment variable '%s' must have a value", strZ(key));

                    // Continue if the option has already been specified on the command line
                    ParseOptionValue *optionValue = parseOptionIdxValue(parseOptionList, option.id, option.keyIdx);

                    if (optionValue->found)
                        continue;

                    optionValue->found = true;
                    optionValue->source = cfgSourceConfig;

                    // Convert boolean to string
                    if (cfgParseOptionType(option.id) == cfgOptTypeBoolean)
                    {
                        if (strEqZ(value, "n"))
                            optionValue->negate = true;
                        else if (!strEqZ(value, "y"))
                            THROW_FMT(OptionInvalidValueError, "environment boolean option '%s' must be 'y' or 'n'", strZ(key));
                    }
                    // Else split list/hash into separate values
                    else if (option.multi)
                    {
                        optionValue->valueList = strLstNewSplitZ(value, ":");
                    }
                    // Else add the string value
                    else
                    {
                        optionValue->valueList = strLstNew();
                        strLstAdd(optionValue->valueList, value);
                    }
                }
            }
        }

        // Phase 3: parse config file unless --no-config passed
        // -------------------------------------------------------------------------------------------------------------------------
        // Load the configuration file(s)
        if (!param.noConfigLoad && optionLoad)
        {
            const String *const configString = cfgFileLoad(
                storage, parseOptionList,
                (const String *)&parseRuleValueStr[parseRuleValStrCFGOPTDEF_CONFIG_PATH_SP_QT_FS_QT_SP_PROJECT_CONFIG_FILE],
                (const String *)&parseRuleValueStr[parseRuleValStrCFGOPTDEF_CONFIG_PATH_SP_QT_FS_QT_SP_PROJECT_CONFIG_INCLUDE_PATH],
                PGBACKREST_CONFIG_ORIG_PATH_FILE_STR);

            iniFree(configParseLocal.ini);
            configParseLocal.ini = NULL;

            if (configString != NULL)
            {
                MEM_CONTEXT_BEGIN(configParseLocal.memContext)
                {
                    configParseLocal.ini = iniNewP(ioBufferReadNew(BUFSTR(configString)), .store = true);
                }
                MEM_CONTEXT_END();
            }
        }

        if (configParseLocal.ini != NULL)
        {
            // Get the stanza name
            String *stanza = NULL;

            if (parseOptionList[cfgOptStanza].indexList != NULL)
                stanza = strLstGet(parseOptionList[cfgOptStanza].indexList[0].valueList, 0);

            // Build list of sections to search for options
            StringList *const sectionList = strLstNew();

            if (stanza != NULL)
            {
                strLstAddFmt(sectionList, "%s:%s", strZ(stanza), cfgParseCommandName(config->command));
                strLstAdd(sectionList, stanza);
            }

            strLstAddFmt(sectionList, CFGDEF_SECTION_GLOBAL ":%s", cfgParseCommandName(config->command));
            strLstAddZ(sectionList, CFGDEF_SECTION_GLOBAL);

            // Loop through sections to search for options
            for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
            {
                const String *const section = strLstGet(sectionList, sectionIdx);
                const StringList *const keyList = iniSectionKeyList(configParseLocal.ini, section);
                KeyValue *const optionFound = kvNew();

                // Loop through keys to search for options
                for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
                {
                    String *const key = strLstGet(keyList, keyIdx);

                    // Find the optionName in the main list
                    const CfgParseOptionResult option = cfgParseOptionP(key);
                    const ParseRuleOption *const ruleOption = &parseRuleOption[option.id];

                    // Warn if the option not found
                    if (!option.found)
                    {
                        LOG_WARN_FMT("configuration file contains invalid option '%s'", strZ(key));
                        continue;
                    }
                    // Warn if negate option found in config
                    else if (option.negate)
                    {
                        LOG_WARN_FMT("configuration file contains negate option '%s'", strZ(key));
                        continue;
                    }
                    // Warn if reset option found in config
                    else if (option.reset)
                    {
                        LOG_WARN_FMT("configuration file contains reset option '%s'", strZ(key));
                        continue;
                    }

                    // Warn if this option should be command-line only
                    if (ruleOption->section == cfgSectionCommandLine)
                    {
                        LOG_WARN_FMT("configuration file contains command-line only option '%s'", strZ(key));
                        continue;
                    }

                    // Make sure this option does not appear in the same section with an alternate name
                    const Variant *const optionFoundKey = VARUINT64(option.id * CFG_OPTION_KEY_MAX + option.keyIdx);
                    const Variant *const optionFoundName = kvGet(optionFound, optionFoundKey);

                    if (optionFoundName != NULL)
                    {
                        THROW_FMT(
                            OptionInvalidError, "configuration file contains duplicate options ('%s', '%s') in section '[%s]'",
                            strZ(key), strZ(varStr(optionFoundName)), strZ(section));
                    }
                    else
                        kvPut(optionFound, optionFoundKey, VARSTR(key));

                    // Continue if the option is not valid for this command
                    if (!cfgParseOptionValid(config->command, config->commandRole, option.id))
                    {
                        // Warn if it is in a command section
                        if (sectionIdx % 2 == 0)
                        {
                            LOG_WARN_FMT(
                                "configuration file contains option '%s' invalid for section '%s'", strZ(key), strZ(section));
                        }

                        continue;
                    }

                    // Continue if stanza option is in a global section
                    if (ruleOption->section == cfgSectionStanza &&
                        (strEqZ(section, CFGDEF_SECTION_GLOBAL) || strBeginsWithZ(section, CFGDEF_SECTION_GLOBAL ":")))
                    {
                        LOG_WARN_FMT(
                            "configuration file contains stanza-only option '%s' in global section '%s'", strZ(key), strZ(section));
                        continue;
                    }

                    // Continue if this option has already been found in another section or command-line/environment
                    ParseOptionValue *const optionValue = parseOptionIdxValue(parseOptionList, option.id, option.keyIdx);

                    if (optionValue->found)
                        continue;

                    optionValue->found = true;
                    optionValue->source = cfgSourceConfig;

                    // Process list
                    if (iniSectionKeyIsList(configParseLocal.ini, section, key))
                    {
                        // Error if the option cannot be specified multiple times
                        if (!option.multi)
                        {
                            THROW_FMT(
                                OptionInvalidError, "option '%s' cannot be set multiple times",
                                cfgParseOptionKeyIdxName(option.id, option.keyIdx));
                        }

                        optionValue->valueList = iniGetList(configParseLocal.ini, section, key);
                    }
                    else
                    {
                        // Get the option value
                        const String *const value = iniGet(configParseLocal.ini, section, key);

                        if (strSize(value) == 0)
                        {
                            THROW_FMT(
                                OptionInvalidValueError, "section '%s', key '%s' must have a value", strZ(section), strZ(key));
                        }

                        if (cfgParseOptionType(option.id) == cfgOptTypeBoolean)
                        {
                            if (strEqZ(value, "n"))
                                optionValue->negate = true;
                            else if (!strEqZ(value, "y"))
                                THROW_FMT(OptionInvalidValueError, "boolean option '%s' must be 'y' or 'n'", strZ(key));
                        }
                        // Else add the string value
                        else
                        {
                            optionValue->valueList = strLstNew();
                            strLstAdd(optionValue->valueList, value);
                        }
                    }
                }
            }
        }

        // Phase 4: create the config and resolve indexed options for each group
        // -------------------------------------------------------------------------------------------------------------------------
        // Determine how many indexes are used in each group
        bool groupIdxMap[CFG_OPTION_GROUP_TOTAL][CFG_OPTION_KEY_MAX] = {{0}};

        for (unsigned int optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
        {
            // Always assign name since it may be needed for error messages
            const ParseRuleOption *const ruleOption = &parseRuleOption[optionId];
            config->option[optionId].name = ruleOption->name;

            // Is the option valid for this command?
            if (cfgParseOptionValid(config->command, config->commandRole, optionId))
            {
                config->option[optionId].valid = true;
                config->option[optionId].dataType = cfgParseOptionDataType(optionId);
                config->option[optionId].group = ruleOption->group;
                config->option[optionId].groupId = ruleOption->groupId;
            }
            else
            {
                // Error if the invalid option was explicitly set on the command-line
                if (parseOptionList[optionId].indexList != NULL)
                {
                    THROW_FMT(
                        OptionInvalidError, "option '%s' not valid for command '%s'", cfgParseOptionName(optionId),
                        cfgParseCommandName(config->command));
                }

                // Continue to the next option
                continue;
            }

            // If the option is in a group
            if (ruleOption->group)
            {
                const unsigned int groupId = ruleOption->groupId;

                config->optionGroup[groupId].valid = true;

                // Scan the option values to determine which indexes are in use. Store them in a map that will later be scanned to
                // create a list of just the used indexes.
                for (unsigned int optionKeyIdx = 0; optionKeyIdx < parseOptionList[optionId].indexListTotal; optionKeyIdx++)
                {
                    if (parseOptionList[optionId].indexList[optionKeyIdx].found &&
                        !parseOptionList[optionId].indexList[optionKeyIdx].reset)
                    {
                        if (!groupIdxMap[groupId][optionKeyIdx])
                        {
                            config->optionGroup[groupId].indexTotal++;
                            groupIdxMap[groupId][optionKeyIdx] = true;
                        }
                    }
                }
            }
        }

        // Write the indexes into the group in order
        for (unsigned int groupId = 0; groupId < CFG_OPTION_GROUP_TOTAL; groupId++)
        {
            // Set group name
            config->optionGroup[groupId].name = parseRuleOptionGroup[groupId].name;

            // Skip the group if it is not valid
            if (!config->optionGroup[groupId].valid)
                continue;

            // Allocate memory for the index to key index map
            const unsigned int optionIdxTotal = config->optionGroup[groupId].indexTotal;

            MEM_CONTEXT_BEGIN(config->memContext)
            {
                config->optionGroup[groupId].indexMap = memNew(sizeof(unsigned int) * (optionIdxTotal == 0 ? 1 : optionIdxTotal));
            }
            MEM_CONTEXT_END();

            // If no values were found in any index then use index 0 since all valid groups must have at least one index. This may
            // lead to an error unless all options in the group have defaults but that will be resolved later.
            if (optionIdxTotal == 0)
            {
                config->optionGroup[groupId].indexTotal = 1;
                config->optionGroup[groupId].indexMap[0] = 0;
            }
            // Else write the key to index map for the group. This allows translation from keys to indexes and vice versa.
            else
            {
                unsigned int optionIdxMax = 0;
                unsigned int optionKeyIdx = 0;

                // ??? For the pg group, key 1 is required to maintain compatibility with older versions. Before removing this
                // constraint the pg group remap to key 1 for remotes will need to be dealt with in the protocol/helper module.
                if (groupId == cfgOptGrpPg)
                {
                    optionKeyIdx = 1;
                    optionIdxMax = 1;

                    config->optionGroup[groupId].indexMap[0] = 0;
                }

                // Write keys into the index map
                for (; optionKeyIdx < CFG_OPTION_KEY_MAX; optionKeyIdx++)
                {
                    if (groupIdxMap[groupId][optionKeyIdx])
                    {
                        config->optionGroup[groupId].indexMap[optionIdxMax] = optionKeyIdx;
                        optionIdxMax++;
                    }
                }
            }
        }

        // Phase 5: validate option definitions and load into configuration
        // -------------------------------------------------------------------------------------------------------------------------
        // Determine whether a group index will be kept based on non-default values
        bool optionGroupIndexKeep[CFG_OPTION_GROUP_TOTAL][CFG_OPTION_KEY_MAX] = {{false}};

        for (unsigned int optionOrderIdx = 0; optionOrderIdx < CFG_OPTION_TOTAL; optionOrderIdx++)
        {
            // Validate options based on the option resolve order. This allows resolving all options in a single pass.
            const ConfigOption optionId = optionResolveOrder[optionOrderIdx];

            // Skip this option if it is not valid
            if (!config->option[optionId].valid)
                continue;

            // Determine the option index total. For options that are not indexed the index total is 1.
            const ParseRuleOption *const ruleOption = &parseRuleOption[optionId];
            const bool optionGroup = ruleOption->group;
            const unsigned int optionGroupId = optionGroup ? ruleOption->groupId : UINT_MAX;
            const unsigned int optionListIndexTotal = optionGroup ? config->optionGroup[optionGroupId].indexTotal : 1;

            MEM_CONTEXT_BEGIN(config->memContext)
            {
                config->option[optionId].index = memNew(sizeof(ConfigOptionValue) * optionListIndexTotal);
            }
            MEM_CONTEXT_END();

            // Loop through the option indexes
            const ConfigOptionType optionType = cfgParseOptionType(optionId);

            for (unsigned int optionListIdx = 0; optionListIdx < optionListIndexTotal; optionListIdx++)
            {
                // Get the key index by looking it up in the group or by defaulting to 0 for ungrouped options
                const unsigned optionKeyIdx = optionGroup ? config->optionGroup[optionGroupId].indexMap[optionListIdx] : 0;

                // Get the parsed value using the key index. Provide a default structure when the value was not found.
                const ParseOptionValue *const parseOptionValue =
                    optionKeyIdx < parseOptionList[optionId].indexListTotal ?
                        &parseOptionList[optionId].indexList[optionKeyIdx] : &(ParseOptionValue){0};

                // Get the location where the value will be stored in the configuration
                ConfigOptionValue *const configOptionValue = &config->option[optionId].index[optionListIdx];

                // Is the value set for this option?
                const bool optionSet =
                    parseOptionValue->found && (optionType == cfgOptTypeBoolean || !parseOptionValue->negate) &&
                    !parseOptionValue->reset;

                // Initialize option value and set negate and reset flag
                *configOptionValue = (ConfigOptionValue){.negate = parseOptionValue->negate, .reset = parseOptionValue->reset};

                // Is the option valid?
                CfgParseOptionalRuleState optionalRules = {0};
                CfgParseOptionalFilterDependResult dependResult = {.valid = true};

                if (cfgParseOptionalRule(&optionalRules, parseRuleOptionalTypeValid, config->command, optionId))
                {
                    PackRead *const filter = pckReadNewC(optionalRules.valid, optionalRules.validSize);
                    dependResult = cfgParseOptionalFilterDepend(filter, config, optionListIdx);

                    // If depend not resolved and option value is set on the command-line then error. It is OK to have unresolved
                    // options in the config file because they may be there for another command. For instance, spool-path is only
                    // loaded for the archive-push command when archive-async=y, and the presence of spool-path in the config file
                    // should not cause an error here, it will just end up null.
                    if (!dependResult.valid && optionSet && parseOptionValue->source == cfgSourceParam)
                    {
                        PackRead *const filter = pckReadNewC(optionalRules.valid, optionalRules.validSize);

                        // If there is a boolean default value just consume it since it is not needed here
                        pckReadNext(filter);

                        if (pckReadType(filter) == pckTypeBool)
                            pckReadBoolP(filter);

                        // Get depend option id and name
                        const ConfigOption dependId = pckReadU32P(filter);
                        const String *dependOptionName = STR(
                            cfgParseOptionKeyIdxName(dependId, parseRuleOption[dependId].group ? optionKeyIdx : 0));

                        // If depend value is not set
                        ASSERT(config->option[dependId].index != NULL);

                        if (!config->option[dependId].index[optionListIdx].set)
                        {
                            THROW_FMT(
                                OptionInvalidError, "option '%s' not valid without option '%s'",
                                cfgParseOptionKeyIdxName(optionId, optionKeyIdx), strZ(dependOptionName));
                        }

                        // Build type dependent error data
                        const String *errorValue = EMPTY_STR;

                        switch (parseRuleOption[dependId].type)
                        {
                            case cfgOptTypeBoolean:
                            {
                                if (!pckReadBoolP(filter))
                                    dependOptionName = strNewFmt("no-%s", strZ(dependOptionName));

                                break;
                            }

                            default:
                            {
                                String *const errorList = strNew();
                                unsigned int validSize = 0;

                                while (pckReadNext(filter))
                                {
                                    strCatFmt(
                                        errorList, "%s'%s'", validSize != 0 ? ", " : "",
                                        strZ(cfgParseOptionValueStr(parseRuleOption[dependId].type, pckReadU32P(filter))));

                                    validSize++;
                                }

                                ASSERT(validSize > 0);

                                if (validSize == 1)
                                    errorValue = strNewFmt(" = %s", strZ(errorList));
                                else
                                    errorValue = strNewFmt(" in (%s)", strZ(errorList));
                            }
                        }

                        THROW_FMT(
                            OptionInvalidError, "option '%s' not valid without option '%s'%s",
                            cfgParseOptionKeyIdxName(optionId, optionKeyIdx), strZ(dependOptionName), strZ(errorValue));
                    }

                    pckReadFree(filter);
                }

                if (dependResult.valid)
                {
                    // Is the option set?
                    if (optionSet)
                    {
                        configOptionValue->set = true;
                        configOptionValue->source = parseOptionValue->source;

                        // Check beta status
                        parseOptionBeta(optionId, optionKeyIdx, ruleOption->beta, &parseOptionList[cfgOptBeta]);

                        if (optionType == cfgOptTypeBoolean)
                        {
                            configOptionValue->value.boolean = !parseOptionValue->negate;
                        }
                        else if (optionType == cfgOptTypeHash)
                        {
                            KeyValue *value = NULL;

                            MEM_CONTEXT_BEGIN(config->memContext)
                            {
                                value = kvNew();
                            }
                            MEM_CONTEXT_END();

                            for (unsigned int listIdx = 0; listIdx < strLstSize(parseOptionValue->valueList); listIdx++)
                            {
                                const char *const pair = strZ(strLstGet(parseOptionValue->valueList, listIdx));
                                const char *const equal = strchr(pair, '=');

                                if (equal == NULL)
                                {
                                    THROW_FMT(
                                        OptionInvalidError, "key/value '%s' not valid for '%s' option",
                                        strZ(strLstGet(parseOptionValue->valueList, listIdx)),
                                        cfgParseOptionKeyIdxName(optionId, optionKeyIdx));
                                }

                                kvPut(value, VARSTR(strNewZN(pair, (size_t)(equal - pair))), VARSTRZ(equal + 1));
                            }

                            configOptionValue->value.keyValue = value;
                        }
                        else if (optionType == cfgOptTypeList)
                        {
                            MEM_CONTEXT_BEGIN(config->memContext)
                            {
                                configOptionValue->value.list = varLstNewStrLst(parseOptionValue->valueList);
                            }
                            MEM_CONTEXT_END();
                        }
                        else
                        {
                            String *const value = strLstGet(parseOptionValue->valueList, 0);
                            const String *const valueAllow = value;

                            // Preserve original value to display
                            MEM_CONTEXT_BEGIN(config->memContext)
                            {
                                configOptionValue->display = strDup(value);
                            }
                            MEM_CONTEXT_END();

                            // If a numeric type check that the value is valid
                            if (optionType == cfgOptTypeInteger || optionType == cfgOptTypeSize || optionType == cfgOptTypeTime)
                            {
                                // Check that the value can be converted
                                TRY_BEGIN()
                                {
                                    switch (optionType)
                                    {
                                        case cfgOptTypeInteger:
                                            configOptionValue->value.integer = cvtZToInt64(strZ(value));
                                            break;

                                        case cfgOptTypeSize:
                                            configOptionValue->value.integer = cfgParseSize(value);
                                            break;

                                        default:
                                        {
                                            ASSERT(optionType == cfgOptTypeTime);

                                            configOptionValue->value.integer = cfgParseTime(value);
                                            break;
                                        }
                                    }
                                }
                                CATCH_ANY()
                                {
                                    THROW_FMT(
                                        OptionInvalidValueError, "'%s' is not valid for '%s' option", strZ(value),
                                        cfgParseOptionKeyIdxName(optionId, optionKeyIdx));
                                }
                                TRY_END();

                                if (cfgParseOptionalRule(
                                        &optionalRules, parseRuleOptionalTypeAllowRange, config->command, optionId) &&
                                    (configOptionValue->value.integer < optionalRules.allowRangeMin ||
                                     configOptionValue->value.integer > optionalRules.allowRangeMax))
                                {
                                    THROW_FMT(
                                        OptionInvalidValueError,
                                        "'%s' is out of range for '%s' option\n"
                                        "HINT: allowed range is %s to %s inclusive",
                                        strZ(value), cfgParseOptionKeyIdxName(optionId, optionKeyIdx),
                                        strZ(cfgParseOptionValueStr(optionType, optionalRules.allowRangeMinIdx)),
                                        strZ(cfgParseOptionValueStr(optionType, optionalRules.allowRangeMaxIdx)));
                                }
                            }
                            // Else if StringId
                            else if (optionType == cfgOptTypeStringId)
                            {
                                configOptionValue->value.stringId = strIdFromZN(strZ(valueAllow), strSize(valueAllow), false);
                            }
                            // Else if string make sure it is valid
                            else
                            {
                                ASSERT(optionType == cfgOptTypePath || optionType == cfgOptTypeString);

                                // Set string value to display value
                                configOptionValue->value.string = configOptionValue->display;

                                // Empty strings are not valid
                                if (strSize(value) == 0)
                                {
                                    THROW_FMT(
                                        OptionInvalidValueError, "'%s' must be >= 1 character for '%s' option", strZ(value),
                                        cfgParseOptionKeyIdxName(optionId, optionKeyIdx));
                                }

                                // If path make sure it is valid
                                if (optionType == cfgOptTypePath)
                                {
                                    // Make sure it starts with /
                                    if (!strBeginsWithZ(value, "/"))
                                    {
                                        THROW_FMT(
                                            OptionInvalidValueError, "'%s' must begin with / for '%s' option", strZ(value),
                                            cfgParseOptionKeyIdxName(optionId, optionKeyIdx));
                                    }

                                    // Make sure there are no occurrences of //
                                    if (strstr(strZ(value), "//") != NULL)
                                    {
                                        THROW_FMT(
                                            OptionInvalidValueError, "'%s' cannot contain // for '%s' option", strZ(value),
                                            cfgParseOptionKeyIdxName(optionId, optionKeyIdx));
                                    }

                                    // If the path ends with a / we'll strip it off (unless the value is just /)
                                    if (strEndsWithZ(value, "/") && strSize(value) != 1)
                                    {
                                        strTruncIdx(value, (int)strSize(value) - 1);

                                        // Reset string value since it was modified
                                        MEM_CONTEXT_BEGIN(config->memContext)
                                        {
                                            configOptionValue->value.string = strDup(value);
                                        }
                                        MEM_CONTEXT_END();
                                    }
                                }
                            }

                            // If the option has an allow list then check it
                            if (cfgParseOptionalRule(&optionalRules, parseRuleOptionalTypeAllowList, config->command, optionId))
                            {
                                PackRead *const allowList = pckReadNewC(optionalRules.allowList, optionalRules.allowListSize);
                                bool allowListFound = false;

                                pckReadNext(allowList);

                                while (true)
                                {
                                    // Compare based on option type
                                    const unsigned int valueIdx = pckReadU32P(allowList);

                                    switch (ruleOption->type)
                                    {
                                        case cfgOptTypeStringId:
                                            allowListFound = parseRuleValueStrId[valueIdx] == configOptionValue->value.stringId;
                                            break;

                                        default:
                                            allowListFound =
                                                cfgParseOptionValue(ruleOption->type, valueIdx) ==
                                                configOptionValue->value.integer;
                                            break;
                                    }

                                    // Stop when allow list is exhausted or value is found
                                    if (!cfgParseOptionValueCondition(
                                            pckReadNext(allowList), allowList, allowListFound, optionId, optionKeyIdx,
                                            valueAllow) ||
                                        allowListFound)
                                    {
                                        break;
                                    }
                                }

                                pckReadFree(allowList);

                                if (!allowListFound)
                                {
                                    PackRead *const allowList = pckReadNewC(optionalRules.allowList, optionalRules.allowListSize);
                                    String *const hintList = strNew();

                                    while (!pckReadNullP(allowList))
                                    {
                                        if (!strEmpty(hintList))
                                            strCatZ(hintList, ", ");

                                        strCatFmt(
                                            hintList, "'%s'",
                                            strZ(cfgParseOptionValueStr(ruleOption->type, pckReadU32P(allowList))));
                                    }

                                    THROW_FMT(
                                        OptionInvalidValueError,
                                        "'%s' is not allowed for '%s' option\n"
                                        "HINT: allowed values are %s",
                                        strZ(valueAllow), cfgParseOptionKeyIdxName(optionId, optionKeyIdx), strZ(hintList));
                                }
                            }
                        }
                    }
                    // Else set source when negated (value is already false)
                    else if (parseOptionValue->negate)
                    {
                        configOptionValue->source = parseOptionValue->source;
                    }
                    // Else try to set a default
                    else
                    {
                        bool found = false;

                        MEM_CONTEXT_BEGIN(config->memContext)
                        {
                            found = cfgParseOptionalRule(&optionalRules, parseRuleOptionalTypeDefault, config->command, optionId);
                        }
                        MEM_CONTEXT_END();

                        // If the option has a default
                        if (found)
                        {
                            configOptionValue->set = true;
                            configOptionValue->value = optionalRules.defaultValue;
                        }
                        // Else error if option is required and help was not requested
                        else
                        {
                            const bool required =
                                cfgParseOptionalRule(&optionalRules, parseRuleOptionalTypeRequired, config->command, optionId) ?
                                    optionalRules.required : ruleOption->required;

                            if (required && !config->help)
                            {
                                THROW_FMT(
                                    OptionRequiredError, "%s command requires option: %s%s",
                                    cfgParseCommandName(config->command), cfgParseOptionKeyIdxName(optionId, optionKeyIdx),
                                    ruleOption->section == cfgSectionStanza ? "\nHINT: does this stanza exist?" : "");
                            }
                        }
                    }

                    // If a non-default group option, keep the group index
                    if (optionGroup && configOptionValue->source != cfgSourceDefault)
                        optionGroupIndexKeep[optionGroupId][optionListIdx] = true;
                }
                // Else apply the default for the unresolved dependency, if it exists
                else if (dependResult.defaultExists)
                {
                    configOptionValue->set = true;
                    configOptionValue->value.boolean = dependResult.defaultValue;
                }

                pckReadFree(optionalRules.pack);
            }
        }

        // Phase 6: Remove any group indexes that have all default values (unless there is only one)
        //
        // It is possible that a group index was created for an option that was later found to not meet dependencies. In this case
        // all values will be default leading to a phantom group, which can be quite confusing. Remove all group indexes that are
        // all default (except the final one) and make sure the key for the final all default group index is 1.
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int optionGroupIdx = 0; optionGroupIdx < CFG_OPTION_GROUP_TOTAL; optionGroupIdx++)
        {
            ConfigOptionGroupData *const optionGroup = &config->optionGroup[optionGroupIdx];

            // Iterate group indexes
            for (unsigned int keyIdx = optionGroup->indexTotal - 1; keyIdx + 1 > 0; keyIdx--)
            {
                // Break if there is only one index since each group must have at least one
                if (optionGroup->indexTotal == 1)
                    break;

                // If the group index does not have a non-default value
                if (!optionGroupIndexKeep[optionGroupIdx][keyIdx])
                {
                    // Remove the value if it is not last
                    if (keyIdx < optionGroup->indexTotal - 1)
                    {
                        // Remove index key
                        memmove(
                            optionGroup->indexMap + keyIdx, optionGroup->indexMap + (keyIdx + 1),
                            sizeof(unsigned int) * (optionGroup->indexTotal - keyIdx - 1));

                        // Iterate all options
                        for (unsigned int optionIdx = 0; optionIdx < CFG_OPTION_TOTAL; optionIdx++)
                        {
                            ConfigOptionData *const option = &config->option[optionIdx];

                            // Remove the value if in the correct group
                            if (option->group && option->groupId == optionGroupIdx)
                            {
                                memmove(
                                    option->index + keyIdx, option->index + (keyIdx + 1),
                                    sizeof(ConfigOptionValue) * (optionGroup->indexTotal - keyIdx - 1));
                            }
                        }
                    }

                    // Decrement index total
                    optionGroup->indexTotal--;
                }
            }

            // If the remaining index contains all default values and is not key 1 then make it key 1. This prevents the key from
            // being determined by the key of an unused option.
            if (optionGroup->indexTotal == 1 && !optionGroupIndexKeep[optionGroupIdx][0] && optionGroup->indexMap[0] != 0)
                optionGroup->indexMap[0] = 0;
        }

        // Initialize config
        cfgInit(config);

        // Set option group default index. The first index in the group is automatically set unless the group option, e.g. pg, is
        // set. For now the group default options are hard-coded but they could be dynamic. An assert has been added to make sure
        // the code breaks if a new group is added.
        for (unsigned int groupId = 0; groupId < CFG_OPTION_GROUP_TOTAL; groupId++)
        {
            ASSERT(groupId == cfgOptGrpPg || groupId == cfgOptGrpRepo);

            // Get the group default option
            const unsigned int defaultOptionId = groupId == cfgOptGrpPg ? cfgOptPg : cfgOptRepo;

            // Does a default always exist?
            config->optionGroup[groupId].indexDefaultExists =
                // A default always exists for the pg group
                groupId == cfgOptGrpPg ||
                // The repo group allows a default when the repo option is valid, i.e. either repo1 is the only key set or a repo
                // is specified
                cfgOptionValid(cfgOptRepo);

            // Does the group default option exist?
            if (cfgOptionTest(defaultOptionId))
            {
                // Search for the key
                const unsigned int optionKeyIdx = cfgOptionUInt(defaultOptionId) - 1;
                unsigned int index = 0;

                for (; index < cfgOptionGroupIdxTotal(groupId); index++)
                {
                    if (config->optionGroup[groupId].indexMap[index] == optionKeyIdx)
                        break;
                }

                // Error if the key was not found
                if (index == cfgOptionGroupIdxTotal(groupId))
                {
                    THROW_FMT(
                        OptionInvalidValueError, "key '%s' is not valid for '%s' option", strZ(cfgOptionDisplay(defaultOptionId)),
                        cfgOptionName(defaultOptionId));
                }

                // Set the default
                config->optionGroup[groupId].indexDefault = index;
                config->optionGroup[groupId].indexDefaultExists = true;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
