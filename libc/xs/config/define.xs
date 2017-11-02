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
        RETVAL = cfgCommandId(commandName);
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

bool
cfgDefOptionAllowList(commandId, optionId)
    U32 commandId
    U32 optionId
CODE:
    RETVAL = false;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionAllowList(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

const char *
cfgDefOptionAllowListValue(commandId, optionId, valueId)
    U32 commandId
    U32 optionId
    U32 valueId
CODE:
    RETVAL = NULL;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionAllowListValue(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId), valueId);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

I32
cfgDefOptionAllowListValueTotal(commandId, optionId)
    U32 commandId
    U32 optionId
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionAllowListValueTotal(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

bool
cfgDefOptionAllowListValueValid(commandId, optionId, value);
    U32 commandId
    U32 optionId
    const char *value
CODE:
    RETVAL = false;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionAllowListValueValid(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId), value);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

bool
cfgDefOptionAllowRange(commandId, optionId)
    U32 commandId
    U32 optionId
CODE:
    RETVAL = false;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionAllowRange(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

double
cfgDefOptionAllowRangeMax(commandId, optionId)
    U32 commandId
    U32 optionId
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionAllowRangeMax(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

double
cfgDefOptionAllowRangeMin(commandId, optionId)
    U32 commandId
    U32 optionId
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionAllowRangeMin(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId));
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

bool
cfgDefOptionDepend(commandId, optionId);
    U32 commandId
    U32 optionId
CODE:
    RETVAL = false;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionDepend(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

I32
cfgDefOptionDependOption(commandId, optionId)
    U32 commandId
    U32 optionId
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgOptionIdFromDefId(
            cfgDefOptionDependOption(
                cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId)), cfgOptionIndex(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

const char *
cfgDefOptionDependValue(commandId, optionId, valueId)
    U32 commandId
    U32 optionId
    U32 valueId
CODE:
    RETVAL = NULL;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionDependValue(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId), valueId);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

I32
cfgDefOptionDependValueTotal(commandId, optionId)
    U32 commandId
    U32 optionId
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionDependValueTotal(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

bool
cfgDefOptionDependValueValid(commandId, optionId, value)
    U32 commandId
    U32 optionId
    const char *value
CODE:
    RETVAL = false;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionDependValueValid(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId), value);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

const char *
cfgDefOptionNameAlt(optionId)
    U32 optionId
CODE:
    RETVAL = NULL;

    ERROR_XS_BEGIN()
    {
        if (cfgOptionIndexTotal(optionId) > 1 && cfgOptionIndex(optionId) == 0)
            RETVAL = cfgDefOptionName(cfgOptionDefIdFromId(optionId));
        else
            RETVAL = cfgDefOptionNameAlt(cfgOptionDefIdFromId(optionId));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

bool cfgDefOptionNegate(optionId)
    U32 optionId
CODE:
    RETVAL = false;

    ERROR_XS_BEGIN()
    {
        RETVAL = cfgDefOptionNegate(cfgOptionDefIdFromId(optionId));
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
cfgDefOptionRequired(commandId, optionId)
    U32 commandId
    U32 optionId
CODE:
    RETVAL = false;

    ERROR_XS_BEGIN()
    {
        // Only the first indexed option is ever required
        if (cfgOptionIndex(optionId) == 0)
        {
            RETVAL = cfgDefOptionRequired(cfgCommandDefIdFromId(commandId), cfgOptionDefIdFromId(optionId));
        }
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

const char *
cfgDefOptionSection(optionId)
    U32 optionId
CODE:
    RETVAL = NULL;

    ERROR_XS_BEGIN()
    {
        switch (cfgDefOptionSection(cfgOptionDefIdFromId(optionId)))
        {
            case cfgDefSectionGlobal:
                RETVAL = "global";
                break;

            case cfgDefSectionStanza:
                RETVAL = "stanza";
                break;

            default:
                RETVAL = NULL;
                break;
        }
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
