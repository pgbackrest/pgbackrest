/***********************************************************************************************************************************
Time Management
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <sys/time.h>
#include <sys/select.h>

#include "common/debug.h"
#include "common/time.h"

/***********************************************************************************************************************************
Constants describing number of sub-units in an interval
***********************************************************************************************************************************/
#define MSEC_PER_USEC                                               ((TimeMSec)1000)

/**********************************************************************************************************************************/
TimeMSec
timeMSec(void)
{
    FUNCTION_TEST_VOID();

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    FUNCTION_TEST_RETURN(TIME_MSEC, ((TimeMSec)currentTime.tv_sec * MSEC_PER_SEC) + (TimeMSec)currentTime.tv_usec / MSEC_PER_USEC);
}

/**********************************************************************************************************************************/
void
sleepMSec(TimeMSec sleepMSec)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, sleepMSec);
    FUNCTION_TEST_END();

    if (sleepMSec > 0)
    {
        struct timeval delay;
        delay.tv_sec = (time_t)(sleepMSec / MSEC_PER_SEC);
        delay.tv_usec = (suseconds_t)(sleepMSec % MSEC_PER_SEC * 1000);
        select(0, NULL, NULL, NULL, &delay);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
datePartsValid(int year, int month, int day)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, year);
        FUNCTION_TEST_PARAM(INT, month);
        FUNCTION_TEST_PARAM(INT, day);
    FUNCTION_TEST_END();

    static const int daysPerMonth[2][12] =
    {
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
        {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    };

    if (!(year >= 1970 && month > 0 && month <= 12 && day > 0 && day <= daysPerMonth[yearIsLeap(year) ? 1 : 0][month - 1]))
        THROW_FMT(FormatError, "invalid date %04d-%02d-%02d", year, month, day);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
timePartsValid(int hour, int minute, int second)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, hour);
        FUNCTION_TEST_PARAM(INT, minute);
        FUNCTION_TEST_PARAM(INT, second);
    FUNCTION_TEST_END();

    if (!(hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60))
        THROW_FMT(FormatError, "invalid time %02d:%02d:%02d", hour, minute, second);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
tzPartsValid(int tzHour, int tzMinute)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, tzHour);                           // signed hour part of timezone
        FUNCTION_TEST_PARAM(INT, tzMinute);                         // minutes part of timezone
    FUNCTION_TEST_END();

    // Valid time zones range from GMT-12 all the way to GMT+14 (i.e. -1200 and +1400 are the min/max).
    // ??? This is only a sanity check for basic validity of timezone offset of 15 minute intervals until the timezone
    // database is implemented.
    if (!(((tzHour > -12 && tzHour < 14) && (tzMinute % 15 == 0)) || (tzHour == -12 && tzMinute == 0) ||
        (tzHour == 14 && tzMinute == 0)))
    {
        THROW_FMT(FormatError, "invalid timezone %02d%02d", tzHour, tzMinute);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
int
tzOffsetSeconds(int tzHour, int tzMinute)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, tzHour);                           // signed hour part of timezone (e.g. -7)
        FUNCTION_TEST_PARAM(INT, tzMinute);                         // minutes part of timezone
    FUNCTION_TEST_END();

    // Validate the timezone hour and minute
    tzPartsValid(tzHour, tzMinute);

    int sign = 1;

    // Preserve the sign and convert the hours to a positive number for calculating seconds
    if (tzHour < 0)
    {
        sign = -1;
        tzHour = sign * tzHour;
    }

    FUNCTION_TEST_RETURN(INT, sign * (tzHour * 3600 + tzMinute * 60));
}

/**********************************************************************************************************************************/
bool
yearIsLeap(int year)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, year);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(BOOL, (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

/**********************************************************************************************************************************/
int
dayOfYear(int year, int month, int day)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, year);
        FUNCTION_TEST_PARAM(INT, month);
        FUNCTION_TEST_PARAM(INT, day);
    FUNCTION_TEST_END();

    datePartsValid(year, month, day);

    static const int cumulativeDaysPerMonth[2][12] =
    {
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
        {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
    };

    FUNCTION_TEST_RETURN(INT, cumulativeDaysPerMonth[yearIsLeap(year) ? 1 : 0][month - 1] + day);
}

/**********************************************************************************************************************************/
time_t
epochFromParts(int year, int month, int day, int hour, int minute, int second, int tzOffsetSecond)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, year);
        FUNCTION_TEST_PARAM(INT, month);
        FUNCTION_TEST_PARAM(INT, day);
        FUNCTION_TEST_PARAM(INT, hour);
        FUNCTION_TEST_PARAM(INT, minute);
        FUNCTION_TEST_PARAM(INT, second);
        FUNCTION_TEST_PARAM(INT, tzOffsetSecond);
    FUNCTION_TEST_END();

    timePartsValid(hour, minute, second);

    // Return epoch using calculation from https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_16
    FUNCTION_TEST_RETURN(
        TIME,
        -1 * tzOffsetSecond + second + minute * 60 + hour * 3600 +
        (dayOfYear(year, month, day) - 1) * 86400 + (year - 1900 - 70) * 31536000 +
        ((year - 1900 - 69) / 4) * 86400 - ((year - 1900 - 1) / 100) * 86400 + ((year - 1900 + 299) / 400) * 86400);
}
