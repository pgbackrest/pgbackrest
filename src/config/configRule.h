/***********************************************************************************************************************************
Command and Option Configuration Rules
***********************************************************************************************************************************/
#ifndef CONFIG_RULE_H
#define CONFIG_RULE_H

#include "common/type.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
int cfgCommandId(const char *commandName);
int cfgOptionId(const char *optionName);
bool cfgRuleOptionAllowListValueValid(int commandId, int optionId, const char *value);
bool cfgRuleOptionDependValueValid(int commandId, int optionId, const char *value);
int cfgOptionTotal();

/***********************************************************************************************************************************
Auto-Generated Functions
***********************************************************************************************************************************/
bool cfgRuleOptionAllowList(int commandId, int optionId);
const char * cfgRuleOptionAllowListValue(int commandId, int optionId, int valueId);
int cfgRuleOptionAllowListValueTotal(int commandId, int optionId);
bool cfgRuleOptionAllowRange(int commandId, int optionId);
double cfgRuleOptionAllowRangeMax(int commandId, int optionId);
double cfgRuleOptionAllowRangeMin(int commandId, int optionId);
const char * cfgRuleOptionDefault(int commandId, int optionId);
bool cfgRuleOptionDepend(int commandId, int optionId);
int cfgRuleOptionDependOption(int commandId, int optionId);
const char *cfgRuleOptionDependValue(int commandId, int optionId, int valueId);
int cfgRuleOptionDependValueTotal(int commandId, int optionId);
const char *cfgRuleOptionHint(int commandId, int optionId);
const char *cfgRuleOptionNameAlt(int optionId);
bool cfgRuleOptionNegate(int optionId);
const char *cfgRuleOptionPrefix(int optionId);
bool cfgRuleOptionRequired(int commandId, int optionId);
const char *cfgRuleOptionSection(int optionId);
bool cfgRuleOptionSecure(int optionId);
int cfgRuleOptionType(int optionId);
bool cfgRuleOptionValid(int commandId, int optionId);

#endif
