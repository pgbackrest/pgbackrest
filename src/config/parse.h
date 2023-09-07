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
    cfgOptTypeStringId,                                             // StringId
    cfgOptTypeTime,                                                 // Time in seconds, e.g. 23, 1.5
} ConfigOptionType;

/***********************************************************************************************************************************
Underlying data type for an option
***********************************************************************************************************************************/
typedef enum
{
    cfgOptDataTypeBoolean,                                          // Boolean
    cfgOptDataTypeHash,                                             // Hash
    cfgOptDataTypeInteger,                                          // Signed 64-bit integer
    cfgOptDataTypeList,                                             // List
    cfgOptDataTypeString,                                           // String
    cfgOptDataTypeStringId,                                         // StringId
} ConfigOptionDataType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse the command-line arguments and config file to produce final config data
typedef struct CfgParseParam
{
    VAR_PARAM_HEADER;
    bool noResetLogLevel;                                           // Do not reset log level
    bool noConfigLoad;                                              // Do not reload the config file
    const String *stanza;                                           // Load config as stanza
} CfgParseParam;

#define cfgParseP(storage, argListSize, argList, ...)                                                                              \
    cfgParse(storage, argListSize, argList, (CfgParseParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN void cfgParse(const Storage *storage, unsigned int argListSize, const char *argList[], CfgParseParam param);

// Get command name by id
FN_EXTERN const char *cfgParseCommandName(ConfigCommand commandId);

// Get command/role name with custom separator
FN_EXTERN String *cfgParseCommandRoleName(const ConfigCommand commandId, const ConfigCommandRole commandRoleId);

// Convert command role enum to String
FN_EXTERN const String *cfgParseCommandRoleStr(ConfigCommandRole commandRole);

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
    bool beta;                                                      // Is the option in beta?
} CfgParseOptionResult;

#define cfgParseOptionP(optionName, ...)                                                                                            \
    cfgParseOption(optionName, (CfgParseOptionParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN CfgParseOptionResult cfgParseOption(const String *const optionName, const CfgParseOptionParam param);

// Default value for the option
FN_EXTERN const String *cfgParseOptionDefault(ConfigCommand commandId, ConfigOption optionId);

// Option name from id
FN_EXTERN const char *cfgParseOptionName(ConfigOption optionId);

// Option name from id and key
FN_EXTERN const char *cfgParseOptionKeyIdxName(ConfigOption optionId, unsigned int keyIdx);

// Does the option need to be protected from showing up in logs, command lines, etc?
FN_EXTERN bool cfgParseOptionSecure(ConfigOption optionId);

// Option data type
FN_EXTERN ConfigOptionType cfgParseOptionType(ConfigOption optionId);

// Get the underlying data type for an option
FN_EXTERN ConfigOptionDataType cfgParseOptionDataType(ConfigOption optionId);

// Is the option required?
FN_EXTERN bool cfgParseOptionRequired(ConfigCommand commandId, ConfigOption optionId);

FN_EXTERN StringList *cfgParseStanzaList(void);

// Is the option valid for the command?
FN_EXTERN bool cfgParseOptionValid(ConfigCommand commandId, ConfigCommandRole commandRoleId, ConfigOption optionId);

#endif
