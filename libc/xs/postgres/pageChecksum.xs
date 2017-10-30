# ----------------------------------------------------------------------------------------------------------------------------------
# Page Checksum Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

U16
pageChecksum(page, blockNo, pageSize)
    const char *page
    U32 blockNo
    U32 pageSize
CODE:
    RETVAL = 0;

    ERROR_XS_BEGIN()
    {
        RETVAL = pageChecksum(
            (const unsigned char *)page, blockNo, pageSize);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

bool
pageChecksumTest(page, blockNo, pageSize, ignoreWalId, ignoreWalOffset)
    const char *page
    U32 blockNo
    U32 pageSize
    U32 ignoreWalId
    U32 ignoreWalOffset
CODE:
    RETVAL = false;

    ERROR_XS_BEGIN()
    {
        RETVAL = pageChecksumTest(
            (const unsigned char *)page, blockNo, pageSize, ignoreWalId, ignoreWalOffset);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL

bool
pageChecksumBufferTest(pageBuffer, pageBufferSize, blockNoBegin, pageSize, ignoreWalId, ignoreWalOffset)
    const char *pageBuffer
    U32 pageBufferSize
    U32 blockNoBegin
    U32 pageSize
    U32 ignoreWalId
    U32 ignoreWalOffset
CODE:
    RETVAL = false;

    ERROR_XS_BEGIN()
    {
        RETVAL = pageChecksumBufferTest(
            (const unsigned char *)pageBuffer, pageBufferSize, blockNoBegin, pageSize, ignoreWalId, ignoreWalOffset);
    }
    ERROR_XS_END();
OUTPUT:
    RETVAL
