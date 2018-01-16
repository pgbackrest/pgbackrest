/***********************************************************************************************************************************
Test Wait Handler
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("waitNew(), waitMore, and waitFree()"))
    {
        Wait *wait = NULL;

        TEST_ERROR(waitNew(0.01), AssertError, "waitTime must be >= 0.1 and <= 999999.0");
        TEST_ERROR(waitNew(9999999), AssertError, "waitTime must be >= 0.1 and <= 999999.0");

        // -------------------------------------------------------------------------------------------------------------------------
        unsigned long begin = timeUSec();

        TEST_ASSIGN(wait, waitNew(0.2), "new wait = 0.2 sec");
        TEST_RESULT_DOUBLE(wait->waitTime, 200000, "    check wait time");
        TEST_RESULT_DOUBLE(wait->sleepTime, 20000, "    check sleep time");
        TEST_RESULT_DOUBLE(wait->sleepPrevTime, 0, "    check sleep prev time");
        TEST_RESULT_BOOL(wait->beginTime > (unsigned long)1483228800000000, true, "    check begin time");

        while (waitMore(wait));
        unsigned long end = timeUSec();

        // Check bounds for time slept (within a range of .1 seconds)
        TEST_RESULT_BOOL(end - begin >= wait->waitTime, true, "    lower range check");
        TEST_RESULT_BOOL(end - begin < wait->waitTime + 100000, true, "    upper range check");

        TEST_RESULT_VOID(waitFree(wait), "    free wait");

        // -------------------------------------------------------------------------------------------------------------------------
        begin = timeUSec();

        TEST_ASSIGN(wait, waitNew(1.1), "new wait = 1.1 sec");
        TEST_RESULT_DOUBLE(wait->waitTime, 1100000, "    check wait time");
        TEST_RESULT_DOUBLE(wait->sleepTime, 100000, "    check sleep time");
        TEST_RESULT_DOUBLE(wait->sleepPrevTime, 0, "    check sleep prev time");
        TEST_RESULT_BOOL(wait->beginTime > (unsigned long)1483228800000000, true, "    check begin time");

        while (waitMore(wait));
        end = timeUSec();

        // Check bounds for time slept (within a range of .1 seconds)
        TEST_RESULT_BOOL(end - begin >= wait->waitTime, true, "    lower range check");
        TEST_RESULT_BOOL(end - begin < wait->waitTime + 1200000, true, "    upper range check");

        TEST_RESULT_VOID(waitFree(wait), "    free wait");
    }
}
