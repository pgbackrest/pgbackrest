# ----------------------------------------------------------------------------------------------------------------------------------
# Config Test Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

####################################################################################################################################
# Parse command line and return a JSON object with results
####################################################################################################################################
SV *
cfgParseTest(backrestBin, parseParam)
    const char *backrestBin
    const char *parseParam
CODE:
    RETVAL = NULL;

    ERROR_XS_BEGIN()
    {
        // This should run in a temp context but for some reason getopt_long gets upset when if gets called again after the previous
        // arg list being freed.  So, this is a memory leak but it is only used for testing, not production.
        StringList *paramList = strLstNewSplitZ(strCat(strNew("pgbackrest|"), parseParam), "|");

        // Don't use cfgLoad() because it has a lot of side effects that we don't want
        configParse(strLstSize(paramList), strLstPtr(paramList));
        cfgExeSet(strNew(backrestBin));
        cfgLoadUpdateOption();

        String *result = perlOptionJson();

        RETVAL = newSV(strSize(result));
        SvPOK_only(RETVAL);

        strcpy(SvPV_nolen(RETVAL), strPtr(result));
        SvCUR_set(RETVAL, strSize(result));
    }
    ERROR_XS_END()
OUTPUT:
    RETVAL
