/***********************************************************************************************************************************
Parse Configuration Yaml
***********************************************************************************************************************************/
#ifndef BUILD_CONFIG_PARSE_H
#define BUILD_CONFIG_PARSE_H

/***********************************************************************************************************************************
Types
***********************************************************************************************************************************/
typedef struct BldCfgCommand
{
    const String *name;                                             // Name
    bool logFile;                                                   // Does the command write automatically to a log file?
    const String *logLevelDefault;                                  // Default log level
    bool lockRequired;                                              // Is a lock required
    bool lockRemoteRequired;                                        // Is a remote lock required?
    const String *lockType;                                         // Lock type
} BldCfgCommand;

typedef struct BldCfgOptionGroup
{
    const String *name;                                             // Name
} BldCfgOptionGroup;

typedef struct BldCfgOptionCommand
{
    const String *name;                                             // Name
    const StringList *allowList;                                    // Allowed value list
} BldCfgOptionCommand;

typedef struct BldCfgOption
{
    const String *name;                                             // Name
    const String *type;                                             // Option type, e.g. integer
    const String *group;                                            // Option group, if any
    const StringList *allowList;                                    // Allowed value list
    const List *cmdList;                                            // Command override list
} BldCfgOption;

typedef struct BldCfg
{
    List *commandList;                                              // Command list
    List *optGrpList;                                               // Option group list
    List *optList;                                                  // Option list
} BldCfg;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse config.yaml
BldCfg bldCfgParse(const Storage *const storageRepo);

#endif
