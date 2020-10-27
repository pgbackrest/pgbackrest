/***********************************************************************************************************************************
Command and Option Parse
***********************************************************************************************************************************/
#include "build.auto.h"

#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/ini.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "config/config.intern.h"
#include "config/define.h"
#include "config/parse.h"
#include "storage/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Standard config file name and old default path and name
***********************************************************************************************************************************/
#define PGBACKREST_CONFIG_FILE                                      PROJECT_BIN ".conf"
#define PGBACKREST_CONFIG_ORIG_PATH_FILE                            "/etc/" PGBACKREST_CONFIG_FILE
    STRING_STATIC(PGBACKREST_CONFIG_ORIG_PATH_FILE_STR,             PGBACKREST_CONFIG_ORIG_PATH_FILE);

/***********************************************************************************************************************************
Prefix for environment variables
***********************************************************************************************************************************/
#define PGBACKREST_ENV                                              "PGBACKREST_"
#define PGBACKREST_ENV_SIZE                                         (sizeof(PGBACKREST_ENV) - 1)

// In some environments this will not be extern'd
extern char **environ;

/***********************************************************************************************************************************
Standard config include path name
***********************************************************************************************************************************/
#define PGBACKREST_CONFIG_INCLUDE_PATH                              "conf.d"

/***********************************************************************************************************************************
Option value constants
***********************************************************************************************************************************/
VARIANT_STRDEF_STATIC(OPTION_VALUE_0,                               "0");
VARIANT_STRDEF_STATIC(OPTION_VALUE_1,                               "1");

/***********************************************************************************************************************************
Parse option flags
***********************************************************************************************************************************/
// Offset the option values so they don't conflict with getopt_long return codes
#define PARSE_OPTION_FLAG                                           (1 << 30)

// Add a flag for negation rather than checking "--no-"
#define PARSE_NEGATE_FLAG                                           (1 << 29)

// Add a flag for reset rather than checking "--reset-"
#define PARSE_RESET_FLAG                                            (1 << 28)

// Indicate that option name has been deprecated and will be removed in a future release
#define PARSE_DEPRECATE_FLAG                                        (1 << 27)

// Mask for option id (must be 0-255)
#define PARSE_OPTION_MASK                                           0xFF

// Shift and mask for option key index (must be 0-255)
#define PARSE_KEY_IDX_SHIFT                                         8
#define PARSE_KEY_IDX_MASK                                          0xFF

/***********************************************************************************************************************************
Include automatically generated data structure for getopt_long()
***********************************************************************************************************************************/
#include "config/parse.auto.c"

/***********************************************************************************************************************************
Struct to hold options parsed from the command line
***********************************************************************************************************************************/
typedef struct ParseOptionValue
{
    bool found:1;                                                   // Was the option found?
    bool negate:1;                                                  // Was the option negated on the command line?
    bool reset:1;                                                   // Was the option reset on the command line?
    unsigned int source:2;                                          // Where was to option found?
    StringList *valueList;                                          // List of values found
} ParseOptionValue;

typedef struct ParseOption
{
    unsigned int indexListAlloc;                                    // Allocated size of index list
    unsigned int indexListTotal;                                    // Total options in indexed list
    ParseOptionValue *indexList;                                    // List of indexed option values
} ParseOption;

#define FUNCTION_LOG_PARSE_OPTION_FORMAT(value, buffer, bufferSize)                                                                \
    typeToLog("ParseOption", buffer, bufferSize)

/***********************************************************************************************************************************
Get the indexed value, creating the array to contain it if needed
***********************************************************************************************************************************/
static ParseOptionValue *
parseOptionIdxValue(ParseOption *optionList, unsigned int optionId, unsigned int optionIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PARSE_OPTION, parseOption);
        FUNCTION_TEST_PARAM(UINT, optionId);
        FUNCTION_TEST_PARAM(UINT, optionIdx);
    FUNCTION_TEST_END();

    if (optionIdx >= optionList[optionId].indexListTotal)
    {
        if (cfgOptionGroup(optionId))
        {
            unsigned int optionOffset = 0;

            if (optionList[optionId].indexListTotal == 0)
            {
                optionList[optionId].indexListTotal = optionIdx > 3 ? optionIdx + 1 : 4;
                optionList[optionId].indexList = memNew(sizeof(ParseOptionValue) * optionList[optionId].indexListTotal);
            }
            else
            {
                optionOffset = optionList[optionId].indexListTotal;
                optionList[optionId].indexListTotal *= 2;
                optionList[optionId].indexList = memResize(
                    optionList[optionId].indexList, sizeof(ParseOptionValue) * optionList[optionId].indexListTotal);
            }

            for (unsigned int optionIdx = optionOffset; optionIdx < optionList[optionId].indexListTotal; optionIdx++)
                optionList[optionId].indexList[optionIdx] = (ParseOptionValue){0};
        }
        else
        {
            optionList[optionId].indexList = memNew(sizeof(ParseOptionValue));
            optionList[optionId].indexListTotal = 1;
            optionList[optionId].indexList[0] = (ParseOptionValue){0};
        }
    }

    FUNCTION_TEST_RETURN(&optionList[optionId].indexList[optionIdx]);
}

/***********************************************************************************************************************************
Find an option by name in the option list
***********************************************************************************************************************************/
// Helper to parse the option info into a structure
__attribute__((always_inline)) static inline CfgParseOptionResult
cfgParseOptionInfo(int info)
{
    return (CfgParseOptionResult)
    {
        .found = true,
        .id = info & PARSE_OPTION_MASK,
        .keyIdx = (info >> PARSE_KEY_IDX_SHIFT) & PARSE_KEY_IDX_MASK,
        .negate = info & PARSE_NEGATE_FLAG,
        .reset = info & PARSE_RESET_FLAG,
        .deprecated = info & PARSE_DEPRECATE_FLAG,
    };
}

