/***********************************************************************************************************************************
Test Blob
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("Blob"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Add enough data to allocate a new page");

        #define TEST_DATA_MAX                                       5
        #define TEST_DATA_SIZE                                      (BLOB_BLOCK_SIZE / (TEST_DATA_MAX - 1))

        void *dataList[TEST_DATA_MAX];
        const void *dataListPtr[TEST_DATA_MAX];
        Blob *blob = NULL;

        TEST_ASSIGN(blob, blbNew(), "new blob");

        for (int dataIdx = 0; dataIdx < TEST_DATA_MAX; dataIdx++)
        {
            dataList[dataIdx] = memNew(TEST_DATA_SIZE);
            memset(dataList[dataIdx], dataIdx, TEST_DATA_SIZE);

            TEST_ASSIGN(dataListPtr[dataIdx], blbAdd(blob, dataList[dataIdx], TEST_DATA_SIZE), "data add");
        }

        for (int dataIdx = 0; dataIdx < TEST_DATA_MAX; dataIdx++)
            TEST_RESULT_INT(memcmp(dataList[dataIdx], dataListPtr[dataIdx], TEST_DATA_SIZE), 0, "data check");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Add data >= than block size");

        void *data = memNew(BLOB_BLOCK_SIZE);
        const void *dataPtr = NULL;

        memset(data, 255, BLOB_BLOCK_SIZE);

        TEST_ASSIGN(dataPtr, blbAdd(blob, data, BLOB_BLOCK_SIZE), "data add");
        TEST_RESULT_INT(memcmp(data, dataPtr, BLOB_BLOCK_SIZE), 0, "data check");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
