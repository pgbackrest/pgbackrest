/***********************************************************************************************************************************
Command and Option Configuration Definition
***********************************************************************************************************************************/
#ifndef CONFIG_DEFINE_H
#define CONFIG_DEFINE_H

#include <sys/types.h>

/***********************************************************************************************************************************
Section enum - defines which sections of the config an option can appear in
***********************************************************************************************************************************/
typedef enum
{
    cfgDefSectionCommandLine,                                       // command-line only
    cfgDefSectionGlobal,                                            // command-line or in any config section
    cfgDefSectionStanza,                                            // command-line or in any config stanza section
} ConfigDefSection;

#include "common/type/string.h"
#include "config/config.auto.h"
#include "config/define.auto.h"

/***********************************************************************************************************************************
Define global section name
***********************************************************************************************************************************/
#define CFGDEF_SECTION_GLOBAL                                       "global"
    STRING_DECLARE(CFGDEF_SECTION_GLOBAL_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Command total
unsigned int cfgDefCommandTotal(void);

// Command help
const char *cfgDefCommandHelpDescription(ConfigCommand commandDefId);
const char *cfgDefCommandHelpSummary(ConfigCommand commandDefId);

// Option allow lists
bool cfgDefOptionAllowList(ConfigCommand commandDefId, ConfigOption optionDefId);
bool cfgDefOptionAllowListValueValid(ConfigCommand commandDefId, ConfigOption optionDefId, const char *value);

// Allow range
bool cfgDefOptionAllowRange(ConfigCommand commandDefId, ConfigOption optionDefId);
double cfgDefOptionAllowRangeMax(ConfigCommand commandDefId, ConfigOption optionDefId);
double cfgDefOptionAllowRangeMin(ConfigCommand commandDefId, ConfigOption optionDefId);

// Default value for the option
const char *cfgDefOptionDefault(ConfigCommand commandDefId, ConfigOption optionDefId);

// Dependencies and depend lists
bool cfgDefOptionDepend(ConfigCommand commandDefId, ConfigOption optionDefId);
ConfigOption cfgDefOptionDependOption(ConfigCommand commandDefId, ConfigOption optionDefId);
unsigned int cfgDefOptionDependValueTotal(ConfigCommand commandDefId, ConfigOption optionDefId);
bool cfgDefOptionDependValueValid(ConfigCommand commandDefId, ConfigOption optionDefId, const char *value);
const char *cfgDefOptionDependValue(ConfigCommand commandDefId, ConfigOption optionDefId, unsigned int valueId);

// Option help
const char *cfgDefOptionHelpDescription(ConfigCommand commandDefId, ConfigOption optionDefId);
const char *cfgDefOptionHelpSummary(ConfigCommand commandDefId, ConfigOption optionDefId);

// Option help name alt
bool cfgDefOptionHelpNameAlt(ConfigOption optionDefId);
const char *cfgDefOptionHelpNameAltValue(ConfigOption optionDefId, unsigned int valueId);
unsigned int cfgDefOptionHelpNameAltValueTotal(ConfigOption optionDefId);

// Option help section
const char *cfgDefOptionHelpSection(ConfigOption optionDefId);

// Option id by name
int cfgDefOptionId(const char *optionName);

// Total indexed values for option
unsigned int cfgDefOptionIndexTotal(ConfigOption optionDefId);

// Is the option for internal use only?
bool cfgDefOptionInternal(ConfigCommand commandDefId, ConfigOption optionDefId);

// Does the option accept multiple values?
bool cfgDefOptionMulti(ConfigOption optionDefId);

// Name of the option
const char *cfgDefOptionName(ConfigOption optionDefId);

// Is the option required
bool cfgDefOptionRequired(ConfigCommand commandDefId, ConfigOption optionDefId);

// Get option section
ConfigDefSection cfgDefOptionSection(ConfigOption optionDefId);

// Does the option need to be protected from showing up in logs, command lines, etc?
bool cfgDefOptionSecure(ConfigOption optionDefId);

// Option total
unsigned int cfgDefOptionTotal(void);

// Get option data type
int cfgDefOptionType(ConfigOption optionDefId);

// Is the option valid for the command?
bool cfgDefOptionValid(ConfigCommand commandDefId, ConfigOption optionDefId);

#endif
