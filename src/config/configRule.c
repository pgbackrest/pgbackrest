/***********************************************************************************************************************************
Command and Option Configuration Rules
***********************************************************************************************************************************/
#include <string.h>

#include "config.h"
#include "configRule.h"

#include "configRule.auto.c"

/***********************************************************************************************************************************
cfgCommandTotal - total number of commands
***********************************************************************************************************************************/
int
cfgCommandTotal()
{
    return CFGCMDDEF_TOTAL;
}

/***********************************************************************************************************************************
cfgOptionTotal - total number of configuration options
***********************************************************************************************************************************/
int
cfgOptionTotal()
{
    return CFGOPTDEF_TOTAL;
}

/***********************************************************************************************************************************
cfgRuleOptionAllowListValueValid - check if the value matches a value in the allow list
***********************************************************************************************************************************/
bool
cfgRuleOptionAllowListValueValid(int commandId, int optionId, const char *value)
{
    if (value != NULL)
    {
        for (int valueIdx = 0; valueIdx < cfgRuleOptionAllowListValueTotal(commandId, optionId); valueIdx++)
            if (strcmp(value, cfgRuleOptionAllowListValue(commandId, optionId, valueIdx)) == 0)
                return true;
    }

    return false;
}

/***********************************************************************************************************************************
cfgRuleOptionDependValueValid - check if the value matches a value in the allow list
***********************************************************************************************************************************/
bool
cfgRuleOptionDependValueValid(int commandId, int optionId, const char *value)
{
    if (value != NULL)
    {
        for (int valueIdx = 0; valueIdx < cfgRuleOptionDependValueTotal(commandId, optionId); valueIdx++)
            if (strcmp(value, cfgRuleOptionDependValue(commandId, optionId, valueIdx)) == 0)
                return true;
    }

    return false;
}
