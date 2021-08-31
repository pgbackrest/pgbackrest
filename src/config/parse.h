/***********************************************************************************************************************************
Parse Configuration
***********************************************************************************************************************************/
#ifndef CONFIG_PARSE_H
#define CONFIG_PARSE_H

#include "config/config.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Option type enum
***********************************************************************************************************************************/
typedef enum
{
    cfgOptTypeBoolean,                                              // Boolean
    cfgOptTypeHash,                                                 // Associative array, e.g. key1=val1,key2=val2
    cfgOptTypeInteger,                                              // Signed 64-bit integer
    cfgOptTypeList,                                                 // String list, e.g. val1,val2
    cfgOptTypePath,                                                 // Path string with validation
    cfgOptTypeSize,                                                 // Size, e.g. 1m, 2gb
    cfgOptTypeString,                                               // String
    cfgOptTypeTime,                                                 // Time in seconds, e.g. 23, 1.5
} ConfigOptionType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse the command-line arguments and config file to produce final config data
void configParse(const Storage *storage, unsigned int argListSize, const char *argList[], bool resetLogLevel);

// Get command name by id
const char *cfgParseCommandName(ConfigCommand commandId);

// Get command/role name with custom separator
String *cfgParseCommandRoleName(
    const ConfigCommand commandId, const ConfigCommandRole commandRoleId, const String *separator);

// Convert command role enum to String
const String *cfgParseCommandRoleStr(ConfigCommandRole commandRole);

// Parse option name and return option info
typedef struct CfgParseOptionParam
{
    VAR_PARAM_HEADER;
    bool prefixMatch;                                               // Allow prefix matches, e.g. 'stanz' for 'stanza'
    bool ignoreMissingIndex;                                        // Help requires the base option name to be searchable
} CfgParseOptionParam;

typedef struct CfgParseOptionResult
{
    bool found;                                                     // Was the option found?
    ConfigOption id;                                                // Option ID
    unsigned int keyIdx;                                            // Option key index, i.e. option key - 1
    bool negate;                                                    // Was the option negated?
    bool reset;                                                     // Was the option reset?
    bool deprecated;                                                // Is the option deprecated?
} CfgParseOptionResult;

#define cfgParseOptionP(optionName, ...)                                                                                            \
    cfgParseOption(optionName, (CfgParseOptionParam){VAR_PARAM_INIT, __VA_ARGS__})

CfgParseOptionResult cfgParseOption(const String *const optionName, const CfgParseOptionParam param);

// Default value for the option
const char *cfgParseOptionDefault(ConfigCommand commandId, ConfigOption optionId);

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
bool cfgParseOptionValid(ConfigCommand commandId, ConfigCommandRole commandRoleId, ConfigOption optionId);

#endif
