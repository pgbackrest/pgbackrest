# ----------------------------------------------------------------------------------------------------------------------------------
# Page Checksum Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

U16
pgPageChecksum(page, blockNo)
    unsigned char *page
    U32 blockNo
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = pgPageChecksum(page, blockNo);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL
