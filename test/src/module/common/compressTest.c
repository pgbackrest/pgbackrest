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
            TEST_RESULT_UINT(compressTypeEnum(STRDEF("none")), compressTypeNone, "check enum");
            TEST_RESULT_Z(compressExtZ(compressTypeNone), "", "check ext");
            TEST_RESULT_Z(compressTypeZ(compressTypeNone), "none", "check type z");
            TEST_RESULT_UINT(compressTypeFromName(STRDEF("file")), compressTypeNone, "check type from name");

            TEST_RESULT_PTR(compressFilter(compressTypeNone, 0), NULL, "no compress filter");
            TEST_RESULT_PTR(compressFilterVar(STRDEF("none"), NULL), NULL, "no filter var");
            TEST_RESULT_PTR(decompressFilter(compressTypeNone), NULL, "no decompress filter");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("gzip compression");
        {
            TEST_RESULT_UINT(compressTypeEnum(STRDEF("gz")), compressTypeGzip, "check enum");
            TEST_RESULT_Z(compressExtZ(compressTypeGzip), ".gz", "check ext");
            TEST_RESULT_Z(compressTypeZ(compressTypeGzip), "gz", "check type z");
            TEST_RESULT_UINT(compressTypeFromName(STRDEF("file.gz")), compressTypeGzip, "check type from name");

            IoFilter *filter = NULL;
            TEST_ASSIGN(filter, compressFilter(compressTypeGzip, 6), "compress filter");
            TEST_RESULT_PTR_NE(filter, NULL, "   check filter is not null");
            TEST_RESULT_STR(ioFilterType(filter), GZIP_COMPRESS_FILTER_TYPE_STR, "   check filter type");

            VariantList *paramList = varLstNew();
            varLstAdd(paramList, varNewInt(3));

            TEST_ASSIGN(filter, compressFilterVar(GZIP_COMPRESS_FILTER_TYPE_STR, paramList), "try to add compress filter var");
            TEST_RESULT_STR(ioFilterType(filter), GZIP_COMPRESS_FILTER_TYPE_STR, "   check filter type");

            TEST_ASSIGN(filter, decompressFilter(compressTypeGzip), "decompress filter");
            TEST_RESULT_PTR_NE(filter, NULL, "   check filter is not null");
            TEST_RESULT_STR(ioFilterType(filter), GZIP_DECOMPRESS_FILTER_TYPE_STR, "   check filter type");

            TEST_ASSIGN(filter, compressFilterVar(GZIP_DECOMPRESS_FILTER_TYPE_STR, NULL), "try to add decompress filter var");
            TEST_RESULT_STR(ioFilterType(filter), GZIP_DECOMPRESS_FILTER_TYPE_STR, "   check filter type");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cat extension");
        {
            String *file = strNew("file");
            TEST_RESULT_VOID(compressExtCat(file, compressTypeGzip), "cat gzip ext");
            TEST_RESULT_STR_Z(file, "file.gz", "    check gzip ext");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("strip extension");
        {
            TEST_ERROR(compressExtStrip(STRDEF("file"), compressTypeGzip), FormatError, "'file' must have '.gz' extension");
            TEST_RESULT_STR_Z(compressExtStrip(STRDEF("file"), compressTypeNone), "file", "nothing to strip");
            TEST_RESULT_STR_Z(compressExtStrip(STRDEF("file.gz"), compressTypeGzip), "file", "strip gz");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("lz4 compression");
        {
            TEST_RESULT_UINT(compressTypeFromName(STRDEF("file.lz4")), compressTypeLz4, "check type from name");

#ifdef HAVE_LIBLZ4
            TEST_RESULT_UINT(compressTypeEnum(STRDEF("lz4")), compressTypeLz4, "check enum");
            TEST_RESULT_Z(compressExtZ(compressTypeLz4), ".lz4", "check ext");
            TEST_RESULT_Z(compressTypeZ(compressTypeLz4), "lz4", "check type z");
#else
            TEST_ERROR(compressTypeEnum(STRDEF("lz4")), OptionInvalidValueError, "pgBackRest not compiled with lz4 support");
#endif
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bogus compression");
        {
            TEST_ERROR(compressTypeEnum(strNew(BOGUS_STR)), AssertError, "invalid compression type 'BOGUS'");
            TEST_ERROR(compressTypeEnum(STRDEF("zst")), OptionInvalidValueError, "pgBackRest not compiled with zst support");
        }
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
