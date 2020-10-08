/***********************************************************************************************************************************
Command and Option Configuration Internals
***********************************************************************************************************************************/
#ifndef CONFIG_CONFIG_INTERN_H
#define CONFIG_CONFIG_INTERN_H

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
typedef struct ConfigOptionValue
{
    bool negate;                                                // Is the option negated?
    bool reset;                                                 // Is the option reset?
    unsigned int source;                                        // Where the option came from, i.e. ConfigSource enum
    Variant *value;                                             // Value
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
        unsigned int **indexList;                                   // List of indexes
    } optionGroup[CFG_OPTION_GROUP_TOTAL];

    // Option data
    struct
    {
        bool valid;                                                 // Is option valid for current command?
        Variant *default;                                           // Default value
        ConfigOptionValue **valueList;                              // List of values (1 unless the option is indexed)
    } option[CFG_OPTION_TOTAL];
} Config;

#endif
