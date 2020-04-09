/***********************************************************************************************************************************
Test Wait Handler
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("waitNew(), waitMore, and waitFree()"))
    {
        Wait *wait = NULL;

        TEST_ERROR(waitNew(9999999000), AssertError, "assertion 'waitTime <= 999999000' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TimeMSec begin = timeMSec();

        TEST_ASSIGN(wait, waitNew(200), "new wait = 0.2 sec");
        TEST_RESULT_UINT(waitRemaining(wait), 200, "    check remaining time");
        TEST_RESULT_UINT(wait->waitTime, 200, "    check wait time");
        TEST_RESULT_UINT(wait->sleepTime, 20, "    check sleep time");
        TEST_RESULT_UINT(wait->sleepPrevTime, 0, "    check sleep prev time");
        TEST_RESULT_BOOL(wait->beginTime > (TimeMSec)1483228800000, true, "    check begin time");

        while (waitMore(wait));
        TimeMSec end = timeMSec();

        // Check bounds for time slept (within a range of .1 seconds)
        TEST_RESULT_BOOL(end - begin >= wait->waitTime, true, "    lower range check");
        TEST_RESULT_BOOL(end - begin < wait->waitTime + 100, true, "    upper range check");

        TEST_RESULT_VOID(waitFree(wait), "    free wait");

        // -------------------------------------------------------------------------------------------------------------------------
        begin = timeMSec();

        TEST_ASSIGN(wait, waitNew(1100), "new wait = 1.1 sec");
        TEST_RESULT_UINT(wait->waitTime, 1100, "    check wait time");
        TEST_RESULT_UINT(wait->sleepTime, 100, "    check sleep time");
        TEST_RESULT_UINT(wait->sleepPrevTime, 0, "    check sleep prev time");
        TEST_RESULT_BOOL(wait->beginTime > (TimeMSec)1483228800000, true, "    check begin time");

        while (waitMore(wait));
        end = timeMSec();

        // Check bounds for time slept (within a range of .1 seconds)
        TEST_RESULT_BOOL(end - begin >= wait->waitTime, true, "    lower range check");
        TEST_RESULT_BOOL(end - begin < wait->waitTime + 1200, true, "    upper range check");

        TEST_RESULT_VOID(waitFree(wait), "    free wait");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
