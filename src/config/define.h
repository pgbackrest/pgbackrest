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

#include "config/define.auto.h"
#include "common/type/string.h"

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
const char *cfgDefCommandHelpDescription(ConfigDefineCommand commandDefId);
const char *cfgDefCommandHelpSummary(ConfigDefineCommand commandDefId);

// Option allow lists
bool cfgDefOptionAllowList(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
bool cfgDefOptionAllowListValueValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, const char *value);

// Allow range
bool cfgDefOptionAllowRange(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
double cfgDefOptionAllowRangeMax(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
double cfgDefOptionAllowRangeMin(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);

// Default value for the option
const char *cfgDefOptionDefault(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);

// Dependencies and depend lists
bool cfgDefOptionDepend(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
ConfigDefineOption cfgDefOptionDependOption(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
unsigned int cfgDefOptionDependValueTotal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
bool cfgDefOptionDependValueValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, const char *value);
const char *cfgDefOptionDependValue(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, unsigned int valueId);

// Option help
const char *cfgDefOptionHelpDescription(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
const char *cfgDefOptionHelpSummary(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);

// Option help name alt
bool cfgDefOptionHelpNameAlt(ConfigDefineOption optionDefId);
const char *cfgDefOptionHelpNameAltValue(ConfigDefineOption optionDefId, unsigned int valueId);
unsigned int cfgDefOptionHelpNameAltValueTotal(ConfigDefineOption optionDefId);

// Option help section
const char *cfgDefOptionHelpSection(ConfigDefineOption optionDefId);

// Option id by name
int cfgDefOptionId(const char *optionName);

// Total indexed values for option
unsigned int cfgDefOptionIndexTotal(ConfigDefineOption optionDefId);

// Is the option for internal use only?
bool cfgDefOptionInternal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);

// Does the option accept multiple values?
bool cfgDefOptionMulti(ConfigDefineOption optionDefId);

// Name of the option
const char *cfgDefOptionName(ConfigDefineOption optionDefId);

// Option prefix for indexed options
const char *cfgDefOptionPrefix(ConfigDefineOption optionDefId);

// Is the option required
bool cfgDefOptionRequired(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);

// Get option section
ConfigDefSection cfgDefOptionSection(ConfigDefineOption optionDefId);

// Does the option need to be protected from showing up in logs, command lines, etc?
bool cfgDefOptionSecure(ConfigDefineOption optionDefId);

// Option total
unsigned int cfgDefOptionTotal(void);

// Get option data type
int cfgDefOptionType(ConfigDefineOption optionDefId);

// Is the option valid for the command?
bool cfgDefOptionValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);

#endif
