/***********************************************************************************************************************************
Parse Configuration
***********************************************************************************************************************************/
#ifndef CONFIG_PARSE_H
#define CONFIG_PARSE_H

#include "config/config.h"
#include "config/parse.auto.h"

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

// Default value for the option
const char *cfgParseOptionDefault(ConfigCommand commandId, ConfigOption optionId);

// Option id from name
int cfgParseOptionId(const char *optionName);

// Option name from id
const char *cfgParseOptionName(ConfigOption optionId);

// Option name from id and key
const char *cfgParseOptionKeyIdxName(ConfigOption optionId, unsigned int keyIdx);

// Does the option need to be protected from showing up in logs, command lines, etc?
bool cfgParseOptionSecure(ConfigOption optionId);

// Option data type
ConfigOptionType cfgParseOptionType(ConfigOption optionId);

// Is the option required?
bool cfgParseOptionRequired(ConfigCommand commandId, ConfigOption optionId);

// Is the option valid for the command?
bool cfgParseOptionValid(ConfigCommand commandId, ConfigOption optionId);

#endif
