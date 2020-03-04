# ----------------------------------------------------------------------------------------------------------------------------------
# Page Checksum Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

U16
pgPageChecksum(page, blockNo, pageSize)
    const char *page
    U32 blockNo
    U32 pageSize
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = pgPageChecksum((const unsigned char *)page, blockNo, pageSize);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL
