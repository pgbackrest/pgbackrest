#ifndef CONFIG_RULE_H
#define CONFIG_RULE_H

#include "common/type.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
int32 cfgCommandId(const char *szCommandName);
int32 cfgOptionId(const char *szOptionName);
bool cfgRuleOptionAllowListValueValid(uint32 uiCommandId, uint32 uiOptionId, const char *szValue);
bool cfgRuleOptionDependValueValid(uint32 uiCommandId, uint32 uiOptionId, const char *szValue);
uint32 cfgOptionTotal();

/***********************************************************************************************************************************
Auto-Generated Functions
***********************************************************************************************************************************/
bool cfgRuleOptionAllowList(uint32 uiCommandId, uint32 uiOptionId);
const char * cfgRuleOptionAllowListValue(uint32 uiCommandId, uint32 uiOptionId, uint32 uiValueId);
int32 cfgRuleOptionAllowListValueTotal(uint32 uiCommandId, uint32 uiOptionId);
bool cfgRuleOptionAllowRange(uint32 uiCommandId, uint32 uiOptionId);
double cfgRuleOptionAllowRangeMax(uint32 uiCommandId, uint32 uiOptionId);
double cfgRuleOptionAllowRangeMin(uint32 uiCommandId, uint32 uiOptionId);
const char * cfgRuleOptionDefault(uint32 uiCommandId, uint32 uiOptionId);
bool cfgRuleOptionDepend(uint32 uiCommandId, uint32 uiOptionId);
int32 cfgRuleOptionDependOption(uint32 uiCommandId, uint32 uiOptionId);
const char *cfgRuleOptionDependValue(uint32 uiCommandId, uint32 uiOptionId, uint32 uiValueId);
int32 cfgRuleOptionDependValueTotal(uint32 uiCommandId, uint32 uiOptionId);
const char *cfgRuleOptionHint(uint32 uiCommandId, uint32 uiOptionId);
const char *cfgRuleOptionNameAlt(uint32 uiOptionId);
bool cfgRuleOptionNegate(uint32 uiOptionId);
const char *cfgRuleOptionPrefix(uint32 uiOptionId);
bool cfgRuleOptionRequired(uint32 uiCommandId, uint32 uiOptionId);
const char *cfgRuleOptionSection(uint32 uiOptionId);
bool cfgRuleOptionSecure(uint32 uiOptionId);
int32 cfgRuleOptionType(uint32 uiOptionId);
bool cfgRuleOptionValid(uint32 uiCommandId, uint32 uiOptionId);
bool cfgRuleOptionValueHash(uint32 uiOptionId);

#endif
