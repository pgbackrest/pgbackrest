# ----------------------------------------------------------------------------------------------------------------------------------
# Config Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

const char *
cfgCommandName(commandId)
    U32 commandId

I32
cfgOptionIndexTotal(optionId)
    U32 optionId

const char *
cfgOptionName(optionId)
    U32 optionId
