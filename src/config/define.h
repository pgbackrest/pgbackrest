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

#include "config/config.h"
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
const char *cfgDefCommandHelpDescription(ConfigCommand commandId);
const char *cfgDefCommandHelpSummary(ConfigCommand commandId);

// Option allow lists
bool cfgDefOptionAllowList(ConfigCommand commandId, ConfigDefineOption optionDefId);
bool cfgDefOptionAllowListValueValid(ConfigCommand commandId, ConfigDefineOption optionDefId, const char *value);

// Allow range
bool cfgDefOptionAllowRange(ConfigCommand commandId, ConfigDefineOption optionDefId);
double cfgDefOptionAllowRangeMax(ConfigCommand commandId, ConfigDefineOption optionDefId);
double cfgDefOptionAllowRangeMin(ConfigCommand commandId, ConfigDefineOption optionDefId);

// Default value for the option
const char *cfgDefOptionDefault(ConfigCommand commandId, ConfigDefineOption optionDefId);

// Dependencies and depend lists
bool cfgDefOptionDepend(ConfigCommand commandId, ConfigDefineOption optionDefId);
ConfigDefineOption cfgDefOptionDependOption(ConfigCommand commandId, ConfigDefineOption optionDefId);
unsigned int cfgDefOptionDependValueTotal(ConfigCommand commandId, ConfigDefineOption optionDefId);
bool cfgDefOptionDependValueValid(ConfigCommand commandId, ConfigDefineOption optionDefId, const char *value);
const char *cfgDefOptionDependValue(ConfigCommand commandId, ConfigDefineOption optionDefId, unsigned int valueId);

// Option help
const char *cfgDefOptionHelpDescription(ConfigCommand commandId, ConfigDefineOption optionDefId);
const char *cfgDefOptionHelpSummary(ConfigCommand commandId, ConfigDefineOption optionDefId);

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
bool cfgDefOptionInternal(ConfigCommand commandId, ConfigDefineOption optionDefId);

// Does the option accept multiple values?
bool cfgDefOptionMulti(ConfigDefineOption optionDefId);

// Name of the option
const char *cfgDefOptionName(ConfigDefineOption optionDefId);

// Is the option required
bool cfgDefOptionRequired(ConfigCommand commandId, ConfigDefineOption optionDefId);

// Get option section
ConfigDefSection cfgDefOptionSection(ConfigDefineOption optionDefId);

// Does the option need to be protected from showing up in logs, command lines, etc?
bool cfgDefOptionSecure(ConfigDefineOption optionDefId);

// Option total
unsigned int cfgDefOptionTotal(void);

// Get option data type
int cfgDefOptionType(ConfigDefineOption optionDefId);

// Is the option valid for the command?
bool cfgDefOptionValid(ConfigCommand commandId, ConfigDefineOption optionDefId);

#endif
