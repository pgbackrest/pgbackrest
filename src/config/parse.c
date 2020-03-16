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

// Mask to exclude all flags and get at the actual option id (only 12 bits allowed for option id, the rest reserved for flags)
#define PARSE_OPTION_MASK                                           0xFFF

/***********************************************************************************************************************************
Include automatically generated data structure for getopt_long()
***********************************************************************************************************************************/
#include "config/parse.auto.c"

/***********************************************************************************************************************************
Struct to hold options parsed from the command line
***********************************************************************************************************************************/
typedef struct ParseOption
{
    bool found:1;                                                   // Was the option found on the command line?
    bool negate:1;                                                  // Was the option negated on the command line?
    bool reset:1;                                                   // Was the option reset on the command line?
    unsigned int source:2;                                          // Where was to option found?
    StringList *valueList;                                          // List of values found
} ParseOption;

#define FUNCTION_LOG_PARSE_OPTION_FORMAT(value, buffer, bufferSize)                                                                \
    typeToLog("ParseOption", buffer, bufferSize)

/***********************************************************************************************************************************
Find an option by name in the option list
***********************************************************************************************************************************/
static unsigned int
optionFind(const String *option)
{
    unsigned int optionIdx = 0;

    while (optionList[optionIdx].name != NULL)
    {
        if (strcmp(strPtr(option), optionList[optionIdx].name) == 0)
            break;

        optionIdx++;
    }

    return optionIdx;
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
        const char *strArray = strPtr(result);
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
        THROW_FMT(FormatError, "value '%s' is not valid", strPtr(*value));

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
            strCat(*config, "\n");
            strCat(*config, strPtr(configPartStr));
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
    bool configRequired = optionList[cfgOptConfig].found;
    bool configIncludeRequired = optionList[cfgOptConfigIncludePath].found;

    // Save default for later determining if must check old original default config path
    const String *optConfigDefaultCurrent = optConfigDefault;

    // If the config-path option is found on the command line, then its value will override the base path defaults for config and
    // config-include-path
    if (optionList[cfgOptConfigPath].found)
    {
        optConfigDefault =
            strNewFmt("%s/%s", strPtr(strLstGet(optionList[cfgOptConfigPath].valueList, 0)), strPtr(strBase(optConfigDefault)));
        optConfigIncludePathDefault =
            strNewFmt("%s/%s", strPtr(strLstGet(optionList[cfgOptConfigPath].valueList, 0)), PGBACKREST_CONFIG_INCLUDE_PATH);
    }

    // If the --no-config option was passed then do not load the config file
    if (optionList[cfgOptConfig].negate)
    {
        loadConfig = false;
        configRequired = false;
    }

    // If --config option is specified on the command line but neither the --config-include-path nor the config-path are passed,
    // then do not attempt to load the include files
    if (optionList[cfgOptConfig].found && !(optionList[cfgOptConfigIncludePath].found || optionList[cfgOptConfigPath].found))
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
        if (optionList[cfgOptConfig].found)
            configFileName = strLstGet(optionList[cfgOptConfig].valueList, 0);
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
        if (optionList[cfgOptConfigIncludePath].found)
            configIncludePath = strLstGet(optionList[cfgOptConfigIncludePath].valueList, 0);
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
                            storageLocal(), strNewFmt("%s/%s", strPtr(configIncludePath), strPtr(strLstGet(list, listIdx))),
                            .ignoreMissing = true)));
            }
        }
    }

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Parse the command-line arguments and config file to produce final config data

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

    // Initialize configuration
    cfgInit();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Set the exe
        cfgExeSet(STR(argList[0]));

        // Phase 1: parse command line parameters
        // -------------------------------------------------------------------------------------------------------------------------
        int option;                                                     // Code returned by getopt_long
        int optionListIdx;                                              // Index of option is list (if an option was returned)
        bool argFound = false;                                          // Track args found to decide on error or help at the end
        StringList *commandParamList = NULL;                            // List of command  parameters

        // Reset optind to 1 in case getopt_long has been called before
        optind = 1;

        // Don't error automatically on unknown options - they will be processed in the loop below
        opterr = false;

        // List of parsed options
        ParseOption parseOptionList[CFG_OPTION_TOTAL] = {{.found = false}};

        // Only the first non-option parameter should be treated as a command so track if the command has been set
        bool commandSet = false;

        while ((option = getopt_long((int)argListSize, (char **)argList, "-:", optionList, &optionListIdx)) != -1)
        {
            switch (option)
            {
                // Parse arguments that are not options, i.e. commands and parameters passed to commands
                case 1:
                {
                    // The first argument should be the command
                    if (!commandSet)
                    {
                        const char *command = argList[optind - 1];

                        // Try getting the command from the valid command list
                        ConfigCommand commandId = cfgCommandId(command, false);
                        ConfigCommandRole commandRoleId = cfgCmdRoleDefault;

                        // If not successful then a command role may be appended
                        if (commandId == cfgCmdNone)
                        {
                            const StringList *commandPart = strLstNewSplit(STR(command), COLON_STR);

                            if (strLstSize(commandPart) == 2)
                            {
                                // Get command id
                                commandId = cfgCommandId(strPtr(strLstGet(commandPart, 0)), false);

                                // If command id is valid then get command role id
                                if (commandId != cfgCmdNone)
                                    commandRoleId = cfgCommandRoleEnum(strLstGet(commandPart, 1));
                            }
                        }

                        // Error when command does not exist
                        if (commandId == cfgCmdNone)
                            THROW_FMT(CommandInvalidError, "invalid command '%s'", command);

                        //  Set the command
                        cfgCommandSet(commandId, commandRoleId);

                        if (cfgCommand() == cfgCmdHelp)
                            cfgCommandHelpSet(true);
                        else
                            commandSet = true;
                    }
                    // Additional arguments are command arguments
                    else
                    {
                        if (commandParamList == NULL)
                            commandParamList = strLstNew();

                        strLstAdd(commandParamList, STR(argList[optind - 1]));
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
                    ConfigOption optionId = option & PARSE_OPTION_MASK;
                    bool negate = option & PARSE_NEGATE_FLAG;
                    bool reset = option & PARSE_RESET_FLAG;

                    // Make sure the option id is valid
                    ASSERT(optionId < CFG_OPTION_TOTAL);

                    // Error if this option is secure and cannot be passed on the command line
                    if (cfgDefOptionSecure(cfgOptionDefIdFromId(optionId)))
                    {
                        THROW_FMT(
                            OptionInvalidError,
                            "option '%s' is not allowed on the command-line\n"
                            "HINT: this option could expose secrets in the process list.\n"
                            "HINT: specify the option in a configuration file or an environment variable instead.",
                            cfgOptionName(optionId));
                    }

                    // If the the option has not been found yet then set it
                    if (!parseOptionList[optionId].found)
                    {
                        parseOptionList[optionId].found = true;
                        parseOptionList[optionId].negate = negate;
                        parseOptionList[optionId].reset = reset;
                        parseOptionList[optionId].source = cfgSourceParam;

                        // Only set the argument if the option requires one
                        if (optionList[optionListIdx].has_arg == required_argument)
                        {
                            parseOptionList[optionId].valueList = strLstNew();
                            strLstAdd(parseOptionList[optionId].valueList, STR(optarg));
                        }
                    }
                    else
                    {
                        // Make sure option is not negated more than once.  It probably wouldn't hurt anything to accept this case
                        // but there's no point in allowing the user to be sloppy.
                        if (parseOptionList[optionId].negate && negate)
                            THROW_FMT(OptionInvalidError, "option '%s' is negated multiple times", cfgOptionName(optionId));

                        // Make sure option is not reset more than once.  Same justification as negate.
                        if (parseOptionList[optionId].reset && reset)
                            THROW_FMT(OptionInvalidError, "option '%s' is reset multiple times", cfgOptionName(optionId));

                        // Don't allow an option to be both negated and reset
                        if ((parseOptionList[optionId].reset && negate) || (parseOptionList[optionId].negate && reset))
                            THROW_FMT(OptionInvalidError, "option '%s' cannot be negated and reset", cfgOptionName(optionId));

                        // Don't allow an option to be both set and negated
                        if (parseOptionList[optionId].negate != negate)
                            THROW_FMT(OptionInvalidError, "option '%s' cannot be set and negated", cfgOptionName(optionId));

                        // Don't allow an option to be both set and reset
                        if (parseOptionList[optionId].reset != reset)
                            THROW_FMT(OptionInvalidError, "option '%s' cannot be set and reset", cfgOptionName(optionId));

                        // Add the argument
                        if (optionList[optionListIdx].has_arg == required_argument &&
                            cfgDefOptionMulti(cfgOptionDefIdFromId(optionId)))
                        {
                            strLstAdd(parseOptionList[optionId].valueList, strNew(optarg));
                        }
                        // Error if the option does not accept multiple arguments
                        else
                            THROW_FMT(OptionInvalidError, "option '%s' cannot be set multiple times", cfgOptionName(optionId));
                    }

                    break;
                }
            }

            // Arg has been found
            argFound = true;
        }

        // Handle command not found
        if (!commandSet && !cfgCommandHelp())
        {
            // If there are args then error
            if (argFound)
                THROW_FMT(CommandRequiredError, "no command found");

            // Otherwise set the command to help
            cfgCommandHelpSet(true);
        }

        // Set command params
        if (commandParamList != NULL)
        {
            if (!cfgCommandHelp() && !cfgParameterAllowed())
                THROW(ParamInvalidError, "command does not allow parameters");

            cfgCommandParamSet(commandParamList);
        }

        // Enable logging (except for local and remote commands) so config file warnings will be output
        if (cfgCommandRole() != cfgCmdRoleLocal && cfgCommandRole() != cfgCmdRoleRemote && resetLogLevel)
            logInit(logLevelWarn, logLevelWarn, logLevelOff, false, 1, false);

        // Only continue if command options need to be validated, i.e. a real command is running or we are getting help for a
        // specific command and would like to display actual option values in the help.
        if (cfgCommand() != cfgCmdNone &&
            cfgCommand() != cfgCmdVersion &&
            cfgCommand() != cfgCmdHelp)
        {
            // Phase 2: parse environment variables
            // ---------------------------------------------------------------------------------------------------------------------
            ConfigDefineCommand commandDefId = cfgCommandDefIdFromId(cfgCommand());

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
                    unsigned int optionIdx = optionFind(key);

                    // Warn if the option not found
                    if (optionList[optionIdx].name == NULL)
                    {
                        LOG_WARN_FMT("environment contains invalid option '%s'", strPtr(key));
                        continue;
                    }
                    // Warn if negate option found in env
                    else if (optionList[optionIdx].val & PARSE_NEGATE_FLAG)
                    {
                        LOG_WARN_FMT("environment contains invalid negate option '%s'", strPtr(key));
                        continue;
                    }
                    // Warn if reset option found in env
                    else if (optionList[optionIdx].val & PARSE_RESET_FLAG)
                    {
                        LOG_WARN_FMT("environment contains invalid reset option '%s'", strPtr(key));
                        continue;
                    }

                    ConfigOption optionId = optionList[optionIdx].val & PARSE_OPTION_MASK;
                    ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

                    // Continue if the option is not valid for this command
                    if (!cfgDefOptionValid(commandDefId, optionDefId))
                        continue;

                    if (strSize(value) == 0)
                        THROW_FMT(OptionInvalidValueError, "environment variable '%s' must have a value", strPtr(key));

                    // Continue if the option has already been specified on the command line
                    if (parseOptionList[optionId].found)
                        continue;

                    parseOptionList[optionId].found = true;
                    parseOptionList[optionId].source = cfgSourceConfig;

                    // Convert boolean to string
                    if (cfgDefOptionType(optionDefId) == cfgDefOptTypeBoolean)
                    {
                        if (strEqZ(value, "n"))
                            parseOptionList[optionId].negate = true;
                        else if (!strEqZ(value, "y"))
                            THROW_FMT(OptionInvalidValueError, "environment boolean option '%s' must be 'y' or 'n'", strPtr(key));
                    }
                    // Else split list/hash into separate values
                    else if (cfgDefOptionMulti(optionDefId))
                    {
                        parseOptionList[optionId].valueList = strLstNewSplitZ(value, ":");
                    }
                    // Else add the string value
                    else
                    {
                        parseOptionList[optionId].valueList = strLstNew();
                        strLstAdd(parseOptionList[optionId].valueList, value);
                    }
                }
            }

            // Phase 3: parse config file unless --no-config passed
            // ---------------------------------------------------------------------------------------------------------------------
            // Load the configuration file(s)
            String *configString = cfgFileLoad(parseOptionList,
                STR(cfgDefOptionDefault(commandDefId, cfgOptionDefIdFromId(cfgOptConfig))),
                STR(cfgDefOptionDefault(commandDefId, cfgOptionDefIdFromId(cfgOptConfigIncludePath))),
                PGBACKREST_CONFIG_ORIG_PATH_FILE_STR);

            if (configString != NULL)
            {
                Ini *config = iniNew();
                iniParse(config, configString);
                // Get the stanza name
                String *stanza = NULL;

                if (parseOptionList[cfgOptStanza].found)
                    stanza = strLstGet(parseOptionList[cfgOptStanza].valueList, 0);

                // Build list of sections to search for options
                StringList *sectionList = strLstNew();

                if (stanza != NULL)
                {
                    strLstAdd(sectionList, strNewFmt("%s:%s", strPtr(stanza), cfgCommandName(cfgCommand())));
                    strLstAdd(sectionList, stanza);
                }

                strLstAdd(sectionList, strNewFmt(CFGDEF_SECTION_GLOBAL ":%s", cfgCommandName(cfgCommand())));
                strLstAdd(sectionList, CFGDEF_SECTION_GLOBAL_STR);

                // Loop through sections to search for options
                for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
                {
                    String *section = strLstGet(sectionList, sectionIdx);
                    StringList *keyList = iniSectionKeyList(config, section);
                    KeyValue *optionFound = kvNew();

                    // Loop through keys to search for options
                    for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
                    {
                        String *key = strLstGet(keyList, keyIdx);

                        // Find the optionName in the main list
                        unsigned int optionIdx = optionFind(key);

                        // Warn if the option not found
                        if (optionList[optionIdx].name == NULL)
                        {
                            LOG_WARN_FMT("configuration file contains invalid option '%s'", strPtr(key));
                            continue;
                        }
                        // Warn if negate option found in config
                        else if (optionList[optionIdx].val & PARSE_NEGATE_FLAG)
                        {
                            LOG_WARN_FMT("configuration file contains negate option '%s'", strPtr(key));
                            continue;
                        }
                        // Warn if reset option found in config
                        else if (optionList[optionIdx].val & PARSE_RESET_FLAG)
                        {
                            LOG_WARN_FMT("configuration file contains reset option '%s'", strPtr(key));
                            continue;
                        }

                        ConfigOption optionId = optionList[optionIdx].val & PARSE_OPTION_MASK;
                        ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

                        /// Warn if this option should be command-line only
                        if (cfgDefOptionSection(optionDefId) == cfgDefSectionCommandLine)
                        {
                            LOG_WARN_FMT("configuration file contains command-line only option '%s'", strPtr(key));
                            continue;
                        }

                        // Make sure this option does not appear in the same section with an alternate name
                        const Variant *optionFoundKey = VARINT(optionId);
                        const Variant *optionFoundName = kvGet(optionFound, optionFoundKey);

                        if (optionFoundName != NULL)
                        {
                            THROW_FMT(
                                OptionInvalidError, "configuration file contains duplicate options ('%s', '%s') in section '[%s]'",
                                strPtr(key), strPtr(varStr(optionFoundName)), strPtr(section));
                        }
                        else
                            kvPut(optionFound, optionFoundKey, VARSTR(key));

                        // Continue if the option is not valid for this command
                        if (!cfgDefOptionValid(commandDefId, optionDefId))
                        {
                            // Warn if it is in a command section
                            if (sectionIdx % 2 == 0)
                            {
                                LOG_WARN_FMT(
                                    "configuration file contains option '%s' invalid for section '%s'", strPtr(key),
                                    strPtr(section));
                                continue;
                            }

                            continue;
                        }

                        // Continue if stanza option is in a global section
                        if (cfgDefOptionSection(optionDefId) == cfgDefSectionStanza &&
                            strBeginsWithZ(section, CFGDEF_SECTION_GLOBAL))
                        {
                            LOG_WARN_FMT(
                                "configuration file contains stanza-only option '%s' in global section '%s'", strPtr(key),
                                strPtr(section));
                            continue;
                        }

                        // Continue if this option has already been found in another section or command-line/environment
                        if (parseOptionList[optionId].found)
                            continue;

                        parseOptionList[optionId].found = true;
                        parseOptionList[optionId].source = cfgSourceConfig;

                        // Process list
                        if (iniSectionKeyIsList(config, section, key))
                        {
                            // Error if the option cannot be specified multiple times
                            if (!cfgDefOptionMulti(optionDefId))
                                THROW_FMT(OptionInvalidError, "option '%s' cannot be set multiple times", cfgOptionName(optionId));

                            parseOptionList[optionId].valueList = iniGetList(config, section, key);
                        }
                        else
                        {
                            // Get the option value
                            const String *value = iniGet(config, section, key);

                            if (strSize(value) == 0)
                            {
                                THROW_FMT(
                                    OptionInvalidValueError, "section '%s', key '%s' must have a value", strPtr(section),
                                    strPtr(key));
                            }

                            if (cfgDefOptionType(optionDefId) == cfgDefOptTypeBoolean)
                            {
                                if (strEqZ(value, "n"))
                                    parseOptionList[optionId].negate = true;
                                else if (!strEqZ(value, "y"))
                                    THROW_FMT(OptionInvalidValueError, "boolean option '%s' must be 'y' or 'n'", strPtr(key));
                            }
                            // Else add the string value
                            else
                            {
                                parseOptionList[optionId].valueList = strLstNew();
                                strLstAdd(parseOptionList[optionId].valueList, value);
                            }
                        }
                    }
                }
            }

            // Phase 4: validate option definitions and load into configuration
            // ---------------------------------------------------------------------------------------------------------------------
            for (unsigned int optionOrderIdx = 0; optionOrderIdx < CFG_OPTION_TOTAL; optionOrderIdx++)
            {
                // Validate options based on the option resolve order.  This allows resolving all options in a single pass.
                ConfigOption optionId = optionResolveOrder[optionOrderIdx];

                // Get the option data parsed from the command-line
                ParseOption *parseOption = &parseOptionList[optionId];

                // Get the option definition id -- will be used to look up option rules
                ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);
                ConfigDefineOptionType optionDefType = cfgDefOptionType(optionDefId);

                // Error if the option is not valid for this command
                if (parseOption->found && !cfgDefOptionValid(commandDefId, optionDefId))
                {
                    THROW_FMT(
                        OptionInvalidError, "option '%s' not valid for command '%s'", cfgOptionName(optionId),
                        cfgCommandName(cfgCommand()));
                }

                // Is the option valid for this command?  If not, there is nothing more to do.
                cfgOptionValidSet(optionId, cfgDefOptionValid(commandDefId, optionDefId));

                if (!cfgOptionValid(optionId))
                    continue;

                // Is the value set for this option?
                bool optionSet =
                    parseOption->found && (optionDefType == cfgDefOptTypeBoolean || !parseOption->negate) &&
                    !parseOption->reset;

                // Set negate flag
                cfgOptionNegateSet(optionId, parseOption->negate);

                // Set reset flag
                cfgOptionResetSet(optionId, parseOption->reset);

                // Check option dependencies
                bool dependResolved = true;

                if (cfgDefOptionDepend(commandDefId, optionDefId))
                {
                    ConfigOption dependOptionId =
                        cfgOptionIdFromDefId(cfgDefOptionDependOption(commandDefId, optionDefId), cfgOptionIndex(optionId));
                    ConfigDefineOption dependOptionDefId = cfgOptionDefIdFromId(dependOptionId);
                    ConfigDefineOptionType dependOptionDefType = cfgDefOptionType(dependOptionDefId);

                    // Get the depend option value
                    const Variant *dependValue = cfgOption(dependOptionId);

                    if (dependValue != NULL)
                    {
                        if (dependOptionDefType == cfgDefOptTypeBoolean)
                        {
                            if (cfgOptionBool(dependOptionId))
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
                        if (optionSet && parseOption->source == cfgSourceParam)
                        {
                            THROW_FMT(
                                OptionInvalidError, "option '%s' not valid without option '%s'", cfgOptionName(optionId),
                                cfgOptionName(dependOptionId));
                        }
                    }
                    // If a depend list exists, make sure the value is in the list
                    else if (cfgDefOptionDependValueTotal(commandDefId, optionDefId) > 0)
                    {
                        dependResolved = cfgDefOptionDependValueValid(commandDefId, optionDefId, strPtr(varStr(dependValue)));

                        // If depend not resolved and option value is set on the command-line then error.  It's OK to have
                        // unresolved options in the config file because they may be there for another command.  For instance,
                        // spool-path is only loaded for the archive-push command when archive-async=y, and the presence of
                        // spool-path in the config file should not cause an error here, it will just end up null.
                        if (!dependResolved && optionSet && parseOption->source == cfgSourceParam)
                        {
                            // Get the depend option name
                            const String *dependOptionName = STR(cfgOptionName(dependOptionId));

                            // Build the list of possible depend values
                            StringList *dependValueList = strLstNew();

                            for (unsigned int listIdx = 0;
                                    listIdx < cfgDefOptionDependValueTotal(commandDefId, optionDefId); listIdx++)
                            {
                                const char *dependValue = cfgDefOptionDependValue(commandDefId, optionDefId, listIdx);

                                // Build list based on depend option type
                                if (dependOptionDefType == cfgDefOptTypeBoolean)
                                {
                                    // Boolean outputs depend option name as no-* when false
                                    if (strcmp(dependValue, "0") == 0)
                                        dependOptionName = strNewFmt("no-%s", cfgOptionName(dependOptionId));
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
                                errorValue = strNewFmt(" = %s", strPtr(strLstGet(dependValueList, 0)));
                            else if (strLstSize(dependValueList) > 1)
                                errorValue = strNewFmt(" in (%s)", strPtr(strLstJoin(dependValueList, ", ")));

                            // Throw the error
                            THROW(
                                OptionInvalidError,
                                strPtr(
                                    strNewFmt(
                                        "option '%s' not valid without option '%s'%s", cfgOptionName(optionId),
                                        strPtr(dependOptionName), strPtr(errorValue))));
                        }
                    }
                }

                // Is the option resolved?
                if (dependResolved)
                {
                    // Is the option set?
                    if (optionSet)
                    {
                        if (optionDefType == cfgDefOptTypeBoolean)
                        {
                            cfgOptionSet(optionId, parseOption->source, VARBOOL(!parseOption->negate));
                        }
                        else if (optionDefType == cfgDefOptTypeHash)
                        {
                            Variant *value = varNewKv(kvNew());
                            KeyValue *keyValue = varKv(value);

                            for (unsigned int listIdx = 0; listIdx < strLstSize(parseOption->valueList); listIdx++)
                            {
                                const char *pair = strPtr(strLstGet(parseOption->valueList, listIdx));
                                const char *equal = strchr(pair, '=');

                                if (equal == NULL)
                                {
                                    THROW_FMT(
                                        OptionInvalidError, "key/value '%s' not valid for '%s' option",
                                        strPtr(strLstGet(parseOption->valueList, listIdx)), cfgOptionName(optionId));
                                }

                                kvPut(keyValue, VARSTR(strNewN(pair, (size_t)(equal - pair))), VARSTRZ(equal + 1));
                            }

                            cfgOptionSet(optionId, parseOption->source, value);
                        }
                        else if (optionDefType == cfgDefOptTypeList)
                        {
                            cfgOptionSet(optionId, parseOption->source, varNewVarLst(varLstNewStrLst(parseOption->valueList)));
                        }
                        else
                        {
                            String *value = strLstGet(parseOption->valueList, 0);

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
                                        valueDbl = (double)varInt64Force(VARSTR(value));
                                    }
                                    else if (optionDefType == cfgDefOptTypeSize)
                                    {
                                        convertToByte(&value, &valueDbl);
                                    }
                                    else
                                        valueDbl = varDblForce(VARSTR(value));
                                }
                                CATCH_ANY()
                                {
                                    THROW_FMT(
                                        OptionInvalidValueError, "'%s' is not valid for '%s' option", strPtr(value),
                                        cfgOptionName(optionId));
                                }
                                TRY_END();

                                // Check value range
                                if (cfgDefOptionAllowRange(commandDefId, optionDefId) &&
                                    (valueDbl < cfgDefOptionAllowRangeMin(commandDefId, optionDefId) ||
                                     valueDbl > cfgDefOptionAllowRangeMax(commandDefId, optionDefId)))
                                {
                                    THROW_FMT(
                                        OptionInvalidValueError, "'%s' is out of range for '%s' option", strPtr(value),
                                        cfgOptionName(optionId));
                                }
                            }
                            // Else if path make sure it is valid
                            else if (optionDefType == cfgDefOptTypePath)
                            {
                                // Make sure it is long enough to be a path
                                if (strSize(value) == 0)
                                {
                                    THROW_FMT(
                                        OptionInvalidValueError, "'%s' must be >= 1 character for '%s' option", strPtr(value),
                                        cfgOptionName(optionId));
                                }

                                // Make sure it starts with /
                                if (!strBeginsWithZ(value, "/"))
                                {
                                    THROW_FMT(
                                        OptionInvalidValueError, "'%s' must begin with / for '%s' option", strPtr(value),
                                        cfgOptionName(optionId));
                                }

                                // Make sure there are no occurrences of //
                                if (strstr(strPtr(value), "//") != NULL)
                                {
                                    THROW_FMT(
                                        OptionInvalidValueError, "'%s' cannot contain // for '%s' option", strPtr(value),
                                        cfgOptionName(optionId));
                                }

                                // If the path ends with a / we'll strip it off (unless the value is just /)
                                if (strEndsWithZ(value, "/") && strSize(value) != 1)
                                    strTrunc(value, (int)strSize(value) - 1);
                            }

                            // If the option has an allow list then check it
                            if (cfgDefOptionAllowList(commandDefId, optionDefId) &&
                                !cfgDefOptionAllowListValueValid(commandDefId, optionDefId, strPtr(value)))
                            {
                                THROW_FMT(
                                    OptionInvalidValueError, "'%s' is not allowed for '%s' option", strPtr(value),
                                    cfgOptionName(optionId));
                            }

                            cfgOptionSet(optionId, parseOption->source, VARSTR(value));
                        }
                    }
                    else if (parseOption->negate)
                        cfgOptionSet(optionId, parseOption->source, NULL);
                    // Else try to set a default
                    else
                    {
                        // Get the default value for this option
                        const char *value = cfgDefOptionDefault(commandDefId, optionDefId);

                        if (value != NULL)
                            cfgOptionSet(optionId, cfgSourceDefault, VARSTRZ(value));
                        else if (cfgOptionIndex(optionId) == 0 && cfgDefOptionRequired(commandDefId, optionDefId) &&
                                 !cfgCommandHelp())
                        {
                            const char *hint = "";

                            if (cfgDefOptionSection(optionDefId) == cfgDefSectionStanza)
                                hint = "\nHINT: does this stanza exist?";

                            THROW_FMT(
                                OptionRequiredError, "%s command requires option: %s%s", cfgCommandName(cfgCommand()),
                                cfgOptionName(optionId), hint);
                        }
                    }
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
