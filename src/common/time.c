/***********************************************************************************************************************************
Time Management
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <sys/time.h>

#include "common/debug.h"
#include "common/time.h"

/***********************************************************************************************************************************
Constants describing number of sub-units in an interval
***********************************************************************************************************************************/
#define MSEC_PER_USEC                                               ((TimeMSec)1000)

/***********************************************************************************************************************************
Epoch time in milliseconds
***********************************************************************************************************************************/
TimeMSec
timeMSec(void)
{
    FUNCTION_TEST_VOID();

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    FUNCTION_TEST_RETURN(((TimeMSec)currentTime.tv_sec * MSEC_PER_SEC) + (TimeMSec)currentTime.tv_usec / MSEC_PER_USEC);
}

/***********************************************************************************************************************************
Sleep for specified milliseconds
***********************************************************************************************************************************/
void
sleepMSec(TimeMSec sleepMSec)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, sleepMSec);
    FUNCTION_TEST_END();

    struct timeval delay;
    delay.tv_sec = (time_t)(sleepMSec / MSEC_PER_SEC);
    delay.tv_usec = (time_t)(sleepMSec % MSEC_PER_SEC * 1000);
    select(0, NULL, NULL, NULL, &delay);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool yearIsLeap(int year)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, year);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

/**********************************************************************************************************************************/
int dayOfYear(int year, int month, int day)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, year);
        FUNCTION_TEST_PARAM(INT, month);
        FUNCTION_TEST_PARAM(INT, day);
    FUNCTION_TEST_END();

    static const int daysPerMonth[2][12] =
    {
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
        {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
    };

    FUNCTION_TEST_RETURN(daysPerMonth[yearIsLeap(year) ? 1 : 0][month - 1] + day);
}

/**********************************************************************************************************************************/
time_t
epochFromParts(int year, int month, int day, int hour, int minute, int second)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, year);
        FUNCTION_TEST_PARAM(INT, month);
        FUNCTION_TEST_PARAM(INT, day);
        FUNCTION_TEST_PARAM(INT, hour);
        FUNCTION_TEST_PARAM(INT, minute);
        FUNCTION_TEST_PARAM(INT, second);
    FUNCTION_TEST_END();

    // Return epoch time using calculation from https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_16
    FUNCTION_TEST_RETURN(
        second + minute * 60 + hour * 3600 +
        (dayOfYear(year, month, day) - 1) * 86400 + (year - 1900 - 70) * 31536000 +
        ((year - 1900 - 69) / 4) * 86400 - ((year - 1900 - 1) / 100) * 86400 + ((year - 1900 + 299) / 400) * 86400);
}
