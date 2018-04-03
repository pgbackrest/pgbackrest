/***********************************************************************************************************************************
Test Time Management
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("timeMSec()"))
    {
        // Make sure the time returned is between 2017 and 2100
        TEST_RESULT_BOOL(timeMSec() > (TimeMSec)1483228800000, true, "lower range check");
        TEST_RESULT_BOOL(timeMSec() < (TimeMSec)4102444800000, true, "upper range check");
    }

    // *****************************************************************************************************************************
    if (testBegin("sleepMSec()"))
    {
        // Sleep and measure time slept
        TimeMSec begin = timeMSec();
        sleepMSec(1400);
        TimeMSec end = timeMSec();

        // Check bounds for time slept (within a range of .1 seconds)
        TEST_RESULT_BOOL(end - begin >= (TimeMSec)1400, true, "lower range check");
        TEST_RESULT_BOOL(end - begin < (TimeMSec)1500, true, "upper range check");
    }
}
