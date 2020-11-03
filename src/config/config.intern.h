/***********************************************************************************************************************************
Command and Option Configuration Internals

These structures and functions are generally used by modules that create configurations, e.g. config/parse, or modules that
manipulate the configuration as a whole, e.g. protocol/helper, in order to communicate with other processes.

The general-purpose functions for querying the current configuration are found in config.h.
***********************************************************************************************************************************/
#ifndef CONFIG_CONFIG_INTERN_H
#define CONFIG_CONFIG_INTERN_H

#include "config/config.h"

/***********************************************************************************************************************************
The maximum number of keys that an indexed option can have, e.g. pg256-path would be the maximum pg-path option
***********************************************************************************************************************************/
#define CFG_OPTION_KEY_MAX                                          256

/***********************************************************************************************************************************
Configuration data. These structures are not directly user-created or accessible. configParse() creates the structures and uses
cfgInit() to load it as the current configuration. Various cfg*() functions provide access.
***********************************************************************************************************************************/
typedef struct ConfigOptionValue
{
    bool negate;                                                // Is the option negated?
    bool reset;                                                 // Is the option reset?
    unsigned int source;                                        // Where the option came from, i.e. ConfigSource enum
    const Variant *value;                                       // Value
} ConfigOptionValue;

typedef struct Config
{
    MemContext *memContext;                                         // Mem context for config data

    // Generally set by the command parser but can also be set by during execute to change commands, i.e. backup -> expire
    ConfigCommand command;                                          // Current command
    ConfigCommandRole commandRole;                                  // Current command role

    String *exe;                                                    // Location of the executable
    bool help;                                                      // Was help requested for the command?
    StringList *paramList;                                          // Parameters passed to the command (if any)

    // Group options that are related together to allow valid and test checks across all options in the group
    struct
    {
        bool valid;                                                 // Is option group valid for the current command?
        unsigned int indexTotal;                                    // Total number of indexes with values in option group
        unsigned int indexDefault;                                  // Default index (usually 0)
        unsigned int index[CFG_OPTION_KEY_MAX];                     // List of index to key mappings
    } optionGroup[CFG_OPTION_GROUP_TOTAL];

    // Option data
    struct
    {
        bool valid;                                                 // Is option valid for current command?
        const Variant *defaultValue;                                // Default value
        ConfigOptionValue *index;                                   // List of indexed values (only 1 unless the option is indexed)
    } option[CFG_OPTION_TOTAL];
} Config;

/***********************************************************************************************************************************
Init Function
***********************************************************************************************************************************/
// Init with new configuration
void cfgInit(Config *config);

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
// Get the option name using the key index -- i.e. the key that was used during configuration - 1, e.g. to get pg2-host pass 1 to
// keyIdx.
const char *cfgOptionKeyIdxName(ConfigOption optionId, unsigned int keyIdx);

// Convert the key used in the original configuration to a group index. This is used when an option key must be translated into the
// local group index, e.g. during parsing or when getting the value of specific options from a remote.
unsigned int cfgOptionKeyToIdx(ConfigOption optionId, unsigned int key);

// Total indexes for the option if in a group, 1 otherwise.
unsigned int cfgOptionIdxTotal(ConfigOption optionId);

// Invalidate an option so it will not be passed to other processes. This is used to manage deprecated options that have a newer
// option that should be used when possible, e.g. compress and compress-type.
void cfgOptionInvalidate(ConfigOption optionId);

#endif
