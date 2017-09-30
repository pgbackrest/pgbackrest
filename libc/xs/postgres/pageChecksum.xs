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
    RETVAL = pageChecksum(
        (const unsigned char *)page, blockNo, pageSize);
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
    RETVAL = pageChecksumTest(
        (const unsigned char *)page, blockNo, pageSize, ignoreWalId, ignoreWalOffset);
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
    RETVAL = pageChecksumBufferTest(
        (const unsigned char *)pageBuffer, pageBufferSize, blockNoBegin, pageSize, ignoreWalId, ignoreWalOffset);
OUTPUT:
    RETVAL
