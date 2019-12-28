# ----------------------------------------------------------------------------------------------------------------------------------
# Config Definition Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

I32
cfgCommandId(commandName)
    const char *commandName
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgCommandId(commandName, true);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

I32
cfgOptionId(optionName)
    const char *optionName
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgOptionId(optionName);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

const char *
cfgDefOptionDefault(commandId, optionId)
    U32 commandId
    U32 optionId
CODE:
    RETVAL = NULL;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionDefault(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

const char *
cfgDefOptionPrefix(optionId)
    U32 optionId
CODE:
    RETVAL = NULL;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionPrefix(cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

bool
cfgDefOptionSecure(optionId)
    U32 optionId
CODE:
    RETVAL = false;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionSecure(cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

I32
cfgDefOptionType(optionId);
    U32 optionId
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionType(cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

bool
cfgDefOptionValid(commandId, optionId)
    U32 commandId
    U32 optionId
CODE:
    RETVAL = false;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionValid(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

U32
cfgOptionTotal()
CODE:
    RETVAL = CFG_OPTION_TOTAL;
OUTPUT:
    RETVAL
