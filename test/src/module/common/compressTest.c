/***********************************************************************************************************************************
Test Compression
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("compress*()"))
    {
        TEST_TITLE("no compression");
        {
            TEST_RESULT_UINT(compressTypeEnum(NULL), compressTypeNone, "check enum");
            TEST_RESULT_Z(compressExtZ(compressTypeNone), "", "check ext");

            IoFilterGroup *filterGroup = ioFilterGroupNew();
            TEST_RESULT_VOID(compressFilterAdd(filterGroup, compressTypeNone, 0), "try to add compress filter");
            TEST_RESULT_UINT(lstSize(filterGroup->filterList), 0, "   check no filter was added");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("gzip compression");
        {
            TEST_RESULT_UINT(compressTypeEnum(STRDEF("gz")), compressTypeGzip, "check enum");
            TEST_RESULT_Z(compressExtZ(compressTypeGzip), ".gz", "check ext");

            IoFilterGroup *filterGroup = ioFilterGroupNew();
            TEST_RESULT_VOID(compressFilterAdd(filterGroup, compressTypeGzip, 6), "try to add compress filter");
            TEST_RESULT_UINT(lstSize(filterGroup->filterList), 1, "   check filter was added");
            TEST_RESULT_STR(
                ioFilterType(ioFilterGroupGet(filterGroup, 0)->filter), GZIP_COMPRESS_FILTER_TYPE_STR, "   check filter type");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cat extension");
        {
            String *file = strNew("file");
            TEST_RESULT_VOID(compressExtCat(file, compressTypeGzip), "cat gzip ext");
            TEST_RESULT_STR_Z(file, "file.gz", "    check gzip ext");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("lz4 compression");
        {
#ifdef HAVE_LIBLZ4
            TEST_RESULT_UINT(compressTypeEnum(STRDEF("lz4")), compressTypeLz4, "check enum");
            TEST_RESULT_Z(compressExtZ(compressTypeLz4), ".lz4", "check ext");

            IoFilterGroup *filterGroup = ioFilterGroupNew();
            TEST_RESULT_VOID(compressFilterAdd(filterGroup, compressTypeLz4, 1), "try to add compress filter");
            TEST_RESULT_UINT(lstSize(filterGroup->filterList), 1, "   check filter was added");
            TEST_RESULT_STR(
                ioFilterType(ioFilterGroupGet(filterGroup, 0)->filter), LZ4_COMPRESS_FILTER_TYPE_STR, "   check filter type");
#else
            TEST_ERROR(compressInit(true, strNew(LZ4_EXT), 2), OptionInvalidValueError, "pgBackRest not compiled with lz4 support");
#endif
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bogus compression");
        {
            TEST_ERROR(compressTypeEnum(strNew(BOGUS_STR)), AssertError, "invalid compression type 'BOGUS'");

            TEST_ERROR(compressExtZ((CompressType)999999), AssertError, "invalid compression type 999999");

            IoFilterGroup *filterGroup = ioFilterGroupNew();
            TEST_ERROR(compressFilterAdd(filterGroup, (CompressType)999999, 0), AssertError, "invalid compression type 999999");
        }
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
