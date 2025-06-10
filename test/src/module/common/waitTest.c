/***********************************************************************************************************************************
Test Wait Handler
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("waitNew(), waitMore, and waitFree()"))
    {
        Wait *wait = NULL;

        TEST_ERROR(waitNew(9999999000), AssertError, "assertion 'waitTime <= 999999000' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("0ms wait");

        TimeMSec begin = timeMSec();

        TEST_ASSIGN(wait, waitNew(0), "new wait");
        TEST_RESULT_UINT(wait->waitTime, 0, "    check wait time");
        TEST_RESULT_UINT(wait->sleepTime, 0, "    check sleep time");
        TEST_RESULT_UINT(wait->sleepPrevTime, 0, "    check sleep prev time");
        TEST_RESULT_BOOL(waitMore(wait), false, "    no wait more");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("100ms with retries after time expired");

        TEST_ASSIGN(wait, waitNew(100), "new wait");
        sleepMSec(100);

        TEST_RESULT_BOOL(waitMore(wait), true, "    time expired, first retry");
        TEST_RESULT_BOOL(waitMore(wait), true, "    time expired, second retry");
        TEST_RESULT_BOOL(waitMore(wait), false, "    time expired, retries expired");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("200ms wait");

        begin = timeMSec();

        TEST_ASSIGN(wait, waitNew(200), "new wait = 0.2 sec");
        TEST_RESULT_UINT(wait->waitTime, 200, "    check wait time");
        TEST_RESULT_UINT(wait->sleepTime, 20, "    check sleep time");
        TEST_RESULT_UINT(wait->sleepPrevTime, 0, "    check sleep prev time");
        TEST_RESULT_BOOL(wait->beginTime > (TimeMSec)1483228800000, true, "    check begin time");

        TEST_RESULT_BOOL(waitMore(wait), true, "    first retry");
        TEST_RESULT_UINT(wait->retry, 1, "    check retry");

        TEST_RESULT_BOOL(waitMore(wait), true, "    second retry");
        TEST_RESULT_UINT(wait->retry, 0, "    check retry");

        TEST_RESULT_BOOL(waitMore(wait), true, "    still going because of time");

        while (waitMore(wait));
        TimeMSec end = timeMSec();

        // Check bounds for time slept (within a range of .1 seconds)
        TEST_RESULT_BOOL(end - begin >= wait->waitTime, true, "    lower range check");
        TEST_RESULT_BOOL(end - begin < wait->waitTime + 100, true, "    upper range check");

        TEST_RESULT_VOID(waitFree(wait), "    free wait");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("1100ms wait");

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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("waitRemainder()");

        TEST_ASSIGN(wait, waitNew(500), "new wait = 500ms");
        TEST_RESULT_BOOL(waitRemains(wait) <= 500, true, "check initial wait remainder");
        TEST_RESULT_BOOL(waitRemains(wait) > 400, true, "check initial wait remainder");

        sleepMSec(100);

        TEST_RESULT_BOOL(waitRemains(wait) <= 400, true, "check updated wait remainder");
        TEST_RESULT_BOOL(waitRemains(wait) > 300, true, "check updated wait remainder");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
