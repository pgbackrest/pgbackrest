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
    if (testBegin("timeUSec()"))
    {
        // Make sure the time returned is between 2017 and 2100
        TEST_RESULT_BOOL(timeUSec() > (TimeUSec)1483228800000000, true, "lower range check");
        TEST_RESULT_BOOL(timeUSec() < (TimeUSec)4102444800000000, true, "upper range check");
    }

    // *****************************************************************************************************************************
    if (testBegin("sleepUSec()"))
    {
        // Sleep and measure time slept
        TimeUSec begin = timeUSec();
        sleepUSec(1400000);
        TimeUSec end = timeUSec();

        // Check bounds for time slept (within a range of .1 seconds)
        TEST_RESULT_BOOL(end - begin > (TimeUSec)1400000, true, "lower range check");
        TEST_RESULT_BOOL(end - begin < (TimeUSec)1500000, true, "upper range check");
    }
}
