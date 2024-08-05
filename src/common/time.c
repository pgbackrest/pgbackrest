/***********************************************************************************************************************************
Time Management
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "common/debug.h"
#include "common/time.h"

/***********************************************************************************************************************************
Constants describing number of sub-units in an interval
***********************************************************************************************************************************/
#define MSEC_PER_USEC                                               ((TimeMSec)1000)

/**********************************************************************************************************************************/
FN_EXTERN TimeMSec
timeMSec(void)
{
    FUNCTION_TEST_VOID();

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    FUNCTION_TEST_RETURN(TIME_MSEC, ((TimeMSec)currentTime.tv_sec * MSEC_PER_SEC) + (TimeMSec)currentTime.tv_usec / MSEC_PER_USEC);
}

/**********************************************************************************************************************************/
FN_EXTERN void
sleepMSec(const TimeMSec sleepMSec)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, sleepMSec);
    FUNCTION_TEST_END();

    if (sleepMSec > 0)
    {
        const struct timespec delay =
        {
            .tv_sec = (time_t)(sleepMSec / MSEC_PER_SEC),
            .tv_nsec = (long)(sleepMSec % MSEC_PER_SEC * 1000000),
        };

        nanosleep(&delay, NULL);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
datePartsValid(const int year, const int month, const int day)
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
FN_EXTERN void
timePartsValid(const int hour, const int minute, const int second)
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
FN_EXTERN void
tzPartsValid(const int tzHour, const int tzMinute)
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
FN_EXTERN int
tzOffsetSeconds(int tzHour, const int tzMinute)
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
FN_EXTERN bool
yearIsLeap(const int year)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, year);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(BOOL, (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

/**********************************************************************************************************************************/
FN_EXTERN int
dayOfYear(const int year, const int month, const int day)
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
FN_EXTERN time_t
epochFromParts(
    const int year, const int month, const int day, const int hour, const int minute, const int second, const int tzOffsetSecond)
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

/**********************************************************************************************************************************/
static int
timePartFromZN(const char *const time, const char *const part, const size_t partSize)
{
    int result = 0;
    int power = 1;

    for (size_t partIdx = partSize - 1; partIdx < partSize; partIdx--)
    {
        if (!isdigit(part[partIdx]))
            THROW_FMT(FormatError, "invalid date/time %s", time);

        result += (part[partIdx] - '0') * power;
        power *= 10;
    }

    return result;
}

FN_EXTERN time_t
epochFromZ(const char *const time)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, time);
    FUNCTION_TEST_END();

    ASSERT(time != NULL);

    // Validate structure of date/time
    if (strlen(time) < 19 || time[4] != '-' || time[7] != '-' || time[10] != ' ' || time[13] != ':' || time[16] != ':')
        THROW_FMT(FormatError, "invalid date/time %s", time);

    // Parse date/time
    const int year = timePartFromZN(time, time, 4);
    const int month = timePartFromZN(time, time + 5, 2);
    const int day = timePartFromZN(time, time + 8, 2);
    const int hour = timePartFromZN(time, time + 11, 2);
    const int minute = timePartFromZN(time, time + 14, 2);
    const int second = timePartFromZN(time, time + 17, 2);

    // Confirm date and time parts are valid
    datePartsValid(year, month, day);
    timePartsValid(hour, minute, second);

    // Consume milliseconds when present (they are omitted from the result)
    const char *part = time + 19;

    if ((*part == '.' || *part == ',') && strlen(part) >= 2)
    {
        part++;

        while (*part != 0 && isdigit(*part))
            part++;
    }

    // Add timezone offset when present
    if ((*part == '+' || *part == '-') && strlen(part) >= 3)
    {
        const int offsetHour = timePartFromZN(time, part + 1, 2) * (*part == '-' ? -1 : 1);
        part += 3;

        // Offset separator is optional
        if (*part == ':')
            part++;

        // Offset minutes are optional
        int offsetMinute = 0;

        if (strlen(part) == 2)
        {
            offsetMinute = timePartFromZN(time, part, 2);
            part += 2;
        }

        // Make sure there is nothing left over
        if (*part != 0)
            THROW_FMT(FormatError, "invalid date/time %s", time);

        FUNCTION_TEST_RETURN(
            TIME, epochFromParts(year, month, day, hour, minute, second, tzOffsetSeconds(offsetHour, offsetMinute)));
    }

    // Make sure there is nothing left over
    if (*part != 0)
        THROW_FMT(FormatError, "invalid date/time %s", time);

    // If no timezone was specified then use the current timezone. Set tm_isdst to -1 to force mktime to consider if DST. For
    // example, if system time is America/New_York then 2019-09-14 20:02:49 was a time in DST so the Epoch value should be
    // 1568505769 (and not 1568509369 which would be 2019-09-14 21:02:49 - an hour too late).
    struct tm timePart =
    {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = second,
        .tm_isdst = -1,
    };

    FUNCTION_TEST_RETURN(TIME, mktime(&timePart));
}
