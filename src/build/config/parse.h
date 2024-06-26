/***********************************************************************************************************************************
Parse Configuration Yaml
***********************************************************************************************************************************/
#ifndef BUILD_CONFIG_PARSE_H
#define BUILD_CONFIG_PARSE_H

#include "storage/storage.h"

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
#define OPT_TYPE_INTEGER                                            "integer"
STRING_DECLARE(OPT_TYPE_INTEGER_STR);
#define OPT_TYPE_LIST                                               "list"
STRING_DECLARE(OPT_TYPE_LIST_STR);
#define OPT_TYPE_PATH                                               "path"
STRING_DECLARE(OPT_TYPE_PATH_STR);
#define OPT_TYPE_SIZE                                               "size"
STRING_DECLARE(OPT_TYPE_SIZE_STR);
#define OPT_TYPE_STRING                                             "string"
STRING_DECLARE(OPT_TYPE_STRING_STR);
#define OPT_TYPE_STRING_ID                                          "string-id"
STRING_DECLARE(OPT_TYPE_STRING_ID_STR);
#define OPT_TYPE_TIME                                               "time"
STRING_DECLARE(OPT_TYPE_TIME_STR);

/***********************************************************************************************************************************
Option constants
***********************************************************************************************************************************/
#define OPT_BETA                                                    "beta"
STRING_DECLARE(OPT_BETA_STR);
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
    const String *name;                                             // Name
    bool internal;                                                  // Is the command internal?
    bool logFile;                                                   // Does the command write automatically to a log file?
    const String *logLevelDefault;                                  // Default log level
    bool lockRequired;                                              // Is a lock required
    bool lockRemoteRequired;                                        // Is a remote lock required?
    const String *lockType;                                         // Lock type
    bool parameterAllowed;                                          // Are command line parameters allowed?
    const StringList *roleList;                                     // Roles valid for the command
} BldCfgCommand;

typedef struct BldCfgOptionGroup
{
    const String *name;                                             // Name
} BldCfgOptionGroup;

typedef struct BldCfgOption BldCfgOption;                           // Forward declaration

typedef struct BldCfgOptionDepend
{
    const BldCfgOption *option;                                     // Option dependency is on
    const String *defaultValue;                                     // Default value, if any, when dependency is not resolved
    const StringList *valueList;                                    // Allowed value list
} BldCfgOptionDepend;

typedef struct BldCfgOptionDeprecate
{
    const String *name;                                             // Deprecated option name
    bool indexed;                                                   // Can the deprecation be indexed?
    bool unindexed;                                                 // Can the deprecation be unindexed?
} BldCfgOptionDeprecate;

typedef struct BldCfgOptionCommand
{
    const String *name;                                             // Name
    bool internal;                                                  // Is the option internal?
    bool required;                                                  // Is the option required?
    const String *defaultValue;                                     // Default value, if any
    const BldCfgOptionDepend *depend;                               // Dependency, if any
    const List *allowList;                                          // Allowed value list
    const StringList *roleList;                                     // Roles valid for the command
} BldCfgOptionCommand;

typedef struct BldCfgOptionValue
{
    const String *value;                                            // Option value
    const String *condition;                                        // Is the option conditionally compiled?
} BldCfgOptionValue;

struct BldCfgOption
{
    const String *name;                                             // Name
    const String *type;                                             // Option type, e.g. integer
    const String *section;                                          // Option section, i.e. stanza or global
    bool boolLike;                                                  // Option accepts y/n and can be treated as bool?
    bool internal;                                                  // Is the option internal?
    bool beta;                                                      // Is the option beta?
    bool required;                                                  // Is the option required?
    bool negate;                                                    // Can the option be negated?
    bool reset;                                                     // Can the option be reset?
    const String *defaultValue;                                     // Default value, if any
    bool defaultLiteral;                                            // Should default be interpreted literally, i.e. not a string
    const String *group;                                            // Option group, if any
    bool secure;                                                    // Does the option contain a secret?
    const BldCfgOptionDepend *depend;                               // Dependency, if any
    const List *allowList;                                          // Allowed value list
    const String *allowRangeMin;                                    // Allow range min, if any
    const String *allowRangeMax;                                    // Allow range max, if any
    const List *cmdList;                                            // Command override list
    const List *deprecateList;                                      // List of option deprecations
};

typedef struct BldCfg
{
    const List *cmdList;                                            // Command list
    const List *optGrpList;                                         // Option group list
    const List *optList;                                            // Option list
    const List *optResolveList;                                     // Option list in resolved dependency order
} BldCfg;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse config.yaml
BldCfg bldCfgParse(const Storage *const storageRepo);

#endif
