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

// Option allow lists
bool cfgDefOptionAllowList(ConfigCommand commandId, ConfigOption optionId);
bool cfgDefOptionAllowListValueValid(ConfigCommand commandId, ConfigOption optionId, const char *value);

// Allow range
bool cfgDefOptionAllowRange(ConfigCommand commandId, ConfigOption optionId);
int64_t cfgDefOptionAllowRangeMax(ConfigCommand commandId, ConfigOption optionId);
int64_t cfgDefOptionAllowRangeMin(ConfigCommand commandId, ConfigOption optionId);

// Default value for the option
const char *cfgDefOptionDefault(ConfigCommand commandId, ConfigOption optionId);

// Dependencies and depend lists
bool cfgDefOptionDepend(ConfigCommand commandId, ConfigOption optionId);
ConfigOption cfgDefOptionDependOption(ConfigCommand commandId, ConfigOption optionId);
unsigned int cfgDefOptionDependValueTotal(ConfigCommand commandId, ConfigOption optionId);
bool cfgDefOptionDependValueValid(ConfigCommand commandId, ConfigOption optionId, const char *value);
const char *cfgDefOptionDependValue(ConfigCommand commandId, ConfigOption optionId, unsigned int valueId);

// Option help
const char *cfgDefOptionHelpDescription(ConfigCommand commandId, ConfigOption optionId);
const char *cfgDefOptionHelpSummary(ConfigCommand commandId, ConfigOption optionId);

// Option help name alt
bool cfgDefOptionHelpNameAlt(ConfigOption optionId);
const char *cfgDefOptionHelpNameAltValue(ConfigOption optionId, unsigned int valueId);
unsigned int cfgDefOptionHelpNameAltValueTotal(ConfigOption optionId);

// Option help section
const char *cfgDefOptionHelpSection(ConfigOption optionId);

// Option id by name
int cfgDefOptionId(const char *optionName);

// Is the option for internal use only?
bool cfgDefOptionInternal(ConfigCommand commandId, ConfigOption optionId);

// Does the option accept multiple values?
bool cfgDefOptionMulti(ConfigOption optionId);

// Name of the option
const char *cfgDefOptionName(ConfigOption optionId);

// Is the option required
bool cfgDefOptionRequired(ConfigCommand commandId, ConfigOption optionId);

// Get option section
ConfigDefSection cfgDefOptionSection(ConfigOption optionId);

// Does the option need to be protected from showing up in logs, command lines, etc?
bool cfgDefOptionSecure(ConfigOption optionId);

// Option total
unsigned int cfgDefOptionTotal(void);

// Get option data type
int cfgDefOptionType(ConfigOption optionId);

// Is the option valid for the command?
bool cfgDefOptionValid(ConfigCommand commandId, ConfigOption optionId);

#endif
