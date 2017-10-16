/***********************************************************************************************************************************
Test Page Checksums
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Page data for testing -- use 8192 for page size since this is the most common value
***********************************************************************************************************************************/
#define TEST_PAGE_SIZE                                              8192
#define TEST_PAGE_TOTAL                                             16

static unsigned char testPageBuffer[TEST_PAGE_TOTAL][TEST_PAGE_SIZE];

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("pageChecksum()"))
    {
        // Checksum for 0x00 fill,  page 0x00
        memset(testPageBuffer[0], 0, TEST_PAGE_SIZE);
        TEST_RESULT_U16_HEX(pageChecksum(testPageBuffer[0], 0, TEST_PAGE_SIZE), 0xC6AA, "check for 0x00 filled page, block 0");

        // Checksum for 0xFF fill, page 0x00
        memset(testPageBuffer[0], 0xFF, TEST_PAGE_SIZE);
        TEST_RESULT_U16_HEX(pageChecksum(testPageBuffer[0], 0, TEST_PAGE_SIZE), 0x0E1C, "check for 0xFF filled page, block 0");

        // Checksum for 0xFF fill, page 0xFF
        memset(testPageBuffer[0], 0xFF, TEST_PAGE_SIZE);
        TEST_RESULT_U16_HEX(pageChecksum(testPageBuffer[0], 999, TEST_PAGE_SIZE), 0x0EC3, "check for 0xFF filled page, block 999");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("pageChecksumTest()"))
    {
        // Zero the pages
        memset(testPageBuffer, 0, TEST_PAGE_TOTAL * TEST_PAGE_SIZE);

        // Pages with pd_upper = 0 should always return true no matter the block no
        TEST_RESULT_BOOL(pageChecksumTest(testPageBuffer[0], 0, TEST_PAGE_SIZE, 0, 0), true, "pd_upper is 0, block 0");
        TEST_RESULT_BOOL(pageChecksumTest(testPageBuffer[1], 999, TEST_PAGE_SIZE, 0, 0), true, "pd_upper is 0, block 999");

        // Update pd_upper and check for failure no matter the block no
        ((PageHeader)testPageBuffer[0])->pd_upper = 0x00FF;
        TEST_RESULT_BOOL(pageChecksumTest(testPageBuffer[0], 0, TEST_PAGE_SIZE, 0xFFFF, 0xFFFF), false, "badchecksum, page 0");
        TEST_RESULT_BOOL(
            pageChecksumTest(testPageBuffer[0], 9999, TEST_PAGE_SIZE, 0xFFFF, 0xFFFF), false, "badchecksum, page 9999");

        // Update LSNs and check that page checksums past the ignore limits are successful
        ((PageHeader)testPageBuffer[0])->pd_lsn.walid = 0x8888;
        ((PageHeader)testPageBuffer[0])->pd_lsn.xrecoff = 0x8888;

        TEST_RESULT_BOOL(
            pageChecksumTest(testPageBuffer[0], 0, TEST_PAGE_SIZE, 0x8888, 0x8888), true, "bad checksum past ignore limit");
        TEST_RESULT_BOOL(
            pageChecksumTest(testPageBuffer[0], 0, TEST_PAGE_SIZE, 0x8889, 0x8889), false, "bad checksum before ignore limit");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("pageChecksumBufferTest()"))
    {
        // Check that assertion fails if page buffer and page size are not divisible
        TEST_ERROR(
            pageChecksumBufferTest(testPageBuffer[0], TEST_PAGE_TOTAL * TEST_PAGE_SIZE - 1, 0, TEST_PAGE_SIZE, 0, 0),
            AssertError, "buffer size 131071, page size 8192 are not divisible");

        // Create pages that will pass the test (starting with block 0)
        for (int pageIdx = 0; pageIdx < TEST_PAGE_TOTAL; pageIdx++)
        {
            // Don't fill with zero because zeroes will succeed on the pd_upper check
            memset(testPageBuffer[pageIdx], 0x77, TEST_PAGE_SIZE);

            ((PageHeader)testPageBuffer[pageIdx])->pd_checksum = pageChecksum(testPageBuffer[pageIdx], pageIdx, TEST_PAGE_SIZE);
        }

        TEST_RESULT_BOOL(
            pageChecksumBufferTest(testPageBuffer[0], TEST_PAGE_TOTAL * TEST_PAGE_SIZE, 0, TEST_PAGE_SIZE, 0xFFFFFFFF, 0xFFFFFFFF),
            true, "valid page buffer starting at block 0");

        // Create pages that will pass the test (beginning with block <> 0)
        int blockBegin = 999;

        for (int pageIdx = 0; pageIdx < TEST_PAGE_TOTAL; pageIdx++)
        {
            ((PageHeader)testPageBuffer[pageIdx])->pd_checksum = pageChecksum(
                    testPageBuffer[pageIdx], pageIdx + blockBegin, TEST_PAGE_SIZE);
        }

        TEST_RESULT_BOOL(
            pageChecksumBufferTest(
                testPageBuffer[0], TEST_PAGE_TOTAL * TEST_PAGE_SIZE, blockBegin, TEST_PAGE_SIZE, 0xFFFFFFFF, 0xFFFFFFFF),
            true, "valid page buffer starting at block 999");

        // Break the checksum for a page and make sure it is found
        int pageInvalid = 7;
        assert(pageInvalid >= 0 && pageInvalid < TEST_PAGE_TOTAL);
        ((PageHeader)testPageBuffer[pageInvalid])->pd_checksum = 0xEEEE;

        TEST_RESULT_BOOL(
            pageChecksumBufferTest(
                testPageBuffer[0], TEST_PAGE_TOTAL * TEST_PAGE_SIZE, blockBegin, TEST_PAGE_SIZE, 0xFFFFFFFF, 0xFFFFFFFF),
            false, "invalid page buffer");
    }
}
