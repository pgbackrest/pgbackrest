/***********************************************************************************************************************************
Test Statistics Collector
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("all"))
    {
        TEST_RESULT_VOID(statInit(), "init stats");

        const String *statTlsClient = STRDEF("tls.client");

        TEST_RESULT_UINT(lstSize(statLocalData.stat), 0, "stat list is empty");

        TEST_RESULT_VOID(statInc(statTlsClient), "inc tls.client");
        TEST_RESULT_UINT(lstSize(statLocalData.stat), 1, "stat list has one stat");
        TEST_RESULT_VOID(statInc(statTlsClient), "inc tls.client");
        TEST_RESULT_UINT(lstSize(statLocalData.stat), 1, "stat list has one stat");

        // TEST_RESULT_STR_Z(jsonFromVar(statToVar(statTlsClient)), "{}", "stat output");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
