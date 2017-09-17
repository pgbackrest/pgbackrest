# ----------------------------------------------------------------------------------------------------------------------------------
# Page Checksum Perl Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

U16
pageChecksum(page, blkno, pageSize)
    const char *page
    U32 blkno
    U32 pageSize

bool
pageChecksumTest(szPage, uiBlockNo, uiPageSize, uiIgnoreWalId, uiIgnoreWalOffset)
    const char *szPage
    U32 uiBlockNo
    U32 uiPageSize
    U32 uiIgnoreWalId
    U32 uiIgnoreWalOffset

bool
pageChecksumBufferTest(szPageBuffer, uiBufferSize, uiBlockNoStart, uiPageSize, uiIgnoreWalId, uiIgnoreWalOffset)
    const char *szPageBuffer
    U32 uiBufferSize
    U32 uiBlockNoStart
    U32 uiPageSize
    U32 uiIgnoreWalId
    U32 uiIgnoreWalOffset
