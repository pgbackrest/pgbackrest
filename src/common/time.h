/***********************************************************************************************************************************
Time Management
***********************************************************************************************************************************/
#ifndef COMMON_TIME_H
#define COMMON_TIME_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/***********************************************************************************************************************************
Time types
***********************************************************************************************************************************/
typedef uint64_t TimeMSec;

/***********************************************************************************************************************************
Constants describing number of sub-units in an interval
***********************************************************************************************************************************/
#define MSEC_PER_SEC                                                ((TimeMSec)1000)
#define SEC_PER_DAY                                                 ((time_t)86400)

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Sleep for specified milliseconds
FN_EXTERN void sleepMSec(TimeMSec sleepMSec);

// Epoch time in milliseconds
FN_EXTERN TimeMSec timeMSec(void);

// Are the date parts valid? (year >= 1970, month 1-12, day 1-31)
FN_EXTERN void datePartsValid(int year, int month, int day);

// Are the time parts valid?
FN_EXTERN void timePartsValid(int hour, int minute, int second);

// Are the timezone offset parts valid?
FN_EXTERN void tzPartsValid(int tzHour, int tzMinute);

// Given the hours, minutes and sign of a time zone (e.g. -0700 => -7, 0) return the signed number or seconds (e.g. -25200)
FN_EXTERN int tzOffsetSeconds(int tzHour, int tzMinute);

// Is the year a leap year?
FN_EXTERN bool yearIsLeap(int year);

// Get days since the beginning of the year (year >= 1970, month 1-12, day 1-31), returns 1-366
FN_EXTERN int dayOfYear(int year, int month, int day);

// Return epoch time from date/time parts (year >= 1970, month 1-12, day 1-31, hour 0-23, minute 0-59, second 0-59, tzOffsetSecond
// is the number of seconds plus or minus (+/-) the provided time - if 0, then the datetime is assumed to be UTC)
FN_EXTERN time_t epochFromParts(int year, int month, int day, int hour, int minute, int second, int tzOffsetSecond);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_TIME_MSEC_TYPE                                                                                                \
    TimeMSec
#define FUNCTION_LOG_TIME_MSEC_FORMAT(value, buffer, bufferSize)                                                                   \
    cvtUInt64ToZ(value, buffer, bufferSize)

#endif
