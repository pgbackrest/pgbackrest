# ----------------------------------------------------------------------------------------------------------------------------------
# Random Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

####################################################################################################################################
SV *
randomBytes(size)
    I32 size
CODE:
    RETVAL = newSV(size);
    SvPOK_only(RETVAL);

    randomBytes((unsigned char *)SvPV_nolen(RETVAL), size);

    SvCUR_set(RETVAL, size);
OUTPUT:
    RETVAL
