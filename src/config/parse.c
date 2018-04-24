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
    String *result = strLower(strDup(*value));
    if (regExpMatchOne(strNew("^[0-9]+(kb|k|mb|m|gb|g|tb|t|pb|p|b)*$"), result))
    {
        // Get the character array and size
        const char *strArray = strPtr(result);
        size_t size = strSize(result);
        int chrPos = -1;

        // If there is a b on the end, then see if the previous character is a number
        if (strArray[size - 1] == 'b')
        {
            // If the previous character is a number, then the letter to look at is b which is the last position else it is in the
            // next to last position (e.g. kb - so the k is the position of interest).  Only need to test for <= 9 since the regex
            // enforces the format.
            if (strArray[size - 2] <= '9')
                chrPos = (int)(size - 1);
            else
                chrPos = (int)(size - 2);
        }
        // else if there is no b at the end but the last position is not a number then it must be one of the letters, e.g. k
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
                        multiplier = 1024L * 1024L * 1024L * 1024L;
                        break;

                    case 'p':
                        multiplier = 1024L * 1024L * 1024L * 1024L * 1024L;
                        break;

                    default:
                        THROW(                                      // {uncoverable - regex covers all cases but default required}
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
        THROW(FormatError, "value '%s' is not valid", strPtr(*value));
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
                            THROW(CommandInvalidError, "invalid command '%s'", argList[optind - 1]);
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
                    THROW(OptionInvalidError, "invalid option '%s'", argList[optind - 1]);
                    break;
                }

                // If the option is missing an argument then error
                case ':':
                {
                    THROW(OptionInvalidError, "option '%s' requires argument", argList[optind - 1]);
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
                            THROW(OptionInvalidError, "option '%s' is negated multiple times", cfgOptionName(optionId));

                        // Make sure option is not reset more than once.  Same justification as negate.
                        if (parseOptionList[optionId].reset && reset)
                            THROW(OptionInvalidError, "option '%s' is reset multiple times", cfgOptionName(optionId));

                        // Don't allow an option to be both negated and reset
                        if ((parseOptionList[optionId].reset && negate) || (parseOptionList[optionId].negate && reset))
                            THROW(OptionInvalidError, "option '%s' cannot be negated and reset", cfgOptionName(optionId));

                        // Don't allow an option to be both set and negated
                        if (parseOptionList[optionId].negate != negate)
                            THROW(OptionInvalidError, "option '%s' cannot be set and negated", cfgOptionName(optionId));

                        // Don't allow an option to be both set and reset
                        if (parseOptionList[optionId].reset != reset)
                            THROW(OptionInvalidError, "option '%s' cannot be set and reset", cfgOptionName(optionId));

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
                THROW(CommandRequiredError, "no command found");

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

            if (!parseOptionList[cfgOptConfig].negate)
            {
                // Get the config file name from the command-line if it exists else default
                const String *configFile = NULL;

                if (parseOptionList[cfgOptConfig].found)
                    configFile = strLstGet(parseOptionList[cfgOptConfig].valueList, 0);
                else
                    configFile = strNew(cfgDefOptionDefault(commandDefId, cfgOptionDefIdFromId(cfgOptConfig)));

                // Load the ini file
                Buffer *buffer = storageGetNP(
                    storageOpenReadP(storageLocal(), configFile, .ignoreMissing = !parseOptionList[cfgOptConfig].found));

                // Load the config file if it was found
                if (buffer != NULL)
                {
                    // Parse the ini file
                    Ini *config = iniNew();
                    iniParse(config, strNewBuf(buffer));

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
                                LOG_WARN("'%s' contains invalid option '%s'", strPtr(configFile), strPtr(key));
                                continue;
                            }
                            // Warn if negate option found in config
                            else if (optionList[optionIdx].val & PARSE_NEGATE_FLAG)
                            {
                                LOG_WARN("'%s' contains negate option '%s'", strPtr(configFile), strPtr(key));
                                continue;
                            }
                            // Warn if reset option found in config
                            else if (optionList[optionIdx].val & PARSE_RESET_FLAG)
                            {
                                LOG_WARN("'%s' contains reset option '%s'", strPtr(configFile), strPtr(key));
                                continue;
                            }

                            ConfigOption optionId = optionList[optionIdx].val & PARSE_OPTION_MASK;
                            ConfigDefineOption optionDefId = cfgOptionDefIdFromId(optionId);

                            /// Warn if this option should be command-line only
                            if (cfgDefOptionSection(optionDefId) == cfgDefSectionCommandLine)
                            {
                                LOG_WARN("'%s' contains command-line only option '%s'", strPtr(configFile), strPtr(key));
                                continue;
                            }

                            // Make sure this option does not appear in the same section with an alternate name
                            Variant *optionFoundKey = varNewInt(optionId);
                            const Variant *optionFoundName = kvGet(optionFound, optionFoundKey);

                            if (optionFoundName != NULL)
                            {
                                THROW(
                                    OptionInvalidError, "'%s' contains duplicate options ('%s', '%s') in section '[%s]'",
                                    strPtr(configFile), strPtr(key), strPtr(varStr(optionFoundName)), strPtr(section));
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
                                        "'%s' contains option '%s' invalid for section '%s'", strPtr(configFile), strPtr(key),
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
                                    "'%s' contains stanza-only option '%s' in global section '%s'", strPtr(configFile), strPtr(key),
                                    strPtr(section));
                                continue;
                            }

                            // Continue if this option has already been found in another section
                            if (parseOptionList[optionId].found)
                                continue;

                            // Get the option value
                            const Variant *value = iniGetDefault(config, section, key, NULL);

                            if (varType(value) == varTypeString && strSize(varStr(value)) == 0)
                                THROW(OptionInvalidValueError, "section '%s', key '%s' must have a value", strPtr(section),
                                strPtr(key));

                            parseOptionList[optionId].found = true;
                            parseOptionList[optionId].source = cfgSourceConfig;

                            // Convert boolean to string
                            if (cfgDefOptionType(optionDefId) == cfgDefOptTypeBoolean)
                            {
                                if (strcasecmp(strPtr(varStr(value)), "n") == 0)
                                    parseOptionList[optionId].negate = true;
                                else if (strcasecmp(strPtr(varStr(value)), "y") != 0)
                                    THROW(OptionInvalidError, "boolean option '%s' must be 'y' or 'n'", strPtr(key));
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
                        THROW(
                            OptionInvalidError, "option '%s' not valid for command '%s'", cfgOptionName(optionId),
                            cfgCommandName(cfgCommand()));
                    }

                    // Error if this option is secure and cannot be passed on the command line
                    if (parseOption->found && parseOption->source == cfgSourceParam && cfgDefOptionSecure(optionDefId))
                    {
                        THROW(
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
                        THROW(OptionInvalidError, "option '%s' cannot have multiple arguments", cfgOptionName(optionId));
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
                                THROW(
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
                                        case cfgDefOptTypeSize:  // CSHANG
                                        {
                                            strLstAddZ(dependValueList, dependValue);   // {uncovered - no depends of other types}
                                            break;                                      // {+uncovered}
                                        }
                                    }
                                }

                                // Build the error string
                                String *error = strNew("option '%s' not valid without option '%s'");

                                if (strLstSize(dependValueList) == 1)
                                    strCat(error, " = %s");
                                else if (strLstSize(dependValueList) > 1)
                                    strCat(error, " in (%s)");

                                // Throw the error
                                THROW(
                                    OptionInvalidError, strPtr(error), cfgOptionName(optionId), strPtr(dependOptionName),
                                    strPtr(strLstJoin(dependValueList, ", ")));
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
                                    THROW(
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
                                    THROW(
                                        OptionInvalidValueError, "'%s' is not valid for '%s' option", strPtr(value),
                                        cfgOptionName(optionId));
                                }
                                TRY_END();

                                // Check value range
                                if (cfgDefOptionAllowRange(commandDefId, optionDefId) &&
                                    (valueDbl < cfgDefOptionAllowRangeMin(commandDefId, optionDefId) ||
                                     valueDbl > cfgDefOptionAllowRangeMax(commandDefId, optionDefId)))
                                {
                                    THROW(
                                        OptionInvalidValueError, "'%s' is out of range for '%s' option", strPtr(value),
                                        cfgOptionName(optionId));
                                }
                            }

                            // If the option has an allow list then check it
                            if (cfgDefOptionAllowList(commandDefId, optionDefId) &&
                                !cfgDefOptionAllowListValueValid(commandDefId, optionDefId, strPtr(value)))
                            {
                                THROW(
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

                            THROW(
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
