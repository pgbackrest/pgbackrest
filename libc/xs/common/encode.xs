# ----------------------------------------------------------------------------------------------------------------------------------
# Binary to String Encode/Decode Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

####################################################################################################################################
SV *
encodeToStr(encodeType, source)
    int encodeType
    SV *source
CODE:
    RETVAL = NULL;

    STRLEN sourceSize;
    unsigned char *sourcePtr = (unsigned char *)SvPV(source, sourceSize);

    ERROR_XS_BEGIN()
    {
        RETVAL = newSV(encodeToStrSize(encodeType, sourceSize));
        SvPOK_only(RETVAL);

        encodeToStr(encodeType, sourcePtr, sourceSize, (char *)SvPV_nolen(RETVAL));
        SvCUR_set(RETVAL, encodeToStrSize(encodeType, sourceSize));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

####################################################################################################################################
SV *
decodeToBin(encodeType, source)
    int encodeType
    const char *source
CODE:
    RETVAL = NULL;

    ERROR_XS_BEGIN()
    {
        RETVAL = newSV(decodeToBinSize(encodeType, source));
        SvPOK_only(RETVAL);

        decodeToBin(encodeType, source, (unsigned char *)SvPV_nolen(RETVAL));
        SvCUR_set(RETVAL, decodeToBinSize(encodeType, source));
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL
