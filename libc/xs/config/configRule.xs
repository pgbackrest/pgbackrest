# ----------------------------------------------------------------------------------------------------------------------------------
# Config Rule Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

I32
cfgCommandId(szCommandName)
    const char *szCommandName

I32
cfgOptionId(szOptionName)
    const char *szOptionName

bool
cfgRuleOptionAllowList(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

const char *
cfgRuleOptionAllowListValue(uiCommandId, uiOptionId, uiValueId)
    U32 uiCommandId
    U32 uiOptionId
    U32 uiValueId

I32
cfgRuleOptionAllowListValueTotal(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

bool
cfgRuleOptionAllowListValueValid(uiCommandId, uiOptionId, szValue);
    U32 uiCommandId
    U32 uiOptionId
    const char *szValue

bool
cfgRuleOptionAllowRange(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

double
cfgRuleOptionAllowRangeMax(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

double
cfgRuleOptionAllowRangeMin(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

const char *
cfgRuleOptionDefault(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

bool
cfgRuleOptionDepend(uiCommandId, uiOptionId);
    U32 uiCommandId
    U32 uiOptionId

I32
cfgRuleOptionDependOption(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

const char *
cfgRuleOptionDependValue(uiCommandId, uiOptionId, uiValueId)
    U32 uiCommandId
    U32 uiOptionId
    U32 uiValueId

I32
cfgRuleOptionDependValueTotal(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

bool
cfgRuleOptionDependValueValid(uiCommandId, uiOptionId, szValue)
    U32 uiCommandId
    U32 uiOptionId
    const char *szValue

const char *
cfgRuleOptionHint(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

const char *
cfgRuleOptionNameAlt(uiOptionId)
    U32 uiOptionId

bool cfgRuleOptionNegate(uiOptionId)
    U32 uiOptionId

const char *
cfgRuleOptionPrefix(uiOptionId)
    U32 uiOptionId

bool
cfgRuleOptionRequired(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

const char *
cfgRuleOptionSection(uiOptionId)
    U32 uiOptionId

bool
cfgRuleOptionSecure(uiOptionId)
    U32 uiOptionId

I32
cfgRuleOptionType(uiOptionId);
    U32 uiOptionId

bool
cfgRuleOptionValid(uiCommandId, uiOptionId)
    U32 uiCommandId
    U32 uiOptionId

U32
cfgOptionTotal()

bool
cfgRuleOptionValueHash(uiOptionId)
    U32 uiOptionId