CfgParseOptionResult
cfgParseOption(const String *optionName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, optionName);
    FUNCTION_TEST_END();

    ASSERT(optionName != NULL);

    // Search for the option
    unsigned int findIdx = 0;

    while (optionList[findIdx].name != NULL)
    {
        if (strEqZ(optionName, optionList[findIdx].name))
            break;

        findIdx++;
    }

    // If the option was found
    if (optionList[findIdx].name != NULL)
        FUNCTION_TEST_RETURN(cfgParseOptionInfo(optionList[findIdx].val));

    FUNCTION_TEST_RETURN((CfgParseOptionResult){0});
}

/***********************************************************************************************************************************
Convert the value passed into bytes and update valueDbl for range checking
***********************************************************************************************************************************/
static double
sizeQualifierToMultiplier(char qualifier)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHAR, qualifier);
    FUNCTION_TEST_END();

    double result;

    switch (qualifier)
    {
        case 'b':
        {
            result = 1;
            break;
        }

        case 'k':
        {
            result = 1024;
            break;
        }

        case 'm':
        {
            result = 1024 * 1024;
            break;
        }

        case 'g':
        {
            result = 1024 * 1024 * 1024;
            break;
        }

        case 't':
        {
            result = 1024LL * 1024LL * 1024LL * 1024LL;
            break;
        }

        case 'p':
        {
            result = 1024LL * 1024LL * 1024LL * 1024LL * 1024LL;
            break;
        }

        default:
            THROW_FMT(AssertError, "'%c' is not a valid size qualifier", qualifier);
    }

    FUNCTION_TEST_RETURN(result);
}

