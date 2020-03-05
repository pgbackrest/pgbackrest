# ----------------------------------------------------------------------------------------------------------------------------------
# Page Checksum Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

U16
pgPageChecksum(page, blockNo)
    const char *page
    U32 blockNo
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = pgPageChecksum((const unsigned char *)page, blockNo);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL
