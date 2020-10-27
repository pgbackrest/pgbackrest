/***********************************************************************************************************************************
Command and Option Configuration Internals
***********************************************************************************************************************************/
#ifndef CONFIG_CONFIG_INTERN_H
#define CONFIG_CONFIG_INTERN_H

#include "config/config.h"

/***********************************************************************************************************************************
Define index max
***********************************************************************************************************************************/
#define CFG_OPTION_INDEX_MAX                                        256

/***********************************************************************************************************************************
!!!
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
        unsigned int indexTotal;                                    // Max index in option group
        unsigned int indexDefault;                                  // Default index (usually 0)
        unsigned int index[CFG_OPTION_INDEX_MAX];                   // List of indexes
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
// Get the option name using the key -- i.e. the key that was used during configuration, e.g. the 2 in pg2-host
const char *cfgOptionKeyIdxName(ConfigOption optionId, unsigned int optionIdx);

// Convert the key used in the original configuration to a group index. This is used when an option key must be translated into the
// local group index, e.g. during parsing or when getting the value of specific options from a remote.
unsigned int cfgOptionKeyToIdx(ConfigOption optionId, unsigned int key);

// Total indexes for the option if in a group, 1 otherwise.
unsigned int cfgOptionIdxTotal(ConfigOption optionId);

// Invalidate an option so it will not be passed to other processes. This is used to manage deprecated options that have a newer
// option that should be used when possible, e.g. compress and compress-type.
void cfgOptionInvalidate(ConfigOption optionId);

#endif
