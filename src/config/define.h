/***********************************************************************************************************************************
Command and Option Configuration Definition
***********************************************************************************************************************************/
#ifndef CONFIG_DEFINE_H
#define CONFIG_DEFINE_H

#include "common/type.h"
#include "config/define.auto.h"

/***********************************************************************************************************************************
Section enum - defines which sections of the config an option can appear in
***********************************************************************************************************************************/
typedef enum
{
    cfgDefSectionCommandLine,                                       // command-line only
    cfgDefSectionGlobal,                                            // command-line or in any config section
    cfgDefSectionStanza,                                            // command-line of in any config stanza section
} ConfigDefSection;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
int cfgDefCommandTotal();
void cfgDefCommandCheck(ConfigDefineCommand commandDefId);

bool cfgDefOptionAllowList(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
int cfgDefOptionAllowListValueTotal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
bool cfgDefOptionAllowListValueValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, const char *value);
const char *cfgDefOptionAllowListValue(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, int valueId);
bool cfgDefOptionAllowRange(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
double cfgDefOptionAllowRangeMax(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
double cfgDefOptionAllowRangeMin(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
void cfgDefOptionCheck(ConfigDefineOption optionDefId);
const char *cfgDefOptionDefault(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
bool cfgDefOptionDepend(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
ConfigDefineOption cfgDefOptionDependOption(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
int cfgDefOptionDependValueTotal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
bool cfgDefOptionDependValueValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, const char *value);
const char *cfgDefOptionDependValue(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, int valueId);
int cfgDefOptionIndexTotal(ConfigDefineOption optionDefId);
const char *cfgDefOptionName(ConfigDefineOption optionDefId);
const char *cfgDefOptionNameAlt(ConfigDefineOption optionDefId);
bool cfgDefOptionNegate(ConfigDefineOption optionDefId);
const char *cfgDefOptionPrefix(ConfigDefineOption optionDefId);
bool cfgDefOptionRequired(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
ConfigDefSection cfgDefOptionSection(ConfigDefineOption optionDefId);
bool cfgDefOptionSecure(ConfigDefineOption optionDefId);
int cfgDefOptionTotal();
int cfgDefOptionType(ConfigDefineOption optionDefId);
bool cfgDefOptionValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);

#endif
