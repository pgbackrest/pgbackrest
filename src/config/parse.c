/***********************************************************************************************************************************
Command and Option Parse
***********************************************************************************************************************************/
#include <getopt.h>
#include <string.h>
#include <strings.h>

#include "common/assert.h"
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
#define PGBACKREST_CONFIG_FILE                                      PGBACKREST_BIN ".conf"
#define PGBACKREST_CONFIG_ORIG_PATH_FILE                            "/etc/" PGBACKREST_CONFIG_FILE

/***********************************************************************************************************************************
Standard config include path name
***********************************************************************************************************************************/
#define PGBACKREST_CONFIG_INCLUDE_PATH                              "conf.d"

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
#include "parse.auto.c"

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
void
convertToByte(String **value, double *valueDbl)
{
    // Make a copy of the value so it is not updated until we know the conversion will succeed
    String *result = strLower(strDup(*value));

    // Match the value against possible values
    if (regExpMatchOne(strNew("^[0-9]+(kb|k|mb|m|gb|g|tb|t|pb|p|b)*$"), result))
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

        // If a letter was found, then truncate, else do nothing since assumed value is already in bytes
        if (chrPos != -1)
        {
            if (strArray[chrPos] != 'b')
            {
                switch (strArray[chrPos])
                {
                    case 'k':
                        multiplier = 1024;
                        break;

                    case 'm':
                        multiplier = 1024 * 1024;
                        break;

                    case 'g':
                        multiplier = 1024 * 1024 * 1024;
                        break;

                    case 't':
                        multiplier = 1024LL * 1024LL * 1024LL * 1024LL;
                        break;

                    case 'p':
                        multiplier = 1024LL * 1024LL * 1024LL * 1024LL * 1024LL;
                        break;

                    default:
                        THROW_FMT(                                  // {uncoverable - regex covers all cases but default required}
                            AssertError, "character %c is not a valid type", strArray[chrPos]);
                }
            }

            // Remove any letters
            strTrunc(result, chrPos);
        }

        // Convert string to bytes
        double newDbl = varDblForce(varNewStr(result)) * multiplier;
        result = varStrForce(varNewDbl(newDbl));

        // If nothing has blown up then safe to overwrite the original values
        *valueDbl = newDbl;
        *value = result;
    }
    else
        THROW_FMT(FormatError, "value '%s' is not valid", strPtr(*value));
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
- If --no-config is specified and --config-path is specified then only *.conf files in the overriden default config-include-path
  (<config-path>/conf.d) will be loaded if exist but not required.
- If --no-config is specified and neither --config-include-path nor --config-path are specified then no configs will be loaded.
- If --config-path only, the defaults for config and config-include-path will be changed to use that as a base path but the files
  will not be required to exist since this is a default override.
