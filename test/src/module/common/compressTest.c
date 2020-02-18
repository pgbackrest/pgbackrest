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
    if (testBegin("compressInit(), compressFilterAdd(), and compressExt*()"))
    {
        TEST_TITLE("no compression");
        {
            TEST_RESULT_VOID(compressInit(false, NULL, 0), "init");
            TEST_RESULT_UINT(compressHelperLocal.type, compressTypeNone, "    check type");
            TEST_RESULT_Z(compressExtZ(), "", "    check ext");

            IoFilterGroup *filterGroup = ioFilterGroupNew();
            TEST_RESULT_VOID(compressFilterAdd(filterGroup), "try to add compress filter");
            TEST_RESULT_UINT(lstSize(filterGroup->filterList), 0, "   check no filter was added");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("gzip compression");
        {
            TEST_RESULT_VOID(compressInit(true, strNew(GZIP_EXT), 7), "init");
            TEST_RESULT_UINT(compressHelperLocal.type, compressTypeGzip, "    check type");
            TEST_RESULT_UINT(compressHelperLocal.level, 7, "    check level");
            TEST_RESULT_Z(compressExtZ(), ".gz", "    check ext");

            IoFilterGroup *filterGroup = ioFilterGroupNew();
            TEST_RESULT_VOID(compressFilterAdd(filterGroup), "try to add compress filter");
            TEST_RESULT_UINT(lstSize(filterGroup->filterList), 1, "   check filter was added");
            TEST_RESULT_STR(
                ioFilterType(ioFilterGroupGet(filterGroup, 0)->filter), GZIP_COMPRESS_FILTER_TYPE_STR, "   check filter type");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cat extension");
        {
            String *file = strNew("file");
            TEST_RESULT_VOID(compressExtCat(file), "cat gzip ext");
            TEST_RESULT_STR_Z(file, "file.gz", "    check gzip ext");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("lz4 compression");
        {
#ifdef HAVE_LIBLZ4
            TEST_RESULT_VOID(compressInit(true, strNew(LZ4_EXT), 2), "init");
            TEST_RESULT_UINT(compressHelperLocal.type, compressTypeLz4, "    check type");
            TEST_RESULT_UINT(compressHelperLocal.level, 2, "    check level");
            TEST_RESULT_Z(compressExtZ(), ".lz4", "    check ext");

            IoFilterGroup *filterGroup = ioFilterGroupNew();
            TEST_RESULT_VOID(compressFilterAdd(filterGroup), "try to add compress filter");
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
            TEST_ERROR(compressInit(true, strNew(BOGUS_STR), 0), AssertError, "invalid compression type 'BOGUS'");

            compressHelperLocal.type = (CompressType)999999;
            TEST_ERROR(compressExtZ(), AssertError, "invalid compression type 999999");

            IoFilterGroup *filterGroup = ioFilterGroupNew();
            TEST_ERROR(compressFilterAdd(filterGroup), AssertError, "invalid compression type 999999");
        }
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
