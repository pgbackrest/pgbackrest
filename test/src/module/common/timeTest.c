/***********************************************************************************************************************************
Test Time Management
***********************************************************************************************************************************/
#include <limits.h>

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

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

    // *****************************************************************************************************************************
    if (testBegin("yearIsLeap(), dayOfYear(), timePartsValid(), and epochFromParts()"))
    {
        TEST_TITLE("is leap year");

        TEST_RESULT_BOOL(yearIsLeap(1996), true, "1996 is leap year");
        TEST_RESULT_BOOL(yearIsLeap(2000), true, "2000 is leap year");
        TEST_RESULT_BOOL(yearIsLeap(2001), false, "2001 is not leap year");
        TEST_RESULT_BOOL(yearIsLeap(2100), false, "2100 is not leap year");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("day of year");

        TEST_RESULT_INT(dayOfYear(1995, 3, 1), 60, "1995-03-01 day of year");
        TEST_RESULT_INT(dayOfYear(1996, 3, 1), 61, "1996-03-01 day of year");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("date parts valid");

        TEST_ERROR(datePartsValid(1969,  1,  1), FormatError, "invalid date 1969-01-01");
        TEST_ERROR(datePartsValid(1970,  0,  0), FormatError, "invalid date 1970-00-00");
        TEST_ERROR(datePartsValid(1970, 13,  0), FormatError, "invalid date 1970-13-00");
        TEST_ERROR(datePartsValid(1970, 12,  0), FormatError, "invalid date 1970-12-00");
        TEST_ERROR(datePartsValid(1970,  2, 29), FormatError, "invalid date 1970-02-29");
        TEST_RESULT_VOID(datePartsValid(1972, 2, 29), "valid date");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("time parts valid");

        TEST_ERROR(timePartsValid(-1,  0,  0), FormatError, "invalid time -1:00:00");
        TEST_ERROR(timePartsValid(24,  0,  0), FormatError, "invalid time 24:00:00");
        TEST_ERROR(timePartsValid( 0, -1,  0), FormatError, "invalid time 00:-1:00");
        TEST_ERROR(timePartsValid( 0, 60,  0), FormatError, "invalid time 00:60:00");
        TEST_ERROR(timePartsValid( 0,  0, -1), FormatError, "invalid time 00:00:-1");
        TEST_ERROR(timePartsValid( 0,  0, 60), FormatError, "invalid time 00:00:60");
        TEST_RESULT_VOID(timePartsValid(23, 59, 59), "valid time");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("time from parts");

        TEST_RESULT_INT(epochFromParts(1970, 1, 1, 0, 0, 0), 0, "beginning of epoch");
        TEST_RESULT_INT(epochFromParts(1996, 3, 3, 3, 3, 3), 825822183, "march of leap year");
        TEST_RESULT_INT(epochFromParts(2000, 4, 4, 4, 4, 4), 954821044, "april of non-leap year");
        TEST_RESULT_INT(epochFromParts(2038, 1, 19, 3, 14, 7), INT_MAX, "end of 32-bit signed int epoch");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
