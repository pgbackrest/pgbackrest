/***********************************************************************************************************************************
Command and Option Configuration Definition
***********************************************************************************************************************************/
#ifndef CONFIG_DEFINE_H
#define CONFIG_DEFINE_H

#include "common/typec.h"
#include "config/define.auto.h"

/***********************************************************************************************************************************
Section enum - defines which sections of the config an option can appear in
***********************************************************************************************************************************/
typedef enum
{
    cfgDefSectionCommandLine,                                       // command-line only
    cfgDefSectionGlobal,                                            // command-line or in any config section
    cfgDefSectionStanza,                                            // command-line or in any config stanza section
} ConfigDefSection;

/***********************************************************************************************************************************
Define global section name
***********************************************************************************************************************************/
#define CFGDEF_SECTION_GLOBAL                                       "global"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
unsigned int cfgDefCommandTotal();
void cfgDefCommandCheck(ConfigDefineCommand commandDefId);
const char *cfgDefCommandHelpDescription(ConfigDefineCommand commandDefId);
const char *cfgDefCommandHelpSummary(ConfigDefineCommand commandDefId);

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
const char *cfgDefOptionHelpDescription(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
bool cfgDefOptionHelpNameAlt(ConfigDefineOption optionDefId);
const char *cfgDefOptionHelpNameAltValue(ConfigDefineOption optionDefId, int valueId);
int cfgDefOptionHelpNameAltValueTotal(ConfigDefineOption optionDefId);
const char *cfgDefOptionHelpSection(ConfigDefineOption optionDefId);
const char *cfgDefOptionHelpSummary(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
int cfgDefOptionId(const char *optionName);
unsigned int cfgDefOptionIndexTotal(ConfigDefineOption optionDefId);
bool cfgDefOptionInternal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
const char *cfgDefOptionName(ConfigDefineOption optionDefId);
const char *cfgDefOptionPrefix(ConfigDefineOption optionDefId);
bool cfgDefOptionRequired(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
ConfigDefSection cfgDefOptionSection(ConfigDefineOption optionDefId);
bool cfgDefOptionSecure(ConfigDefineOption optionDefId);
unsigned int cfgDefOptionTotal();
int cfgDefOptionType(ConfigDefineOption optionDefId);
bool cfgDefOptionValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);

#endif
