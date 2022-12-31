/***********************************************************************************************************************************
Time Management
***********************************************************************************************************************************/
#ifndef COMMON_TIME_H
#define COMMON_TIME_H

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
FV_EXTERN void sleepMSec(TimeMSec sleepMSec);

// Epoch time in milliseconds
FV_EXTERN TimeMSec timeMSec(void);

// Are the date parts valid? (year >= 1970, month 1-12, day 1-31)
FV_EXTERN void datePartsValid(int year, int month, int day);

// Are the time parts valid?
FV_EXTERN void timePartsValid(int hour, int minute, int second);

// Are the timezone offset parts valid?
FV_EXTERN void tzPartsValid(int tzHour, int tzMinute);

// Given the hours, minutes and sign of a time zone (e.g. -0700 => -7, 0) return the signed number or seconds (e.g. -25200)
FV_EXTERN int tzOffsetSeconds(int tzHour, int tzMinute);

// Is the year a leap year?
FV_EXTERN bool yearIsLeap(int year);

// Get days since the beginning of the year (year >= 1970, month 1-12, day 1-31), returns 1-366
FV_EXTERN int dayOfYear(int year, int month, int day);

// Return epoch time from date/time parts (year >= 1970, month 1-12, day 1-31, hour 0-23, minute 0-59, second 0-59, tzOffsetSecond
// is the number of seconds plus or minus (+/-) the provided time - if 0, then the datetime is assumed to be UTC)
FV_EXTERN time_t epochFromParts(int year, int month, int day, int hour, int minute, int second, int tzOffsetSecond);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_TIME_MSEC_TYPE                                                                                                \
    TimeMSec
#define FUNCTION_LOG_TIME_MSEC_FORMAT(value, buffer, bufferSize)                                                                   \
    cvtUInt64ToZ(value, buffer, bufferSize)

#endif
