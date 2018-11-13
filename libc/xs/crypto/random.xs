# ----------------------------------------------------------------------------------------------------------------------------------
# Random Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

####################################################################################################################################
SV *
cryptoRandomBytes(size)
    I32 size
CODE:
    RETVAL = newSV(size);
    SvPOK_only(RETVAL);

    cryptoRandomBytes((unsigned char *)SvPV_nolen(RETVAL), size);

    SvCUR_set(RETVAL, size);
OUTPUT:
    RETVAL
