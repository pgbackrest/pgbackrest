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

// Parse option name and return option info
typedef struct CfgParseOptionResult
{
    bool found;                                                     // Was the option found?
    ConfigOption id;                                                // Option ID
    unsigned int keyIdx;                                            // Option key index, i.e. option key - 1
    bool negate;                                                    // Was the option negated?
    bool reset;                                                     // Was the option reset?
    bool deprecated;                                                // Is the option deprecated?
} CfgParseOptionResult;

CfgParseOptionResult cfgParseOption(const String *optionName);

// Option id from name
int cfgParseOptionId(const char *optionName);

// Option name from id
const char *cfgParseOptionName(ConfigOption optionId);

// Is the option valid for the command?
bool cfgParseOptionValid(ConfigCommand commandId, ConfigOption optionId);

#endif