***********************************************************************************************************************************/
static String *
cfgFileLoad(                                                        // NOTE: Passing defaults to enable more complete test coverage
    const ParseOption *optionList,                                  // All options and their current settings
    const String *optConfigDefault,                                 // Current default for --config option
    const String *optConfigIncludePathDefault,                      // Current default for --config-include-path option
    const String *origConfigDefault)                                // Original --config option default (/etc/pgbackrest.conf)
{
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
        Buffer *buffer = storageGetNP(storageNewReadP(storageLocal(), configFileName, .ignoreMissing = !configRequired));

        // Convert the contents of the file buffer to the config string object
        if (buffer != NULL)
            result = strNewBuf(buffer);
        else if (strEq(configFileName, optConfigDefaultCurrent))
        {
            // If confg is current default and it was not found, attempt to load the config file from the old default location
            buffer = storageGetNP(storageNewReadP(storageLocal(), origConfigDefault, .ignoreMissing = !configRequired));

            if (buffer != NULL)
                result = strNewBuf(buffer);
        }
    }

    // Load *.conf files from the include directory
    if (loadConfigInclude)
    {
        if (result != NULL)
        {
            // Validate the file by parsing it as an Ini object. If the file is not properly formed, en error will occur.
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
            storageLocal(), configIncludePath, .expression = strNew(".+\\.conf$"), .errorOnMissing = configIncludeRequired);

        // If conf files are found, then add them to the config string
        if (list != NULL && strLstSize(list) > 0)
        {
            // Sort the list for reproducibility only -- order does not matter
            strLstSort(list, sortOrderAsc);

            for (unsigned int listIdx = 0; listIdx < strLstSize(list); listIdx++)
            {
                Buffer *fileBuffer = storageGetNP(
                    storageNewReadP(
                        storageLocal(), strNewFmt("%s/%s", strPtr(configIncludePath), strPtr(strLstGet(list, listIdx))),
                        .ignoreMissing = true));

                if (fileBuffer != NULL) // {uncovered - NULL can only occur if file is missing after file list is retrieved}
                {
                    // Convert the contents of the file buffer to a string object
                    String *configPart = strNewBuf(fileBuffer);

                    // Validate the file by parsing it as an Ini object. If the file is not properly formed, an error will occur.
                    if (strSize(configPart) > 0)
                    {
                        Ini *configPartIni = iniNew();
                        iniParse(configPartIni, configPart);

                        // Create the result config file
                        if (result == NULL)
                            result = strNew("");
                        // Else add an LF in case the previous file did not end with one
                        else
                            strCat(result, "\n");

                        // Add the config part to the result config file
                        strCat(result, strPtr(configPart));
                    }
                }
            }
        }
    }

    return result;
}

