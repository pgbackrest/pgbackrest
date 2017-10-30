# ----------------------------------------------------------------------------------------------------------------------------------
# Config Rule Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

I32
cfgCommandId(commandName)
    const char *commandName

I32
cfgOptionId(optionName)
    const char *optionName

bool
cfgRuleOptionAllowList(commandId, optionId)
    U32 commandId
    U32 optionId

const char *
cfgRuleOptionAllowListValue(commandId, optionId, valueId)
    U32 commandId
    U32 optionId
    U32 valueId

I32
cfgRuleOptionAllowListValueTotal(commandId, optionId)
    U32 commandId
    U32 optionId

bool
cfgRuleOptionAllowListValueValid(commandId, optionId, value);
    U32 commandId
    U32 optionId
    const char *value

bool
cfgRuleOptionAllowRange(commandId, optionId)
    U32 commandId
    U32 optionId

double
cfgRuleOptionAllowRangeMax(commandId, optionId)
    U32 commandId
    U32 optionId

double
cfgRuleOptionAllowRangeMin(commandId, optionId)
    U32 commandId
    U32 optionId

const char *
cfgRuleOptionDefault(commandId, optionId)
    U32 commandId
    U32 optionId

bool
cfgRuleOptionDepend(commandId, optionId);
    U32 commandId
    U32 optionId

I32
cfgRuleOptionDependOption(commandId, optionId)
    U32 commandId
    U32 optionId

const char *
cfgRuleOptionDependValue(commandId, optionId, valueId)
    U32 commandId
    U32 optionId
    U32 valueId

I32
cfgRuleOptionDependValueTotal(commandId, optionId)
    U32 commandId
    U32 optionId

bool
cfgRuleOptionDependValueValid(commandId, optionId, value)
    U32 commandId
    U32 optionId
    const char *value

const char *
cfgRuleOptionNameAlt(optionId)
    U32 optionId

bool cfgRuleOptionNegate(optionId)
    U32 optionId

const char *
cfgRuleOptionPrefix(optionId)
    U32 optionId

bool
cfgRuleOptionRequired(commandId, optionId)
    U32 commandId
    U32 optionId

const char *
cfgRuleOptionSection(optionId)
    U32 optionId

bool
cfgRuleOptionSecure(optionId)
    U32 optionId

I32
cfgRuleOptionType(optionId);
    U32 optionId

bool
cfgRuleOptionValid(commandId, optionId)
    U32 commandId
    U32 optionId

U32
cfgOptionTotal()
