/***********************************************************************************************************************************
Parse Configuration Yaml
***********************************************************************************************************************************/
#ifndef BUILD_CONFIG_PARSE_H
#define BUILD_CONFIG_PARSE_H

#include "common/type/stringList.h"

/***********************************************************************************************************************************
Command role constants
***********************************************************************************************************************************/
#define CMD_ROLE_ASYNC                                              "async"
    STRING_DECLARE(CMD_ROLE_ASYNC_STR);
#define CMD_ROLE_LOCAL                                              "local"
    STRING_DECLARE(CMD_ROLE_LOCAL_STR);
#define CMD_ROLE_MAIN                                               "main"
    STRING_DECLARE(CMD_ROLE_MAIN_STR);
#define CMD_ROLE_REMOTE                                             "remote"
    STRING_DECLARE(CMD_ROLE_REMOTE_STR);

/***********************************************************************************************************************************
Command constants
***********************************************************************************************************************************/
#define CMD_HELP                                                    "help"
    STRING_DECLARE(CMD_HELP_STR);
#define CMD_VERSION                                                 "version"
    STRING_DECLARE(CMD_VERSION_STR);

/***********************************************************************************************************************************
Option type constants
***********************************************************************************************************************************/
#define OPT_TYPE_BOOLEAN                                            "boolean"
    STRING_DECLARE(OPT_TYPE_BOOLEAN_STR);
#define OPT_TYPE_HASH                                               "hash"
    STRING_DECLARE(OPT_TYPE_HASH_STR);
#define OPT_TYPE_LIST                                               "list"
    STRING_DECLARE(OPT_TYPE_LIST_STR);
#define OPT_TYPE_STRING                                             "string"
    STRING_DECLARE(OPT_TYPE_STRING_STR);
#define OPT_TYPE_TIME                                               "time"
    STRING_DECLARE(OPT_TYPE_TIME_STR);

/***********************************************************************************************************************************
Option constants
***********************************************************************************************************************************/
#define OPT_STANZA                                                  "stanza"
    STRING_DECLARE(OPT_STANZA_STR);

/***********************************************************************************************************************************
Section constants
***********************************************************************************************************************************/
#define SECTION_COMMAND_LINE                                        "command-line"
    STRING_DECLARE(SECTION_COMMAND_LINE_STR);
#define SECTION_GLOBAL                                              "global"
    STRING_DECLARE(SECTION_GLOBAL_STR);
#define SECTION_STANZA                                              "stanza"
    STRING_DECLARE(SECTION_STANZA_STR);

/***********************************************************************************************************************************
Types
***********************************************************************************************************************************/
typedef struct BldCfgCommand
{
    const String *const name;                                       // Name
    const bool logFile;                                             // Does the command write automatically to a log file?
    const String *const logLevelDefault;                            // Default log level
    const bool lockRequired;                                        // Is a lock required
    const bool lockRemoteRequired;                                  // Is a remote lock required?
    const String *const lockType;                                   // Lock type
    const bool parameterAllowed;                                    // Are command line parameters allowed?
    const StringList *const roleList;                               // Roles valid for the command
} BldCfgCommand;

typedef struct BldCfgOptionGroup
{
    const String *const name;                                       // Name
    const unsigned int indexTotal;                                  // Total indexes for option
} BldCfgOptionGroup;

typedef struct BldCfgOption BldCfgOption;                           // Forward declaration

typedef struct BldCfgOptionDepend
{
    const BldCfgOption *const option;                               // Option dependency is on
    const StringList *const valueList;                              // Allowed value list
} BldCfgOptionDepend;

typedef struct BldCfgOptionDeprecate
{
    const String *const name;                                       // Deprecated option name
    const unsigned int index;                                       // Option index to deprecate
    const bool reset;                                               // Does the deprecated option allow reset
} BldCfgOptionDeprecate;

typedef struct BldCfgOptionCommand
{
    const String *const name;                                       // Name
    const bool required;                                            // Is the option required?
    const String *const defaultValue;                               // Default value, if any
    const BldCfgOptionDepend *const depend;                         // Dependency, if any
    const StringList *const allowList;                              // Allowed value list
    const StringList *const roleList;                               // Roles valid for the command
} BldCfgOptionCommand;

struct BldCfgOption
{
    const String *const name;                                       // Name
    const String *const type;                                       // Option type, e.g. integer
    const String *const section;                                    // Option section, i.e. stanza or global
    const bool required;                                            // Is the option required?
    const bool negate;                                              // Can the option be negated?
    const bool reset;                                               // Can the option be reset?
    const String *const defaultValue;                               // Default value, if any
    const bool defaultLiteral;                                      // Should default be interpreted literally, i.e. not a string
    const String *const group;                                      // Option group, if any
    const bool secure;                                              // Does the option contain a secret?
    const BldCfgOptionDepend *const depend;                         // Dependency, if any
    const StringList *const allowList;                              // Allowed value list
    const String *const allowRangeMin;                              // Allow range min, if any
    const String *const allowRangeMax;                              // Allow range max, if any
    const List *const cmdList;                                      // Command override list
    const StringList *const cmdRoleList;                            // Roles valid for the option
    const List *const deprecateList;                                // List of option deprecations
};

typedef struct BldCfg
{
    const List *const cmdList;                                      // Command list
    const List *const optGrpList;                                   // Option group list
    const List *const optList;                                      // Option list
    const List *const optResolveList;                               // Option list in resolved dependency order
} BldCfg;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse config.yaml
BldCfg bldCfgParse(const Storage *const storageRepo);

#endif
