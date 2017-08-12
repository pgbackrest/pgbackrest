#ifndef CONFIG_RULE_H
#define CONFIG_RULE_H

#include "common/type.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
int32 cfgCommandId(const char *szCommandName);
int32 cfgOptionId(const char *szOptionName);
bool cfgOptionRuleAllowListValueValid(uint32 uiCommandId, uint32 uiOptionId, const char *szValue);
bool cfgOptionRuleDependValueValid(uint32 uiCommandId, uint32 uiOptionId, const char *szValue);
uint32 cfgOptionTotal();

/***********************************************************************************************************************************
Auto-Generated Functions
***********************************************************************************************************************************/
bool cfgOptionRuleAllowList(uint32 uiCommandId, uint32 uiOptionId);
const char * cfgOptionRuleAllowListValue(uint32 uiCommandId, uint32 uiOptionId, uint32 uiValueId);
int32 cfgOptionRuleAllowListValueTotal(uint32 uiCommandId, uint32 uiOptionId);
bool cfgOptionRuleAllowRange(uint32 uiCommandId, uint32 uiOptionId);
double cfgOptionRuleAllowRangeMax(uint32 uiCommandId, uint32 uiOptionId);
double cfgOptionRuleAllowRangeMin(uint32 uiCommandId, uint32 uiOptionId);
const char * cfgOptionRuleDefault(uint32 uiCommandId, uint32 uiOptionId);
bool cfgOptionRuleDepend(uint32 uiCommandId, uint32 uiOptionId);
int32 cfgOptionRuleDependOption(uint32 uiCommandId, uint32 uiOptionId);
const char *cfgOptionRuleDependValue(uint32 uiCommandId, uint32 uiOptionId, uint32 uiValueId);
int32 cfgOptionRuleDependValueTotal(uint32 uiCommandId, uint32 uiOptionId);
const char *cfgOptionRuleHint(uint32 uiCommandId, uint32 uiOptionId);
const char *cfgOptionRuleNameAlt(uint32 uiOptionId);
bool cfgOptionRuleNegate(uint32 uiOptionId);
const char *cfgOptionRulePrefix(uint32 uiOptionId);
bool cfgOptionRuleRequired(uint32 uiCommandId, uint32 uiOptionId);
const char *cfgOptionRuleSection(uint32 uiOptionId);
bool cfgOptionRuleSecure(uint32 uiOptionId);
int32 cfgOptionRuleType(uint32 uiOptionId);
bool cfgOptionRuleValid(uint32 uiCommandId, uint32 uiOptionId);
bool cfgOptionRuleValueHash(uint32 uiOptionId);

#endif
