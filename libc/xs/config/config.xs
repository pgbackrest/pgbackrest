# ----------------------------------------------------------------------------------------------------------------------------------
# Config Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

const char *
cfgCommandName(uiCommandId)
    U32 uiCommandId

I32
cfgOptionIndexTotal(uiOptionId)
    U32 uiOptionId

const char *
cfgOptionName(uiOptionId)
    U32 uiOptionId