static void
convertToByte(String **value, double *valueDbl)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(STRING, value);
        FUNCTION_LOG_PARAM_P(DOUBLE, valueDbl);
    FUNCTION_LOG_END();

    ASSERT(valueDbl != NULL);

    // Make a copy of the value so it is not updated until we know the conversion will succeed
    String *result = strLower(strDup(*value));

    // Match the value against possible values
    if (regExpMatchOne(STRDEF("^[0-9]+(kb|k|mb|m|gb|g|tb|t|pb|p|b)*$"), result))
    {
        // Get the character array and size
        const char *strArray = strZ(result);
        size_t size = strSize(result);
        int chrPos = -1;

        // If there is a 'b' on the end, then see if the previous character is a number
        if (strArray[size - 1] == 'b')
        {
            // If the previous character is a number, then the letter to look at is 'b' which is the last position else it is in the
            // next to last position (e.g. kb - so the 'k' is the position of interest).  Only need to test for <= 9 since the regex
            // enforces the format.
            if (strArray[size - 2] <= '9')
                chrPos = (int)(size - 1);
            else
                chrPos = (int)(size - 2);
        }
        // else if there is no 'b' at the end but the last position is not a number then it must be one of the letters, e.g. 'k'
        else if (strArray[size - 1] > '9')
            chrPos = (int)(size - 1);

        double multiplier = 1;

        // If a letter was found calculate multiplier, else do nothing since assumed value is already in bytes
        if (chrPos != -1)
        {
            multiplier = sizeQualifierToMultiplier(strArray[chrPos]);

            // Remove any letters
            strTrunc(result, chrPos);
        }

        // Convert string to bytes
        double newDbl = varDblForce(VARSTR(result)) * multiplier;
        result = varStrForce(VARDBL(newDbl));

        // If nothing has blown up then safe to overwrite the original values
        *valueDbl = newDbl;
        *value = result;
    }
    else
        THROW_FMT(FormatError, "value '%s' is not valid", strZ(*value));

    FUNCTION_LOG_RETURN_VOID();
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
cfgFileLoadPart(String **config, const Buffer *configPart)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(STRING, config);
        FUNCTION_LOG_PARAM(BUFFER, configPart);
    FUNCTION_LOG_END();

    if (configPart != NULL)
    {
        String *configPartStr = strNewBuf(configPart);

        // Validate the file by parsing it as an Ini object. If the file is not properly formed, an error will occur.
        if (strSize(configPartStr) > 0)
        {
            Ini *configPartIni = iniNew();
            iniParse(configPartIni, configPartStr);

            // Create the result config file
            if (*config == NULL)
                *config = strNew("");
            // Else add an LF in case the previous file did not end with one
            else

            // Add the config part to the result config file
            strCat(*config, LF_STR);
            strCat(*config, configPartStr);
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

static String *
cfgFileLoad(                                                        // NOTE: Passing defaults to enable more complete test coverage
    const ParseOption *optionList,                                  // All options and their current settings
    const String *optConfigDefault,                                 // Current default for --config option
    const String *optConfigIncludePathDefault,                      // Current default for --config-include-path option
    const String *origConfigDefault)                                // Original --config option default (/etc/pgbackrest.conf)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(PARSE_OPTION, optionList);
        FUNCTION_LOG_PARAM(STRING, optConfigDefault);
        FUNCTION_LOG_PARAM(STRING, optConfigIncludePathDefault);
        FUNCTION_LOG_PARAM(STRING, origConfigDefault);
    FUNCTION_LOG_END();

    ASSERT(optionList != NULL);
    ASSERT(optConfigDefault != NULL);
    ASSERT(optConfigIncludePathDefault != NULL);
    ASSERT(origConfigDefault != NULL);

    bool loadConfig = true;
    bool loadConfigInclude = true;

    // If the option is specified on the command line, then found will be true meaning the file is required to exist,
    // else it is optional
    bool configFound = optionList[cfgOptConfig].indexList != NULL && optionList[cfgOptConfig].indexList[0].found;
    bool configRequired = configFound;
    bool configPathRequired = optionList[cfgOptConfigPath].indexList != NULL && optionList[cfgOptConfigPath].indexList[0].found;
    bool configIncludeRequired =
        optionList[cfgOptConfigIncludePath].indexList != NULL && optionList[cfgOptConfigIncludePath].indexList[0].found;

    // Save default for later determining if must check old original default config path
    const String *optConfigDefaultCurrent = optConfigDefault;

    // If the config-path option is found on the command line, then its value will override the base path defaults for config and
    // config-include-path
    if (configPathRequired)
    {
        optConfigDefault = strNewFmt(
            "%s/%s", strZ(strLstGet(optionList[cfgOptConfigPath].indexList[0].valueList, 0)), strBaseZ(optConfigDefault));
        optConfigIncludePathDefault = strNewFmt(
            "%s/%s", strZ(strLstGet(optionList[cfgOptConfigPath].indexList[0].valueList, 0)), PGBACKREST_CONFIG_INCLUDE_PATH);
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
        Buffer *buffer = storageGetP(storageNewReadP(storageLocal(), configFileName, .ignoreMissing = !configRequired));

        // Convert the contents of the file buffer to the config string object
        if (buffer != NULL)
            result = strNewBuf(buffer);
        else if (strEq(configFileName, optConfigDefaultCurrent))
        {
            // If confg is current default and it was not found, attempt to load the config file from the old default location
            buffer = storageGetP(storageNewReadP(storageLocal(), origConfigDefault, .ignoreMissing = !configRequired));

            if (buffer != NULL)
                result = strNewBuf(buffer);
        }
    }

    // Load *.conf files from the include directory
    if (loadConfigInclude)
    {
        if (result != NULL)
        {
            // Validate the file by parsing it as an Ini object. If the file is not properly formed, an error will occur.
            Ini *ini = iniNew();
            iniParse(ini, result);
        }

        const String *configIncludePath = NULL;

        // Get the config include path from the command-line if it exists else default
        if (configIncludeRequired)
            configIncludePath = strLstGet(optionList[cfgOptConfigIncludePath].indexList[0].valueList, 0);
        else
            configIncludePath = optConfigIncludePathDefault;

        // Get a list of conf files from the specified path -error on missing directory if the option was passed on the command line
        StringList *list = storageListP(
            storageLocal(), configIncludePath, .expression = STRDEF(".+\\.conf$"), .errorOnMissing = configIncludeRequired,
            .nullOnMissing = !configIncludeRequired);

        // If conf files are found, then add them to the config string
        if (list != NULL && strLstSize(list) > 0)
        {
            // Sort the list for reproducibility only -- order does not matter
            strLstSort(list, sortOrderAsc);

            for (unsigned int listIdx = 0; listIdx < strLstSize(list); listIdx++)
            {
                cfgFileLoadPart(
                    &result,
                    storageGetP(
                        storageNewReadP(
                            storageLocal(), strNewFmt("%s/%s", strZ(configIncludePath), strZ(strLstGet(list, listIdx))),
                            .ignoreMissing = true)));
            }
        }
    }

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
??? Add validation of section names and check all sections for invalid options in the check command.  It's too expensive to add the
logic to this critical path code.
***********************************************************************************************************************************/
void
configParse(unsigned int argListSize, const char *argList[], bool resetLogLevel)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(UINT, argListSize);
        FUNCTION_LOG_PARAM(CHARPY, argList);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create the config struct
        Config *config;

        MEM_CONTEXT_NEW_BEGIN("Config")
        {
            config = memNew(sizeof(Config));

            *config = (Config)
            {
                .memContext = MEM_CONTEXT_NEW(),
                .command = cfgCmdNone,
                .exe = strNew(argList[0]),
            };
        }
        MEM_CONTEXT_NEW_END();

        // Phase 1: parse command line parameters
        // -------------------------------------------------------------------------------------------------------------------------
        int optionValue;                                                // Value returned by getopt_long
        int optionListIdx;                                              // Index of option is list (if an option was returned)
        bool argFound = false;                                          // Track args found to decide on error or help at the end

        // Reset optind to 1 in case getopt_long has been called before
        optind = 1;

        // Don't error automatically on unknown options - they will be processed in the loop below
        opterr = false;

        // List of parsed options
        ParseOption parseOptionList[CFG_OPTION_TOTAL] = {{0}};

        // Only the first non-option parameter should be treated as a command so track if the command has been set
        bool commandSet = false;

        while ((optionValue = getopt_long((int)argListSize, (char **)argList, "-:", optionList, &optionListIdx)) != -1)
        {
            switch (optionValue)
            {
                // Parse arguments that are not options, i.e. commands and parameters passed to commands
                case 1:
                {
                    // The first argument should be the command
                    if (!commandSet)
                    {
                        const char *command = argList[optind - 1];

                        // Try getting the command from the valid command list
                        config->command = cfgCommandId(command);
                        config->commandRole = cfgCmdRoleDefault;

                        // If not successful then a command role may be appended
                        if (config->command == cfgCmdNone)
                        {
                            const StringList *commandPart = strLstNewSplit(STR(command), COLON_STR);

                            if (strLstSize(commandPart) == 2)
                            {
                                // Get command id
                                config->command = cfgCommandId(strZ(strLstGet(commandPart, 0)));

                                // If command id is valid then get command role id
                                if (config->command != cfgCmdNone)
                                    config->commandRole = cfgCommandRoleEnum(strLstGet(commandPart, 1));
                            }
                        }

                        // Error when command does not exist
                        if (config->command == cfgCmdNone)
                            THROW_FMT(CommandInvalidError, "invalid command '%s'", command);

                        if (config->command == cfgCmdHelp)
                            config->help = true;
                        else
                            commandSet = true;
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

                        strLstAdd(config->paramList, strNew(argList[optind - 1]));
                    }

                    break;
                }

                // If the option is unknown then error
                case '?':
                    THROW_FMT(OptionInvalidError, "invalid option '%s'", argList[optind - 1]);

                // If the option is missing an argument then error
                case ':':
                    THROW_FMT(OptionInvalidError, "option '%s' requires argument", argList[optind - 1]);

                // Parse valid option
                default:
                {
                    // Get option id and flags from the option code
                    CfgParseOptionResult option = cfgParseOptionInfo(optionValue);

                    // Make sure the option id is valid
                    ASSERT(option.id < CFG_OPTION_TOTAL);

                    // Error if this option is secure and cannot be passed on the command line
                    if (cfgDefOptionSecure(option.id))
                    {
                        THROW_FMT(
                            OptionInvalidError,
                            "option '%s' is not allowed on the command-line\n"
                            "HINT: this option could expose secrets in the process list.\n"
                            "HINT: specify the option in a configuration file or an environment variable instead.",
                            cfgOptionKeyIdxName(option.id, option.keyIdx));
                    }

                    // If the option has not been found yet then set it
                    ParseOptionValue *optionValue = parseOptionIdxValue(parseOptionList, option.id, option.keyIdx);

                    if (!optionValue->found)
                    {
                        *optionValue = (ParseOptionValue)
                        {
                            .found = true,
                            .negate = option.negate,
                            .reset = option.reset,
                            .source = cfgSourceParam,
                        };

                        // Only set the argument if the option requires one
                        if (optionList[optionListIdx].has_arg == required_argument)
                        {
                            optionValue->valueList = strLstNew();
                            strLstAdd(optionValue->valueList, STR(optarg));
                        }
                    }
                    else
                    {
                        // Make sure option is not negated more than once.  It probably wouldn't hurt anything to accept this case
                        // but there's no point in allowing the user to be sloppy.
                        if (optionValue->negate && option.negate)
                        {
                            THROW_FMT(
                                OptionInvalidError, "option '%s' is negated multiple times",
                                cfgOptionKeyIdxName(option.id, option.keyIdx));
                        }

                        // Make sure option is not reset more than once.  Same justification as negate.
                        if (optionValue->reset && option.reset)
                        {
                            THROW_FMT(
                                OptionInvalidError, "option '%s' is reset multiple times",
                                cfgOptionKeyIdxName(option.id, option.keyIdx));
                        }

                        // Don't allow an option to be both negated and reset
                        if ((optionValue->reset && option.negate) || (optionValue->negate && option.reset))
                        {
                            THROW_FMT(
                                OptionInvalidError, "option '%s' cannot be negated and reset",
                                cfgOptionKeyIdxName(option.id, option.keyIdx));
                        }

                        // Don't allow an option to be both set and negated
                        if (optionValue->negate != option.negate)
                        {
                            THROW_FMT(
                                OptionInvalidError, "option '%s' cannot be set and negated",
                                cfgOptionKeyIdxName(option.id, option.keyIdx));
                        }

                        // Don't allow an option to be both set and reset
                        if (optionValue->reset != option.reset)
                        {
                            THROW_FMT(
                                OptionInvalidError, "option '%s' cannot be set and reset",
                                cfgOptionKeyIdxName(option.id, option.keyIdx));
                        }

                        // Add the argument
                        if (optionList[optionListIdx].has_arg == required_argument && cfgDefOptionMulti(option.id))
                        {
                            strLstAdd(optionValue->valueList, strNew(optarg));
                        }
                        // Error if the option does not accept multiple arguments
                        else
                        {
                            THROW_FMT(
                                OptionInvalidError, "option '%s' cannot be set multiple times",
                                cfgOptionKeyIdxName(option.id, option.keyIdx));
                        }
                    }

                    break;
                }
            }

            // Arg has been found
            argFound = true;
        }

        // Handle command not found
        if (!commandSet && !config->help)
        {
            // If there are args then error
            if (argFound)
                THROW_FMT(CommandRequiredError, "no command found");

            // Otherwise set the command to help
            config->help = true;
        }

        // Error when parameters found but the command does not allow parameters
        if (config->paramList != NULL && !config->help && !cfgCommandParameterAllowed(config->command))
            THROW(ParamInvalidError, "command does not allow parameters");

        // Enable logging (except for local and remote commands) so config file warnings will be output
        if (config->commandRole != cfgCmdRoleLocal && config->commandRole != cfgCmdRoleRemote && resetLogLevel)
            logInit(logLevelWarn, logLevelWarn, logLevelOff, false, 0, 1, false);

        // Only continue if command options need to be validated, i.e. a real command is running or we are getting help for a
        // specific command and would like to display actual option values in the help.
        if (config->command != cfgCmdNone && config->command != cfgCmdVersion && config->command != cfgCmdHelp)
        {
            // Phase 2: parse environment variables
            // ---------------------------------------------------------------------------------------------------------------------
            unsigned int environIdx = 0;

            // Loop through all environment variables and look for our env vars by matching the prefix
            while (environ[environIdx] != NULL)
            {
                const char *keyValue = environ[environIdx];
                environIdx++;

                if (strstr(keyValue, PGBACKREST_ENV) == keyValue)
                {
                    // Find the first = char
                    const char *equalPtr = strchr(keyValue, '=');
                    ASSERT(equalPtr != NULL);

                    // Get key and value
                    const String *key = strReplaceChr(
                        strLower(strNewN(keyValue + PGBACKREST_ENV_SIZE, (size_t)(equalPtr - (keyValue + PGBACKREST_ENV_SIZE)))),
                        '_', '-');
                    const String *value = STR(equalPtr + 1);

                    // Find the option
                    CfgParseOptionResult option = cfgParseOption(key);

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
                    if (!cfgDefOptionValid(config->command, option.id))
                        continue;

                    if (strSize(value) == 0)
                        THROW_FMT(OptionInvalidValueError, "environment variable '%s' must have a value", strZ(key));

                    // Continue if the option has already been specified on the command line
                    ParseOptionValue *optionValue = parseOptionIdxValue(
                        parseOptionList, option.id, option.keyIdx);

                    if (optionValue->found)
                        continue;

                    optionValue->found = true;
                    optionValue->source = cfgSourceConfig;

                    // Convert boolean to string
                    if (cfgDefOptionType(option.id) == cfgDefOptTypeBoolean)
                    {
                        if (strEqZ(value, "n"))
                            optionValue->negate = true;
                        else if (!strEqZ(value, "y"))
                            THROW_FMT(OptionInvalidValueError, "environment boolean option '%s' must be 'y' or 'n'", strZ(key));
                    }
                    // Else split list/hash into separate values
                    else if (cfgDefOptionMulti(option.id))
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

            // Phase 3: parse config file unless --no-config passed
            // ---------------------------------------------------------------------------------------------------------------------
            // Load the configuration file(s)
            String *configString = cfgFileLoad(
                parseOptionList, STR(cfgDefOptionDefault(config->command, cfgOptConfig)),
                STR(cfgDefOptionDefault(config->command, cfgOptConfigIncludePath)), PGBACKREST_CONFIG_ORIG_PATH_FILE_STR);

            if (configString != NULL)
            {
                Ini *ini = iniNew();
                iniParse(ini, configString);
                // Get the stanza name
                String *stanza = NULL;

                if (parseOptionList[cfgOptStanza].indexList != NULL)
                    stanza = strLstGet(parseOptionList[cfgOptStanza].indexList[0].valueList, 0);

                // Build list of sections to search for options
                StringList *sectionList = strLstNew();

                if (stanza != NULL)
                {
                    strLstAdd(sectionList, strNewFmt("%s:%s", strZ(stanza), cfgCommandName(config->command)));
                    strLstAdd(sectionList, stanza);
                }

                strLstAdd(sectionList, strNewFmt(CFGDEF_SECTION_GLOBAL ":%s", cfgCommandName(config->command)));
                strLstAdd(sectionList, CFGDEF_SECTION_GLOBAL_STR);

                // Loop through sections to search for options
                for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
                {
                    String *section = strLstGet(sectionList, sectionIdx);
                    StringList *keyList = iniSectionKeyList(ini, section);
                    KeyValue *optionFound = kvNew();

                    // Loop through keys to search for options
                    for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
                    {
                        String *key = strLstGet(keyList, keyIdx);

                        // Find the optionName in the main list
                        CfgParseOptionResult option = cfgParseOption(key);

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
                        if (cfgDefOptionSection(option.id) == cfgDefSectionCommandLine)
                        {
                            LOG_WARN_FMT("configuration file contains command-line only option '%s'", strZ(key));
                            continue;
                        }

                        // Make sure this option does not appear in the same section with an alternate name
                        const Variant *optionFoundKey = VARUINT64(option.id * CFG_OPTION_KEY_MAX + option.keyIdx);
                        const Variant *optionFoundName = kvGet(optionFound, optionFoundKey);

                        if (optionFoundName != NULL)
                        {
                            THROW_FMT(
                                OptionInvalidError, "configuration file contains duplicate options ('%s', '%s') in section '[%s]'",
                                strZ(key), strZ(varStr(optionFoundName)), strZ(section));
                        }
                        else
                            kvPut(optionFound, optionFoundKey, VARSTR(key));

                        // Continue if the option is not valid for this command
                        if (!cfgDefOptionValid(config->command, option.id))
                        {
                            // Warn if it is in a command section
                            if (sectionIdx % 2 == 0)
                            {
                                LOG_WARN_FMT(
                                    "configuration file contains option '%s' invalid for section '%s'", strZ(key),
                                    strZ(section));
                                continue;
                            }

                            continue;
                        }

                        // Continue if stanza option is in a global section
                        if (cfgDefOptionSection(option.id) == cfgDefSectionStanza && strBeginsWithZ(section, CFGDEF_SECTION_GLOBAL))
                        {
                            LOG_WARN_FMT(
                                "configuration file contains stanza-only option '%s' in global section '%s'", strZ(key),
                                strZ(section));
                            continue;
                        }

                        // Continue if this option has already been found in another section or command-line/environment
                        ParseOptionValue *optionValue = parseOptionIdxValue(parseOptionList, option.id, option.keyIdx);

                        if (optionValue->found)
                            continue;

                        optionValue->found = true;
                        optionValue->source = cfgSourceConfig;

                        // Process list
                        if (iniSectionKeyIsList(ini, section, key))
                        {
                            // Error if the option cannot be specified multiple times
                            if (!cfgDefOptionMulti(option.id))
                            {
                                THROW_FMT(
                                    OptionInvalidError, "option '%s' cannot be set multiple times",
                                    cfgOptionKeyIdxName(option.id, option.keyIdx));
                            }

                            optionValue->valueList = iniGetList(ini, section, key);
                        }
                        else
                        {
                            // Get the option value
                            const String *value = iniGet(ini, section, key);

                            if (strSize(value) == 0)
                            {
                                THROW_FMT(
                                    OptionInvalidValueError, "section '%s', key '%s' must have a value", strZ(section),
                                    strZ(key));
                            }

                            if (cfgDefOptionType(option.id) == cfgDefOptTypeBoolean)
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
            // ---------------------------------------------------------------------------------------------------------------------
            // Determine how many indexes are used in each group
            bool groupIdxMap[CFG_OPTION_GROUP_TOTAL][CFG_OPTION_KEY_MAX] = {{0}};

            for (unsigned int optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
            {
                // Is the option valid for this command?
                if (cfgDefOptionValid(config->command, optionId))
                {
                    config->option[optionId].valid = true;
                }
                else
                {
                    // Error if the invalid option was explicitly set on the command-line
                    if (parseOptionList[optionId].indexList != NULL)
                    {
                        THROW_FMT(
                            OptionInvalidError, "option '%s' not valid for command '%s'", cfgDefOptionName(optionId),
                            cfgCommandName(config->command));
                    }

                    // Continue to the next option
                    continue;
                }

                if (cfgOptionGroup(optionId))
                {
                    unsigned int groupId = cfgOptionGroupId(optionId);

                    config->optionGroup[groupId].valid = true;

                    for (unsigned int optionIdx = 0; optionIdx < parseOptionList[optionId].indexListTotal; optionIdx++)
                    {
                        if (parseOptionList[optionId].indexList[optionIdx].found &&
                            !parseOptionList[optionId].indexList[optionIdx].reset)
                        {
                            if (!groupIdxMap[groupId][optionIdx])
                            {
                                config->optionGroup[groupId].indexTotal++;
                                groupIdxMap[groupId][optionIdx] = true;
                            }
                        }
                    }
                }
            }

            // Write the indexes into the group in order
            for (unsigned int groupId = 0; groupId < CFG_OPTION_GROUP_TOTAL; groupId++)
            {
                // Skip the group if it is not valid
                if (!config->optionGroup[groupId].valid)
                    continue;

                // If no values were found in any index then use index 0 since all valid groups must have at least one index. This
                // may lead to an error unless all options in the group have defaults but that will be resolved later.
                if (config->optionGroup[groupId].indexTotal == 0)
                {
                    config->optionGroup[groupId].indexTotal = 1;
                }
                // Else determine which group indexes have values
                else
                {
                    unsigned int optionIdxMax = 0;

                    for (unsigned int optionIdx = 0; optionIdx < CFG_OPTION_KEY_MAX; optionIdx++)
                    {
                        if (groupIdxMap[groupId][optionIdx])
                        {
                            config->optionGroup[groupId].index[optionIdxMax] = optionIdx;
                            optionIdxMax++;
                        }
                    }
                }
            }

            // Phase 5: validate option definitions and load into configuration
            // ---------------------------------------------------------------------------------------------------------------------
            for (unsigned int optionOrderIdx = 0; optionOrderIdx < CFG_OPTION_TOTAL; optionOrderIdx++)
            {
                // Validate options based on the option resolve order.  This allows resolving all options in a single pass.
                ConfigOption optionId = optionResolveOrder[optionOrderIdx];

                // Skip this option if it is not valid
                if (!config->option[optionId].valid)
                    continue;

                // Determine the option index total. For options that are not indexed the index total is 1.
                bool optionGroup = cfgOptionGroup(optionId);
                unsigned int optionGroupId = optionGroup ? cfgOptionGroupId(optionId) : UINT_MAX;
                unsigned int optionListIndexTotal = optionGroup ? config->optionGroup[optionGroupId].indexTotal : 1;

                MEM_CONTEXT_BEGIN(config->memContext)
                {
                    config->option[optionId].index = memNew(sizeof(ConfigOptionValue) * optionListIndexTotal);
                }
                MEM_CONTEXT_END();

                // Loop through the option indexes
                ConfigDefineOptionType optionDefType = cfgDefOptionType(optionId);

                for (unsigned int optionListIdx = 0; optionListIdx < optionListIndexTotal; optionListIdx++)
                {
                    unsigned optionIdx = optionGroup ? config->optionGroup[optionGroupId].index[optionListIdx] : 0;
                    ParseOptionValue *parseOptionValue = optionIdx < parseOptionList[optionId].indexListTotal ?
                        &parseOptionList[optionId].indexList[optionIdx] : &(ParseOptionValue){0};
                    ConfigOptionValue *configOptionValue = &config->option[optionId].index[optionListIdx];

                    // Is the value set for this option?
                    bool optionSet =
                        parseOptionValue->found && (optionDefType == cfgDefOptTypeBoolean || !parseOptionValue->negate) &&
                        !parseOptionValue->reset;

                    // Initialize option value and set negate and reset flag
                    *configOptionValue = (ConfigOptionValue){.negate = parseOptionValue->negate, .reset = parseOptionValue->reset};

                    // Check option dependencies
                    bool dependResolved = true;

                    if (cfgDefOptionDepend(config->command, optionId))
                    {
                        ConfigOption dependOptionId = cfgDefOptionDependOption(config->command, optionId);
                        ConfigDefineOptionType dependOptionDefType = cfgDefOptionType(dependOptionId);

                        // Get the depend option value
                        const Variant *dependValue = config->option[dependOptionId].index[optionListIdx].value;

                        if (dependValue != NULL)
                        {
                            if (dependOptionDefType == cfgDefOptTypeBoolean)
                            {
                                if (varBool(dependValue))
                                    dependValue = OPTION_VALUE_1;
                                else
                                    dependValue = OPTION_VALUE_0;
                            }
                        }

                        // Can't resolve if the depend option value is null
                        if (dependValue == NULL)
                        {
                            dependResolved = false;

                            // If depend not resolved and option value is set on the command-line then error.  See unresolved list
                            // depend below for a detailed explanation.
                            if (optionSet && parseOptionValue->source == cfgSourceParam)
                            {
                                THROW_FMT(
                                    OptionInvalidError, "option '%s' not valid without option '%s'",
                                    cfgOptionKeyIdxName(optionId, optionIdx), cfgOptionKeyIdxName(dependOptionId, optionIdx));
                            }
                        }
                        // If a depend list exists, make sure the value is in the list
                        else if (cfgDefOptionDependValueTotal(config->command, optionId) > 0)
                        {
                            dependResolved = cfgDefOptionDependValueValid(config->command, optionId, strZ(varStr(dependValue)));

                            // If depend not resolved and option value is set on the command-line then error.  It's OK to have
                            // unresolved options in the config file because they may be there for another command.  For instance,
                            // spool-path is only loaded for the archive-push command when archive-async=y, and the presence of
                            // spool-path in the config file should not cause an error here, it will just end up null.
                            if (!dependResolved && optionSet && parseOptionValue->source == cfgSourceParam)
                            {
                                // Get the depend option name
                                const String *dependOptionName = STR(cfgOptionKeyIdxName(dependOptionId, optionIdx));

                                // Build the list of possible depend values
                                StringList *dependValueList = strLstNew();

                                for (unsigned int listIdx = 0;
                                        listIdx < cfgDefOptionDependValueTotal(config->command, optionId); listIdx++)
                                {
                                    const char *dependValue = cfgDefOptionDependValue(config->command, optionId, listIdx);

                                    // Build list based on depend option type
                                    if (dependOptionDefType == cfgDefOptTypeBoolean)
                                    {
                                        // Boolean outputs depend option name as no-* when false
                                        if (strcmp(dependValue, "0") == 0)
                                            dependOptionName = strNewFmt("no-%s", cfgOptionKeyIdxName(dependOptionId, optionIdx));
                                    }
                                    else
                                    {
                                        ASSERT(dependOptionDefType == cfgDefOptTypePath || dependOptionDefType == cfgDefOptTypeString);
                                        strLstAdd(dependValueList, strNewFmt("'%s'", dependValue));
                                    }
                                }

                                // Build the error string
                                const String *errorValue = EMPTY_STR;

                                if (strLstSize(dependValueList) == 1)
                                    errorValue = strNewFmt(" = %s", strZ(strLstGet(dependValueList, 0)));
                                else if (strLstSize(dependValueList) > 1)
                                    errorValue = strNewFmt(" in (%s)", strZ(strLstJoin(dependValueList, ", ")));

                                // Throw the error
                                THROW(
                                    OptionInvalidError,
                                    strZ(
                                        strNewFmt(
                                            "option '%s' not valid without option '%s'%s", cfgOptionKeyIdxName(optionId, optionIdx),
                                            strZ(dependOptionName), strZ(errorValue))));
                            }
                        }
                    }

                    // Is the option resolved?
                    if (dependResolved)
                    {
                        // Is the option set?
                        if (optionSet)
                        {
                            configOptionValue->source = parseOptionValue->source;

                            if (optionDefType == cfgDefOptTypeBoolean)
                            {
                                configOptionValue->value = !parseOptionValue->negate ? BOOL_TRUE_VAR : BOOL_FALSE_VAR;
                            }
                            else if (optionDefType == cfgDefOptTypeHash)
                            {
                                Variant *value = NULL;

                                MEM_CONTEXT_BEGIN(config->memContext)
                                {
                                    value = varNewKv(kvNew());
                                }
                                MEM_CONTEXT_END();

                                KeyValue *keyValue = varKv(value);

                                for (unsigned int listIdx = 0; listIdx < strLstSize(parseOptionValue->valueList); listIdx++)
                                {
                                    const char *pair = strZ(strLstGet(parseOptionValue->valueList, listIdx));
                                    const char *equal = strchr(pair, '=');

                                    if (equal == NULL)
                                    {
                                        THROW_FMT(
                                            OptionInvalidError, "key/value '%s' not valid for '%s' option",
                                            strZ(strLstGet(parseOptionValue->valueList, listIdx)),
                                            cfgOptionKeyIdxName(optionId, optionIdx));
                                    }

                                    kvPut(keyValue, VARSTR(strNewN(pair, (size_t)(equal - pair))), VARSTRZ(equal + 1));
                                }

                                configOptionValue->value = value;
                            }
                            else if (optionDefType == cfgDefOptTypeList)
                            {
                                MEM_CONTEXT_BEGIN(config->memContext)
                                {
                                    configOptionValue->value = varNewVarLst(varLstNewStrLst(parseOptionValue->valueList));
                                }
                                MEM_CONTEXT_END();
                            }
                            else
                            {
                                String *value = strLstGet(parseOptionValue->valueList, 0);

                                // If a numeric type check that the value is valid
                                if (optionDefType == cfgDefOptTypeInteger || optionDefType == cfgDefOptTypeFloat ||
                                    optionDefType == cfgDefOptTypeSize)
                                {
                                    double valueDbl = 0;

                                    // Check that the value can be converted
                                    TRY_BEGIN()
                                    {
                                        if (optionDefType == cfgDefOptTypeInteger)
                                        {
                                            MEM_CONTEXT_BEGIN(config->memContext)
                                            {
                                                configOptionValue->value = varNewInt64(cvtZToInt64(strZ(value)));
                                            }
                                            MEM_CONTEXT_END();

                                            valueDbl = (double)varInt64(configOptionValue->value);
                                        }
                                        else if (optionDefType == cfgDefOptTypeSize)
                                        {
                                            convertToByte(&value, &valueDbl);

                                            MEM_CONTEXT_BEGIN(config->memContext)
                                            {
                                                configOptionValue->value = varNewInt64((int64_t)valueDbl);
                                            }
                                            MEM_CONTEXT_END();
                                        }
                                        else
                                        {
                                            MEM_CONTEXT_BEGIN(config->memContext)
                                            {
                                                configOptionValue->value = varNewDbl(cvtZToDouble(strZ(value)));
                                            }
                                            MEM_CONTEXT_END();

                                            valueDbl = varDbl(configOptionValue->value);
                                        }
                                    }
                                    CATCH_ANY()
                                    {
                                        THROW_FMT(
                                            OptionInvalidValueError, "'%s' is not valid for '%s' option", strZ(value),
                                            cfgOptionKeyIdxName(optionId, optionIdx));
                                    }
                                    TRY_END();

                                    // Check value range
                                    if (cfgDefOptionAllowRange(config->command, optionId) &&
                                        (valueDbl < cfgDefOptionAllowRangeMin(config->command, optionId) ||
                                         valueDbl > cfgDefOptionAllowRangeMax(config->command, optionId)))
                                    {
                                        THROW_FMT(
                                            OptionInvalidValueError, "'%s' is out of range for '%s' option", strZ(value),
                                            cfgOptionKeyIdxName(optionId, optionIdx));
                                    }
                                }
                                // Else if path make sure it is valid
                                else
                                {
                                    // Make sure it is long enough to be a path
                                    if (strSize(value) == 0)
                                    {
                                        THROW_FMT(
                                            OptionInvalidValueError, "'%s' must be >= 1 character for '%s' option", strZ(value),
                                            cfgOptionKeyIdxName(optionId, optionIdx));
                                    }

                                    if (optionDefType == cfgDefOptTypePath)
                                    {
                                        // Make sure it starts with /
                                        if (!strBeginsWithZ(value, "/"))
                                        {
                                            THROW_FMT(
                                                OptionInvalidValueError, "'%s' must begin with / for '%s' option", strZ(value),
                                                cfgOptionKeyIdxName(optionId, optionIdx));
                                        }

                                        // Make sure there are no occurrences of //
                                        if (strstr(strZ(value), "//") != NULL)
                                        {
                                            THROW_FMT(
                                                OptionInvalidValueError, "'%s' cannot contain // for '%s' option", strZ(value),
                                                cfgOptionKeyIdxName(optionId, optionIdx));
                                        }

                                        // If the path ends with a / we'll strip it off (unless the value is just /)
                                        if (strEndsWithZ(value, "/") && strSize(value) != 1)
                                            strTrunc(value, (int)strSize(value) - 1);
                                    }

                                    MEM_CONTEXT_BEGIN(config->memContext)
                                    {
                                        configOptionValue->value = varNewStr(value);
                                    }
                                    MEM_CONTEXT_END();
                                }

                                // If the option has an allow list then check it
                                if (cfgDefOptionAllowList(config->command, optionId) &&
                                    !cfgDefOptionAllowListValueValid(config->command, optionId, strZ(value)))
                                {
                                    THROW_FMT(
                                        OptionInvalidValueError, "'%s' is not allowed for '%s' option", strZ(value),
                                        cfgOptionKeyIdxName(optionId, optionIdx));
                                }
                            }
                        }
                        else if (parseOptionValue->negate)
                            configOptionValue->source = parseOptionValue->source;
                        // Else try to set a default
                        else
                        {
                            // Get the default value for this option
                            const char *value = cfgDefOptionDefault(config->command, optionId);

                            // If the option has a default
                            if (value != NULL)
                            {
                                MEM_CONTEXT_BEGIN(config->memContext)
                                {
                                    // This would typically be a switch but since not all cases are covered it would require a
                                    // separate function which does not seem worth it. The eventual plan is to have all the defaults
                                    // represented as constants so they can be assigned directly without creating variants.
                                    if (optionDefType == cfgDefOptTypeBoolean)
                                        configOptionValue->value = strcmp(value, "1") == 0 ? BOOL_TRUE_VAR : BOOL_FALSE_VAR;
                                    else if (optionDefType == cfgDefOptTypeFloat)
                                        configOptionValue->value = varNewDbl(cvtZToDouble(value));
                                    else if (optionDefType == cfgDefOptTypeInteger || optionDefType == cfgDefOptTypeSize)
                                        configOptionValue->value = varNewInt64(cvtZToInt64(value));
                                    else
                                    {
                                        ASSERT(optionDefType == cfgDefOptTypePath || optionDefType == cfgDefOptTypeString);

                                        configOptionValue->value = varNewStrZ(value);
                                    }
                                }
                                MEM_CONTEXT_END();
                            }
                            // Else error if option is required and help was not requested
                            else if (cfgDefOptionRequired(config->command, optionId) && !config->help)
                            {
                                const char *hint = "";

                                if (cfgDefOptionSection(optionId) == cfgDefSectionStanza)
                                    hint = "\nHINT: does this stanza exist?";

                                THROW_FMT(
                                    OptionRequiredError, "%s command requires option: %s%s", cfgCommandName(config->command),
                                    cfgOptionKeyIdxName(optionId, optionIdx), hint);
                            }
                        }
                    }
                }
            }
        }

        // Initialize config
        cfgInit(config);

        // Set option group default index. The first index in the group is automatically set unless the group default option, e.g.
        // pg-default is set. For now the group default options are hard-coded but they could be dynamic. An assert has been added
        // to make sure the code breaks if a new group is added.
        for (unsigned int groupId = 0; groupId < CFG_OPTION_GROUP_TOTAL; groupId++)
        {
            ASSERT(groupId == cfgOptGrpPg || groupId == cfgOptGrpRepo);

            // Get the group default option
            unsigned int defaultOptionId = groupId == cfgOptGrpPg ? cfgOptPgDefault : cfgOptRepoDefault;

            // Does the group default option exist?
            if (cfgOptionTest(defaultOptionId))
            {
                // Search for the key
                unsigned int optionKeyIdx = cfgOptionUInt(defaultOptionId) - 1;
                unsigned int index = 0;

                for (; index < cfgOptionGroupIdxTotal(groupId); index++)
                {
                    if (config->optionGroup[groupId].index[index] == optionKeyIdx)
                        break;
                }

                // Error if the key was not found
                if (index == cfgOptionGroupIdxTotal(groupId))
                {
                    THROW_FMT(
                        OptionInvalidValueError, "key '%u' is not valid for '%s' option", cfgOptionUInt(defaultOptionId),
                        cfgOptionName(defaultOptionId));
                }

                // Set the default
                config->optionGroup[groupId].indexDefault = index;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
