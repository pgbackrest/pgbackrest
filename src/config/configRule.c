/***********************************************************************************************************************************
Command and Option Rules
***********************************************************************************************************************************/
#include "config.h"
#include "configRule.h"

#include "configRule.auto.c"

/***********************************************************************************************************************************
cfgCommandTotal - total number of commands
***********************************************************************************************************************************/
uint32
cfgCommandTotal()
{
    return CFGCMDDEF_TOTAL;
}

/***********************************************************************************************************************************
cfgOptionTotal - total number of configuration options
***********************************************************************************************************************************/
uint32
cfgOptionTotal()
{
    return CFGOPTDEF_TOTAL;
}

/***********************************************************************************************************************************
cfgRuleOptionAllowListValueValid - check if the value matches a value in the allow list
***********************************************************************************************************************************/
bool
cfgRuleOptionAllowListValueValid(uint32 uiCommandId, uint32 uiOptionId, const char *szValue)
{
    if (szValue != NULL)
    {
        for (uint32 uiIndex = 0; uiIndex < cfgRuleOptionAllowListValueTotal(uiCommandId, uiOptionId); uiIndex++)
            if (strcmp(szValue, cfgRuleOptionAllowListValue(uiCommandId, uiOptionId, uiIndex)) == 0)
                return true;
    }

    return false;
}

/***********************************************************************************************************************************
cfgRuleOptionDependValueValid - check if the value matches a value in the allow list
***********************************************************************************************************************************/
bool
cfgRuleOptionDependValueValid(uint32 uiCommandId, uint32 uiOptionId, const char *szValue)
{
    if (szValue != NULL)
    {
        for (uint32 uiIndex = 0; uiIndex < cfgRuleOptionDependValueTotal(uiCommandId, uiOptionId); uiIndex++)
            if (strcmp(szValue, cfgRuleOptionDependValue(uiCommandId, uiOptionId, uiIndex)) == 0)
                return true;
    }

    return false;
}
