/***********************************************************************************************************************************
Test Page Checksums
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Page data for testing -- use 8192 for page size since this is the most common value
***********************************************************************************************************************************/
#define TEST_PAGE_TOTAL                                             16

// GCC doesn't like this elements of this array being used as both char * and struct * so wrap it in a function to disable the
// optimizations that cause warnings
unsigned char *
testPage(unsigned int pageIdx)
{
    static unsigned char testPageBuffer[TEST_PAGE_TOTAL][PG_PAGE_SIZE_DEFAULT];
    return testPageBuffer[pageIdx];
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("pgPageChecksum()"))
    {
        // Checksum for 0xFF fill, page 0x00
        memset(testPage(0), 0xFF, PG_PAGE_SIZE_DEFAULT);
        TEST_RESULT_U16_HEX(pgPageChecksum(testPage(0), 0), 0x0E1C, "check for 0xFF filled page, block 0");

        // Checksum for 0xFF fill, page 0xFF
        memset(testPage(0), 0xFF, PG_PAGE_SIZE_DEFAULT);
        TEST_RESULT_U16_HEX(pgPageChecksum(testPage(0), 999), 0x0EC3, "check for 0xFF filled page, block 999");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgPageChecksumTest() and pgPageLsn()"))
    {
        // Zero the pages
        memset(testPage(0), 0, TEST_PAGE_TOTAL * PG_PAGE_SIZE_DEFAULT);

        for (unsigned int pageIdx = 0; pageIdx < TEST_PAGE_TOTAL; pageIdx++)
            *(PageHeader)testPage(pageIdx) = (PageHeaderData){.pd_upper = 0x0};

        // Pages with pd_upper = 0 should always return true no matter the block no
        TEST_RESULT_BOOL(pgPageChecksumTest(testPage(0), 0, PG_PAGE_SIZE_DEFAULT, 0, 0), true, "pd_upper is 0, block 0");
        TEST_RESULT_BOOL(pgPageChecksumTest(testPage(1), 999, PG_PAGE_SIZE_DEFAULT, 0, 0), true, "pd_upper is 0, block 999");

        // Partial pages are always invalid
        *(PageHeader)testPage(0) = (PageHeaderData){.pd_upper = 0x00FF};
        ((PageHeader)testPage(0))->pd_checksum = pgPageChecksum(testPage(0), 0);
        TEST_RESULT_BOOL(pgPageChecksumTest(testPage(0), 0, PG_PAGE_SIZE_DEFAULT, 1, 1), true, "valid page");
        TEST_RESULT_BOOL(pgPageChecksumTest(testPage(0), 0, PG_PAGE_SIZE_DEFAULT / 2, 1, 1), false, "invalid partial page");

        // Update pd_upper and check for failure no matter the block no
        *(PageHeader)testPage(0) = (PageHeaderData){.pd_upper = 0x00FF, .pd_checksum = 0};
        TEST_RESULT_BOOL(pgPageChecksumTest(testPage(0), 0, PG_PAGE_SIZE_DEFAULT, 0xFFFF, 0xFFFF), false, "badchecksum, page 0");
        TEST_RESULT_BOOL(
            pgPageChecksumTest(testPage(0), 9999, PG_PAGE_SIZE_DEFAULT, 0xFFFF, 0xFFFF), false, "badchecksum, page 9999");

        // Update LSNs and check that page checksums past the ignore limits are successful
        ((PageHeader)testPage(0))->pd_lsn.xlogid = 0x8888;
        ((PageHeader)testPage(0))->pd_lsn.xrecoff = 0x8888;

        TEST_RESULT_UINT(pgPageLsn(testPage(0)), 0x0000888800008888, "check page lsn");

        TEST_RESULT_BOOL(
            pgPageChecksumTest(testPage(0), 0, PG_PAGE_SIZE_DEFAULT, 0x8888, 0x8888), true, "bad checksum past ignore limit");
        TEST_RESULT_BOOL(
            pgPageChecksumTest(testPage(0), 0, PG_PAGE_SIZE_DEFAULT, 0x8888, 0x8889), false, "bad checksum before ignore limit");
        TEST_RESULT_BOOL(
            pgPageChecksumTest(testPage(0), 0, PG_PAGE_SIZE_DEFAULT, 0x8889, 0x8889), false, "bad checksum before ignore limit");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
