/***********************************************************************************************************************************
Command and Option Configuration Internals

These structures and functions are generally used by modules that create configurations, e.g. config/parse, or modules that
manipulate the configuration as a whole, e.g. protocol/helper, in order to communicate with other processes.

The general-purpose functions for querying the current configuration are found in config.h.
***********************************************************************************************************************************/
#ifndef CONFIG_CONFIG_INTERN_H
#define CONFIG_CONFIG_INTERN_H

#include "config/config.h"
#include "config/parse.h"

/***********************************************************************************************************************************
Configuration data. These structures are not directly user-created or accessible. configParse() creates the structures and uses
cfgInit() to load it as the current configuration. Various cfg*() functions provide access.
***********************************************************************************************************************************/
typedef union ConfigOptionValueType
{
    bool boolean;                                           // Boolean
    int64_t integer;                                        // Integer
    const KeyValue *keyValue;                               // KeyValue
    const VariantList *list;                                // VariantList
    const String *string;                                   // String
    StringId stringId;                                      // StringId
} ConfigOptionValueType;

typedef struct ConfigOptionValue
{
    bool set;                                                   // Is the option set?
    bool negate;                                                // Is the option negated?
    bool reset;                                                 // Is the option reset?
    unsigned int source;                                        // Where the option came from, i.e. ConfigSource enum
    const String *display;                                      // Current display value, if any. Used for messages, etc.
    ConfigOptionValueType value;                                // Option value
} ConfigOptionValue;

typedef struct Config
{
    MemContext *memContext;                                         // Mem context for config data

    // Generally set by the command parser but can also be set during execute to change commands, i.e. backup -> expire
    ConfigCommand command;                                          // Current command
    ConfigCommandRole commandRole;                                  // Current command role

    String *exe;                                                    // Location of the executable
    bool help;                                                      // Was help requested for the command?
    bool lockRequired;                                              // Is an immediate lock required?
    bool lockRemoteRequired;                                        // Is a lock required on the remote?
    LockType lockType;                                              // Lock type required
    bool logFile;                                                   // Will the command log to a file?
    LogLevel logLevelDefault;                                       // Default log level
    StringList *paramList;                                          // Parameters passed to the command (if any)

    // Group options that are related together to allow valid and test checks across all options in the group
    struct
    {
        const char *name;                                           // Name
        bool valid;                                                 // Is option group valid for the current command?
        unsigned int indexTotal;                                    // Total number of indexes with values in option group
        bool indexDefaultExists;                                    // Is there a default index for non-indexed functions?
        unsigned int indexDefault;                                  // Default index (usually 0)
        unsigned int *indexMap;                                     // List of index to key index mappings
    } optionGroup[CFG_OPTION_GROUP_TOTAL];

    // Option data
    struct
    {
        const char *name;                                           // Name
        bool valid;                                                 // Is option valid for current command?
        bool group;                                                 // In a group?
        unsigned int groupId;                                       // Id if in a group
        ConfigOptionDataType dataType;                              // Underlying data type
        const String *defaultValue;                                 // Default value
        ConfigOptionValue *index;                                   // List of indexed values (only 1 unless the option is indexed)
    } option[CFG_OPTION_TOTAL];
} Config;

/***********************************************************************************************************************************
Init Function
***********************************************************************************************************************************/
// Init with new configuration
void cfgInit(Config *config);

/***********************************************************************************************************************************
Command Functions
***********************************************************************************************************************************/
// List of retry intervals for local jobs. The interval for the first retry will always be 0. NULL if there are no retries.
VariantList *cfgCommandJobRetry(void);

/***********************************************************************************************************************************
Option Group Functions
***********************************************************************************************************************************/
// Is the option in a group?
bool cfgOptionGroup(ConfigOption optionId);

// Group id if the option is in a group
unsigned int cfgOptionGroupId(ConfigOption optionId);

/***********************************************************************************************************************************
Option Functions
***********************************************************************************************************************************/
// Option default - should only be called by the help command
const String *cfgOptionDefault(ConfigOption optionId);

// Format a variant for display using the supplied option type. cfgOptionDisplay()/cfgOptionIdxDisplay() should be used whenever
// possible, but sometimes the variant needs to be manipulated before being formatted.
const String *cfgOptionDisplayVar(const Variant *const value, const ConfigOptionType optionType);

// Convert the key used in the original configuration to a group index. This is used when an option key must be translated into the
// local group index, e.g. during parsing or when getting the value of specific options from a remote.
unsigned int cfgOptionKeyToIdx(ConfigOption optionId, unsigned int key);

// Total indexes for the option if in a group, 1 otherwise.
unsigned int cfgOptionIdxTotal(ConfigOption optionId);

// Get config option as a Variant
Variant *cfgOptionIdxVar(ConfigOption optionId, unsigned int optionIdx);

__attribute__((always_inline)) static inline Variant *
cfgOptionVar(const ConfigOption optionId)
{
    return cfgOptionIdxVar(optionId, cfgOptionIdxDefault(optionId));
}

// Invalidate an option so it will not be passed to other processes. This is used to manage deprecated options that have a newer
// option that should be used when possible, e.g. compress and compress-type.
void cfgOptionInvalidate(ConfigOption optionId);

#endif
