/***********************************************************************************************************************************
Command and Option Parse
***********************************************************************************************************************************/
#include <assert.h>
#include <getopt.h>
#include <string.h>

#include "common/error.h"
#include "common/memContext.h"
#include "config/parse.h"

/***********************************************************************************************************************************
Include the automatically generated configuration data
***********************************************************************************************************************************/
// Offset the option values so they don't conflict with getopt_long return codes
#define PARSE_OPTION_FLAG                                         (1 << 31)

// Add a flag for negation rather than checking "-no-"
#define PARSE_NEGATE_FLAG                                         (1 << 30)

// Mask to exclude all flags and get at the actual option id (only 12 bits allowed for option id, the rest reserved for flags)
#define PARSE_OPTION_MASK                                         0xFFF

#include "parse.auto.c"

/***********************************************************************************************************************************
Parse the command-line arguments and produce preliminary parse data

This function only checks for obvious errors.  Dependencies, types, etc, are checked later when data from the config file is
available.
***********************************************************************************************************************************/
ParseData *
configParseArg(int argListSize, const char *argList[])
{
    // Allocate memory for the parse data
    ParseData *parseData = memNew(sizeof(ParseData));

    // Reset optind to 1 in case getopt_long has been called before
    optind = 1;

    // Don't error automatically on unknown options - they will be processed in the loop below
    opterr = false;

    // Only the first non-option parameter should be treated as a command so track if the command has been set
    bool commandSet = false;

    // Parse options
    int option;                                                     // Code returned by getopt_long
    int optionListIdx;                                              // Index of option is list (if an option was returned)
    ConfigOption optionId;                                          // Option id extracted from option var
    bool negate;                                                    // Option is being negated
    bool argFound = false;                                          // Track args found to decide on error or help at the end

    while ((option = getopt_long(argListSize, (char **)argList, "-:", optionList, &optionListIdx)) != -1)
    {
        switch (option)
        {
            // Add perl options (if any) to the list
            case 0:
                if (parseData->perlOptionList == NULL)
                    parseData->perlOptionList = strLstNew();

                strLstAdd(parseData->perlOptionList, strNew(optarg));
                break;

            // Parse arguments that are not options, i.e. commands and parameters passed to commands
            case 1:
                // The first argument should be the command
                if (!commandSet)
                {
                    // Try getting the command from the valid command list
                    TRY_BEGIN()
                    {
                        parseData->command = cfgCommandId(argList[optind - 1]);
                    }
                    // Assert error means the command does not exist, which is correct for all usages but this one (since we don't
                    // have any control over what the user passes), so modify the error code and message.
                    CATCH(AssertError)
                    {
                        THROW(CommandInvalidError, "invalid command '%s'", argList[optind - 1]);
                    }
                    TRY_END();
                }
                // Additioal arguments are command arguments
                else
                {
                    if (parseData->commandArgList == NULL)
                        parseData->commandArgList = strLstNew();

                    strLstAdd(parseData->commandArgList, strNew(argList[optind - 1]));
                }

                commandSet = true;
                break;

            // If the option is unknown then error
            case '?':
                THROW(OptionInvalidError, "invalid option '%s'", argList[optind - 1]);
                break;                                              // {uncoverable - case statement does not return}

            // If the option is missing an argument then error
            case ':':
                THROW(OptionInvalidError, "option '%s' requires argument", argList[optind - 1]);
                break;                                              // {uncoverable - case statement does not return}

            // Parse valid option
            default:
                // Get option id and flags from the option code
                optionId = option & PARSE_OPTION_MASK;
                negate = option & PARSE_NEGATE_FLAG;

                // Make sure the option id is valid
                assert(optionId < CFG_OPTION_TOTAL);

                // If the the option has not been found yet then set it
                if (!parseData->parseOptionList[optionId].found)
                {
                    parseData->parseOptionList[optionId].found = true;
                    parseData->parseOptionList[optionId].negate = negate;

                    // Only set the argument if the option requires one
                    if (optionList[optionListIdx].has_arg == required_argument)
                        parseData->parseOptionList[optionId].valueList = strLstAdd(strLstNew(), strNew(optarg));
                }
                else
                {
                    // Make sure option is not negated more than once.  It probably wouldn't hurt anything to accept this case but
                    // there's no point in allowing the user to be sloppy.
                    if (parseData->parseOptionList[optionId].negate && negate)
                        THROW(OptionInvalidError, "option '%s' is negated multiple times", cfgOptionName(optionId));

                    // Don't allow an option to be both set and negated
                    if (parseData->parseOptionList[optionId].negate != negate)
                        THROW(OptionInvalidError, "option '%s' cannot be set and negated", cfgOptionName(optionId));

                    // Error if this option does not allow multiple arguments (!!! IMPLEMENT THIS IN DEFINE.C)
                    if (!(cfgDefOptionType(cfgOptionDefIdFromId(optionId)) == cfgDefOptTypeHash ||
                          cfgDefOptionType(cfgOptionDefIdFromId(optionId)) == cfgDefOptTypeList))
                    {
                        THROW(OptionInvalidError, "option '%s' cannot have multiple arguments", cfgOptionName(optionId));
                    }

                    // Add the argument
                    strLstAdd(parseData->parseOptionList[optionId].valueList, strNew(optarg));
                }

                break;
        }

        // Arg has been found
        argFound = true;
    }

    // Handle command not found
    if (!commandSet)
    {
        // If there are args then error
        if (argFound)
            THROW(CommandRequiredError, "no command found");

        // Otherwise set the comand to help
        parseData->command = cfgCmdHelp;
    }

    // Return the parse data
    return parseData;
}

/***********************************************************************************************************************************
Parse the command-line arguments and config file to produce final config data
***********************************************************************************************************************************/
void
configParse(int argListSize, const char *argList[])
{
    // Parse the command line
    ParseData *parseData = configParseArg(argListSize, argList);

    // Set the command
    cfgCommandSet(parseData->command);

    // Free the parse data
    memFree(parseData);
}
