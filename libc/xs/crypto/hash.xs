####################################################################################################################################
# Cryptographic Hashes Perl Exports
#
# XS wrapper for functions in cipher/hash.c.
####################################################################################################################################

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

####################################################################################################################################
SV *
cryptoHashOne(type, message)
    const char *type
    SV *message
CODE:
    RETVAL = NULL;

    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
        STRLEN messageSize;
        const void *messagePtr = SvPV(message, messageSize);

        String *hash = bufHex(cryptoHashOne(strNew(type), BUF(messagePtr, messageSize)));

        RETVAL = newSV(strSize(hash));
        SvPOK_only(RETVAL);
        strcpy((char *)SvPV_nolen(RETVAL), strPtr(hash));
        SvCUR_set(RETVAL, strSize(hash));
    }
    MEM_CONTEXT_XS_TEMP_END();
OUTPUT:
    RETVAL