/***********************************************************************************************************************************
Parse the command-line arguments and config file to produce final config data

??? Add validation of section names and check all sections for invalid options in the check command.  It's too expensive to add the
logic to this critical path code.
***********************************************************************************************************************************/
void
configParse(unsigned int argListSize, const char *argList[])
{
    // Initialize configuration
    cfgInit();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Set the exe
        cfgExeSet(strNew(argList[0]));

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
        ParseOption parseOptionList[CFG_OPTION_TOTAL];
        memset(&parseOptionList, 0, sizeof(parseOptionList));

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
                        // Try getting the command from the valid command list
                        TRY_BEGIN()
                        {
                            cfgCommandSet(cfgCommandId(argList[optind - 1]));
                        }
                        // Assert error means the command does not exist, which is correct for all usages but this one (since we
                        // don't have any control over what the user passes), so modify the error code and message.
                        CATCH(AssertError)
                        {
                            THROW_FMT(CommandInvalidError, "invalid command '%s'", argList[optind - 1]);
                        }
                        TRY_END();

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

                        strLstAdd(commandParamList, strNew(argList[optind - 1]));
                    }

                    break;
                }

                // If the option is unknown then error
                case '?':
                {
                    THROW_FMT(OptionInvalidError, "invalid option '%s'", argList[optind - 1]);
                    break;
                }

                // If the option is missing an argument then error
                case ':':
                {
                    THROW_FMT(OptionInvalidError, "option '%s' requires argument", argList[optind - 1]);
                    break;
                }

                // Parse valid option
                default:
                {
                    // Get option id and flags from the option code
                    ConfigOption optionId = option & PARSE_OPTION_MASK;
                    bool negate = option & PARSE_NEGATE_FLAG;
                    bool reset = option & PARSE_RESET_FLAG;

                    // Make sure the option id is valid
                    ASSERT_DEBUG(optionId < CFG_OPTION_TOTAL);

                    // If the the option has not been found yet then set it
                    if (!parseOptionList[optionId].found)
                    {
                        parseOptionList[optionId].found = true;
                        parseOptionList[optionId].negate = negate;
                        parseOptionList[optionId].reset = reset;
                        parseOptionList[optionId].source = cfgSourceParam;

                        // Only set the argument if the option requires one
                        if (optionList[optionListIdx].has_arg == required_argument)
                            parseOptionList[optionId].valueList = strLstAdd(strLstNew(), strNew(optarg));
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
                        strLstAdd(parseOptionList[optionId].valueList, strNew(optarg));
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

            // Otherwise set the comand to help
            cfgCommandHelpSet(true);
        }

        // Set command params
        if (commandParamList != NULL)
            cfgCommandParamSet(commandParamList);

        // Enable logging (except for local and remote commands) so config file warnings will be output
        if (cfgCommand() != cfgCmdLocal && cfgCommand() != cfgCmdRemote)
            logInit(logLevelWarn, logLevelWarn, logLevelOff, false);

        // Phase 2: parse config file unless --no-config passed
        // ---------------------------------------------------------------------------------------------------------------------
        if (cfgCommand() != cfgCmdNone &&
            cfgCommand() != cfgCmdVersion &&
            cfgCommand() != cfgCmdHelp)
        {
            // Get the command definition id
            ConfigDefineCommand commandDefId = cfgCommandDefIdFromId(cfgCommand());

            // Load the configuration file(s)
            String *configString = cfgFileLoad(parseOptionList,
                strNew(cfgDefOptionDefault(commandDefId, cfgOptionDefIdFromId(cfgOptConfig))),
                strNew(cfgDefOptionDefault(commandDefId, cfgOptionDefIdFromId(cfgOptConfigIncludePath))),
                strNew(PGBACKREST_CONFIG_ORIG_PATH_FILE));

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
                strLstAdd(sectionList, strNew(CFGDEF_SECTION_GLOBAL));

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
                            LOG_WARN("configuration file contains invalid option '%s'", strPtr(key));
                            continue;
                        }
                        // Warn if negate option found in config
                        else if (optionList[optionIdx].val & PARSE_NEGATE_FLAG)
                        {
                            LOG_WARN("configuration file contains negate option '%s'", strPtr(key));
                            continue;
                        }
                        // Warn if reset option found in config
                        else if (optionList[optionIdx].val & PARSE_RESET_FLAG)
                        {
                            LOG_WARN("configuration file contains reset option '%s'", strPtr(key));
                            continue;
                        }

                        ConfigOption optionId = optionList[optionIdx].val & PARSE_OPTION_MASK;
                        ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

                        /// Warn if this option should be command-line only
                        if (cfgDefOptionSection(optionDefId) == cfgDefSectionCommandLine)
                        {
                            LOG_WARN("configuration file contains command-line only option '%s'", strPtr(key));
                            continue;
                        }

                        // Make sure this option does not appear in the same section with an alternate name
                        Variant *optionFoundKey = varNewInt(optionId);
                        const Variant *optionFoundName = kvGet(optionFound, optionFoundKey);

                        if (optionFoundName != NULL)
                        {
                            THROW_FMT(
                                OptionInvalidError, "configuration file contains duplicate options ('%s', '%s') in section '[%s]'",
                                strPtr(key), strPtr(varStr(optionFoundName)), strPtr(section));
                        }
                        else
                            kvPut(optionFound, optionFoundKey, varNewStr(key));

                        // Continue if the option is not valid for this command
                        if (!cfgDefOptionValid(commandDefId, optionDefId))
                        {
                            // Warn if it is in a command section
                            if (sectionIdx % 2 == 0)
                            {
                                LOG_WARN(
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
                            LOG_WARN(
                                "configuration file contains stanza-only option '%s' in global section '%s'", strPtr(key),
                                strPtr(section));
                            continue;
                        }

                        // Continue if this option has already been found in another section
                        if (parseOptionList[optionId].found)
                            continue;

                        // Get the option value
                        const Variant *value = iniGetDefault(config, section, key, NULL);

                        if (varType(value) == varTypeString && strSize(varStr(value)) == 0)
                        {
                            THROW_FMT(
                                OptionInvalidValueError, "section '%s', key '%s' must have a value", strPtr(section), strPtr(key));
                        }

                        parseOptionList[optionId].found = true;
                        parseOptionList[optionId].source = cfgSourceConfig;

                        // Convert boolean to string
                        if (cfgDefOptionType(optionDefId) == cfgDefOptTypeBoolean)
                        {
                            if (strcasecmp(strPtr(varStr(value)), "n") == 0)
                                parseOptionList[optionId].negate = true;
                            else if (strcasecmp(strPtr(varStr(value)), "y") != 0)
                                THROW_FMT(OptionInvalidError, "boolean option '%s' must be 'y' or 'n'", strPtr(key));
                        }
                        // Else add the string value
                        else if (varType(value) == varTypeString)
                        {
                            parseOptionList[optionId].valueList = strLstNew();
                            strLstAdd(parseOptionList[optionId].valueList, varStr(value));
                        }
                        // Else add the string list
                        else
                            parseOptionList[optionId].valueList = strLstNewVarLst(varVarLst(value));
                    }
                }
            }

            // Phase 3: validate option definitions and load into configuration
            // ---------------------------------------------------------------------------------------------------------------------
            bool allResolved;
            bool optionResolved[CFG_OPTION_TOTAL] = {false};

            do
            {
                // Assume that all dependencies will be resolved in this loop.  This probably won't be true the first few times, but
                // eventually it will be or the do loop would never exit.
                allResolved = true;

                // Loop through all options
                for (ConfigOption optionId = 0; optionId < CFG_OPTION_TOTAL; optionId++)
                {
                    // Get the option data parsed from the command-line
                    ParseOption *parseOption = &parseOptionList[optionId];

                    // Get the option definition id -- will be used to look up option rules
                    ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);
                    ConfigDefineOptionType optionDefType = cfgDefOptionType(optionDefId);

                    // Skip this option if it has already been resolved
                    if (optionResolved[optionId])
                        continue;

                    // Error if the option is not valid for this command
                    if (parseOption->found && !cfgDefOptionValid(commandDefId, optionDefId))
                    {
                        THROW_FMT(
                            OptionInvalidError, "option '%s' not valid for command '%s'", cfgOptionName(optionId),
                            cfgCommandName(cfgCommand()));
                    }

                    // Error if this option is secure and cannot be passed on the command line
                    if (parseOption->found && parseOption->source == cfgSourceParam && cfgDefOptionSecure(optionDefId))
                    {
                        THROW_FMT(
                            OptionInvalidError,
                            "option '%s' is not allowed on the command-line\n"
                            "HINT: this option could expose secrets in the process list.\n"
                            "HINT: specify the option in '%s' instead.",
                            cfgOptionName(optionId), cfgDefOptionDefault(commandDefId, cfgDefOptConfig));
                    }

                    // Error if this option does not allow multiple arguments
                    if (parseOption->valueList != NULL && strLstSize(parseOption->valueList) > 1 &&
                        !(cfgDefOptionType(cfgOptionDefIdFromId(optionId)) == cfgDefOptTypeHash ||
                          cfgDefOptionType(cfgOptionDefIdFromId(optionId)) == cfgDefOptTypeList))
                    {
                        THROW_FMT(OptionInvalidError, "option '%s' cannot have multiple arguments", cfgOptionName(optionId));
                    }

                    // Is the option valid for this command?  If not, mark it as resolved since there is nothing more to do.
                    cfgOptionValidSet(optionId, cfgDefOptionValid(commandDefId, optionDefId));

                    if (!cfgOptionValid(optionId))
                    {
                        optionResolved[optionId] = true;
                        continue;
                    }

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

                        // Make sure the depend option has been resolved, otherwise skip this option for now
                        if (!optionResolved[dependOptionId])
                        {
                            allResolved = false;
                            continue;
                        }

                        // Get the depend option value
                        const Variant *dependValue = cfgOption(dependOptionId);

                        if (dependValue != NULL)
                        {
                            if (dependOptionDefType == cfgDefOptTypeBoolean)
                            {
                                if (cfgOptionBool(dependOptionId))
                                    dependValue = varNewStrZ("1");
                                else
                                    dependValue = varNewStrZ("0");
                            }
                        }

                        // Can't resolve if the depend option value is null
                        if (dependValue == NULL)
                        {
                            dependResolved = false;

                            // if (optionSet)
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
                            // spool-path is only loaded for the archive-push command when archive-async=y, and the presense of
                            // spool-path in the config file should not cause an error here, it will just end up null.
                            if (!dependResolved && optionSet && parseOption->source == cfgSourceParam)
                            {
                                // Get the depend option name
                                String *dependOptionName = strNew(cfgOptionName(dependOptionId));

                                // Build the list of possible depend values
                                StringList *dependValueList = strLstNew();

                                for (int listIdx = 0; listIdx < cfgDefOptionDependValueTotal(commandDefId, optionDefId); listIdx++)
                                {
                                    const char *dependValue = cfgDefOptionDependValue(commandDefId, optionDefId, listIdx);

                                    // Build list based on depend option type
                                    switch (dependOptionDefType)
                                    {
                                        // Boolean outputs depend option name as no-* when false
                                        case cfgDefOptTypeBoolean:
                                        {
                                            if (strcmp(dependValue, "0") == 0)
                                                dependOptionName = strNewFmt("no-%s", cfgOptionName(dependOptionId));

                                            break;
                                        }

                                        // String is output with quotes
                                        case cfgDefOptTypeString:
                                        {
                                            strLstAdd(dependValueList, strNewFmt("'%s'", dependValue));
                                            break;
                                        }

                                        // Other types are output plain
                                        case cfgDefOptTypeFloat:
                                        case cfgDefOptTypeHash:
                                        case cfgDefOptTypeInteger:
                                        case cfgDefOptTypeList:
                                        case cfgDefOptTypeSize:
                                        {
                                            strLstAddZ(dependValueList, dependValue);   // {uncovered - no depends of other types}
                                            break;                                      // {+uncovered}
                                        }
                                    }
                                }

                                // Build the error string
                                String *errorValue = strNew("");

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

                    // Is the option defined?
                    if (optionSet && dependResolved)
                    {
                        if (optionDefType == cfgDefOptTypeBoolean)
                        {
                            cfgOptionSet(optionId, parseOption->source, varNewBool(!parseOption->negate));
                        }
                        else if (optionDefType == cfgDefOptTypeHash)
                        {
                            Variant *value = varNewKv();
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

                                kvPut(keyValue, varNewStr(strNewN(pair, (size_t)(equal - pair))), varNewStr(strNew(equal + 1)));
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
                            if (optionDefType == cfgDefOptTypeInteger || optionDefType == cfgDefOptTypeFloat || optionDefType == cfgDefOptTypeSize)
                            {
                                double valueDbl = 0;

                                // Check that the value can be converted
                                TRY_BEGIN()
                                {

                                    if (optionDefType == cfgDefOptTypeInteger)
                                    {
                                        valueDbl = (double)varInt64Force(varNewStr(value));
                                    }
                                    else if (optionDefType == cfgDefOptTypeSize)
                                    {
                                        convertToByte(&value, &valueDbl);
                                    }
                                    else
                                        valueDbl = varDblForce(varNewStr(value));
                                }
                                CATCH(AssertError)
                                {
                                    RETHROW();                      // {uncovered - asserts can't currently happen here this is JIC}
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

                            // If the option has an allow list then check it
                            if (cfgDefOptionAllowList(commandDefId, optionDefId) &&
                                !cfgDefOptionAllowListValueValid(commandDefId, optionDefId, strPtr(value)))
                            {
                                THROW_FMT(
                                    OptionInvalidValueError, "'%s' is not allowed for '%s' option", strPtr(value),
                                    cfgOptionName(optionId));
                            }

                            cfgOptionSet(optionId, parseOption->source, varNewStr(value));
                        }
                    }
                    else if (dependResolved && parseOption->negate)
                        cfgOptionSet(optionId, parseOption->source, NULL);
                    // Else try to set a default
                    else if (dependResolved)
                    {
                        // Get the default value for this option
                        const char *value = cfgDefOptionDefault(commandDefId, optionDefId);

                        if (value != NULL)
                            cfgOptionSet(optionId, cfgSourceDefault, varNewStrZ(value));
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

                    // Option is now resolved
                    optionResolved[optionId] = true;
                }
            }
            while (!allResolved);
        }
    }
    MEM_CONTEXT_TEMP_END();
}
