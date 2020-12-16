/***********************************************************************************************************************************
Command and Option Configuration Definition
***********************************************************************************************************************************/
#ifndef CONFIG_DEFINE_H
#define CONFIG_DEFINE_H

#include <sys/types.h>

#include "common/type/string.h"
#include "config/config.auto.h"

/***********************************************************************************************************************************
Define global section name
***********************************************************************************************************************************/
#define CFGDEF_SECTION_GLOBAL                                       "global"
    STRING_DECLARE(CFGDEF_SECTION_GLOBAL_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
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

// Is the option required
bool cfgDefOptionRequired(ConfigCommand commandId, ConfigOption optionId);

#endif
