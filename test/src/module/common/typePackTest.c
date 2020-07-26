/***********************************************************************************************************************************
Test Packs
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("pckNew()"))
    {
        uint8_t buffer[64];

        TEST_RESULT_UINT(pckFromUInt64(0750, buffer), 2, "mode to pack");
        TEST_RESULT_UINT(pckToUInt32(buffer), 0750, "pack to mode");
        TEST_RESULT_UINT(pckFromUInt64(8192, buffer), 2, "size to pack");
        TEST_RESULT_UINT(pckToUInt32(buffer), 8192, "pack to size");
        TEST_RESULT_UINT(pckFromUInt64(1024 * 1024, buffer), 3, "size to pack");
        TEST_RESULT_UINT(pckToUInt32(buffer), 1024 * 1024, "pack to size");
        TEST_RESULT_UINT(pckFromUInt64(1024 * 1024 * 1024, buffer), 5, "size to pack");
        TEST_RESULT_UINT(pckToUInt32(buffer), 1024 * 1024 * 1024, "pack to size");
        TEST_RESULT_UINT(pckFromUInt64(1911246845, buffer), 5, "time to pack");
        TEST_RESULT_UINT(pckToUInt32(buffer), 1911246845, "pack to time");
        TEST_RESULT_UINT(pckFromUInt64(0xFFFFFFFFFFFFFFFF, buffer), 10, "max to pack");
        TEST_RESULT_UINT(pckToUInt64(buffer), 0xFFFFFFFFFFFFFFFF, "pack to max");
    }

    // *****************************************************************************************************************************
    if (testBegin("pckToLog()"))
    {
        TEST_TITLE("log for new pack");

        Pack *pack = pckNew();
        TEST_RESULT_STR_Z(pckToLog(pack), "{size: 0, extra: 64}", "log");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
