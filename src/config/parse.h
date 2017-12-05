/***********************************************************************************************************************************
Parse Configuration
***********************************************************************************************************************************/
#ifndef CONFIG_PARSE_H
#define CONFIG_PARSE_H

#include "config/config.h"

/***********************************************************************************************************************************
Struct to hold options parsed from the command line (??? move back to parse.c once perl exec works on full config data)
***********************************************************************************************************************************/
typedef struct ParseOption
{
    bool found:1;                                                   // Was the option found on the command line?
    bool negate:1;                                                  // Was the option negated on the command line?
    StringList *valueList;                                          // List of values found
} ParseOption;

/***********************************************************************************************************************************
Struct to hold all parsed data  (??? move back to parse.c once perl exec works on full config data)
***********************************************************************************************************************************/
typedef struct ParseData
{
    ConfigCommand command;                                          // Command found
    StringList *perlOptionList;                                     // List of perl options
    StringList *commandArgList;                                     // List of command arguments
    ParseOption parseOptionList[CFG_OPTION_TOTAL];               // List of parsed options
} ParseData;

ParseData *configParseArg(int argListSize, const char *argList[]);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void configParse(int argListSize, const char *argList[]);

#endif
