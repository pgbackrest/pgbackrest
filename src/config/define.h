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
    STRING_DECLARE(CFGDEF_SECTION_GLOBAL_STR)

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
unsigned int cfgDefCommandTotal(void);
void cfgDefCommandCheck(ConfigDefineCommand commandDefId);
const char *cfgDefCommandHelpDescription(ConfigDefineCommand commandDefId);
const char *cfgDefCommandHelpSummary(ConfigDefineCommand commandDefId);

bool cfgDefOptionAllowList(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
unsigned int cfgDefOptionAllowListValueTotal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
bool cfgDefOptionAllowListValueValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, const char *value);
const char *cfgDefOptionAllowListValue(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, unsigned int valueId);
bool cfgDefOptionAllowRange(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
double cfgDefOptionAllowRangeMax(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
double cfgDefOptionAllowRangeMin(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
void cfgDefOptionCheck(ConfigDefineOption optionDefId);
const char *cfgDefOptionDefault(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
bool cfgDefOptionDepend(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
ConfigDefineOption cfgDefOptionDependOption(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
unsigned int cfgDefOptionDependValueTotal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
bool cfgDefOptionDependValueValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, const char *value);
const char *cfgDefOptionDependValue(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId, unsigned int valueId);
const char *cfgDefOptionHelpDescription(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
bool cfgDefOptionHelpNameAlt(ConfigDefineOption optionDefId);
const char *cfgDefOptionHelpNameAltValue(ConfigDefineOption optionDefId, unsigned int valueId);
unsigned int cfgDefOptionHelpNameAltValueTotal(ConfigDefineOption optionDefId);
const char *cfgDefOptionHelpSection(ConfigDefineOption optionDefId);
const char *cfgDefOptionHelpSummary(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
int cfgDefOptionId(const char *optionName);
unsigned int cfgDefOptionIndexTotal(ConfigDefineOption optionDefId);
bool cfgDefOptionInternal(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
bool cfgDefOptionMulti(ConfigDefineOption optionDefId);
const char *cfgDefOptionName(ConfigDefineOption optionDefId);
const char *cfgDefOptionPrefix(ConfigDefineOption optionDefId);
bool cfgDefOptionRequired(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);
ConfigDefSection cfgDefOptionSection(ConfigDefineOption optionDefId);
bool cfgDefOptionSecure(ConfigDefineOption optionDefId);
unsigned int cfgDefOptionTotal(void);
int cfgDefOptionType(ConfigDefineOption optionDefId);
bool cfgDefOptionValid(ConfigDefineCommand commandDefId, ConfigDefineOption optionDefId);

#endif
