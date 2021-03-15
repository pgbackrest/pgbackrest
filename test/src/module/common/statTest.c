/***********************************************************************************************************************************
Test Statistics Collector
***********************************************************************************************************************************/
#include "common/type/json.h"

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
        const String *statTlsClient = STRDEF("tls.client");
        const String *statHttpSession = STRDEF("http.session");

        TEST_RESULT_UINT(lstSize(statLocalData.stat), 0, "stat list is empty");

        TEST_RESULT_VOID(statInc(statTlsClient), "inc tls.client");
        TEST_RESULT_UINT(lstSize(statLocalData.stat), 1, "stat list has one stat");
        TEST_RESULT_VOID(statInc(statTlsClient), "inc tls.client");
        TEST_RESULT_UINT(lstSize(statLocalData.stat), 1, "stat list has one stat");
        TEST_RESULT_VOID(statInc(statHttpSession), "inc http.session");
        TEST_RESULT_UINT(lstSize(statLocalData.stat), 2, "stat list has two stats");

        TEST_RESULT_STR_Z(jsonFromKv(statToKv()), "{\"http.session\":{\"total\":1},\"tls.client\":{\"total\":2}}", "stat output");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
