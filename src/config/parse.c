/***********************************************************************************************************************************
Command and Option Configuration
***********************************************************************************************************************************/
#include <getopt.h>
#include <string.h>

#include "common/error.h"
#include "config/config.h"

/***********************************************************************************************************************************
Include the automatically generated configuration data
***********************************************************************************************************************************/
// Offset the option values so they don't conflict with getopt_long return codes
#define PARSE_OPTION_OFFSET                                         100000

#include "parse.auto.c"

/***********************************************************************************************************************************
Map command names to ids and vice versa
***********************************************************************************************************************************/
void
configParse(int argListSize, const char *argList[])
{
    // Reset optind to 1 in case getopt_long has been called before
    optind = 1;

    // Don't error automatically on unknown options - they will be processed in the loop below
    opterr = false;

    // Only the first non-option parameter should be treated as a command so track if the command has been set
    bool commandSet = false;

    // Parse options
    int option;
    int optionIdx;

    while ((option = getopt_long(argListSize, (char **)argList, "-", optionList, &optionIdx)) != -1)
    {
        switch (option)
        {
            // Perl options that should be ignored
            case 0:
                break;

            // Parse parameters that are not options, i.e. commands and parameters passed to commands
            case 1:
                // The first parameter should be the command
                if (!commandSet)
                {
                    // Try getting the command from the valid command list
                    TRY_BEGIN()
                    {
                        cfgCommandSet(cfgCommandId(argList[optind - 1]));
                    }
                    // Assert error means the command does not exist, which is correct for all usages but this one (since we don't
                    // have any control over what the user passes), so modify the error code and message.
                    CATCH(AssertError)
                    {
                        THROW(CommandInvalidError, "invalid command '%s'", argList[optind - 1]);
                    }
                    TRY_END();
                }
                // Else implement additional parameters here for more complex commands

                commandSet = true;
                break;

            // If the option is unknown generate an error
            case '?':
                THROW(OptionInvalidError, "invalid option '%s'", argList[optind - 1]);
                break;                                              // {uncoverable - case statement does not return}

            // Parse valid option
            default:
                break;
        }
    }
}
