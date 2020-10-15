/***********************************************************************************************************************************
Parse Configuration
***********************************************************************************************************************************/
#ifndef CONFIG_PARSE_H
#define CONFIG_PARSE_H

#include "config/config.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse the command-line arguments and config file to produce final config data
void configParse(unsigned int argListSize, const char *argList[], bool resetLogLevel);

// Parse option name and return id and index
typedef struct CfgParseOptionResult
{
    bool found;
    ConfigOption optionId;
    unsigned int optionIdx;
    bool negate;
    bool reset;
    bool deprecated;
} CfgParseOptionResult;

CfgParseOptionResult cfgParseOption(const String *optionName);

#endif
