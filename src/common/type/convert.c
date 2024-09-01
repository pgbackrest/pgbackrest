/***********************************************************************************************************************************
Convert C Types
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/debug.h"
#include "common/macro.h"
#include "common/time.h"
#include "common/type/convert.h"

/***********************************************************************************************************************************
Check results of strto*() function for:
    * leading/trailing spaces
    * invalid characters
    * blank string
    * error in errno
***********************************************************************************************************************************/
static void
cvtZToIntValid(const int errNo, const int base, const char *const value, const char *const endPtr, const char *const type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, errNo);
        FUNCTION_TEST_PARAM(STRINGZ, value);
        FUNCTION_TEST_PARAM(STRINGZ, endPtr);
        FUNCTION_TEST_PARAM(STRINGZ, type);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);
    ASSERT(endPtr != NULL);

    if (errNo != 0 || *value == '\0' || isspace(*value) || *endPtr != '\0')
        THROW_FMT(FormatError, "unable to convert base %d string '%s' to %s", base, value, type);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Convert zero-terminated string to int64 and validate result
***********************************************************************************************************************************/
static int64_t
cvtZToInt64Internal(const char *const value, const char *const type, const int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
        FUNCTION_TEST_PARAM(STRINGZ, type);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);
    ASSERT(type != NULL);

    // Convert from string
    errno = 0;
    char *endPtr = NULL;
    const int64_t result = strtoll(value, &endPtr, base);

    // Validate the result
    cvtZToIntValid(errno, base, value, endPtr, type);

    FUNCTION_TEST_RETURN(INT64, result);
}

/***********************************************************************************************************************************
Convert zero-terminated string to uint64 and validate result
***********************************************************************************************************************************/
static uint64_t
cvtZToUInt64Internal(const char *const value, const char *const type, const int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
        FUNCTION_TEST_PARAM(STRINGZ, type);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);
    ASSERT(type != NULL);

    // Convert from string
    errno = 0;
    char *endPtr = NULL;
    const uint64_t result = strtoull(value, &endPtr, base);

    // Validate the result
    cvtZToIntValid(errno, base, value, endPtr, type);

    FUNCTION_TEST_RETURN(UINT64, result);
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
cvtBoolToZ(const bool value, char *const buffer, const size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    const size_t result = (size_t)snprintf(buffer, bufferSize, "%s", cvtBoolToConstZ(value));

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

FN_EXTERN const char *
cvtBoolToConstZ(bool value)
{
    return value ? TRUE_Z : FALSE_Z;
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
cvtDoubleToZ(const double value, char *const buffer, const size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DOUBLE, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Convert to a string
    const size_t result = (size_t)snprintf(buffer, bufferSize, "%lf", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    // Any formatted double should be at least 8 characters, i.e. 0.000000
    ASSERT(strlen(buffer) >= 8);
    // Any formatted double should have a decimal point
    ASSERT(strchr(buffer, '.') != NULL);

    // Strip off any final 0s and the decimal point if there are no non-zero digits after it
    char *end = buffer + strlen(buffer) - 1;

    while (*end == '0' || *end == '.')
    {
        // It should not be possible to go past the beginning because format "%lf" will always write a decimal point
        ASSERT(end > buffer);

        end--;

        if (*(end + 1) == '.')
            break;
    }

    // Zero terminate the string
    end[1] = 0;

    // Return string length
    FUNCTION_TEST_RETURN(SIZE, (size_t)(end - buffer + 1));
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
cvtIntToZ(const int value, char *const buffer, const size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    const size_t result = (size_t)snprintf(buffer, bufferSize, "%d", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

FN_EXTERN int
cvtZToIntBase(const char *const value, const int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    const int64_t result = cvtZToInt64Internal(value, "int", base);

    if (result > INT_MAX || result < INT_MIN)
        THROW_FMT(FormatError, "unable to convert base %d string '%s' to int", base, value);

    FUNCTION_TEST_RETURN(INT, (int)result);
}

FN_EXTERN int
cvtZToInt(const char *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(INT, cvtZToIntBase(value, 10));
}

FN_EXTERN int
cvtZSubNToIntBase(const char *const value, const size_t offset, const size_t size, const int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
        FUNCTION_TEST_PARAM(SIZE, offset);
        FUNCTION_TEST_PARAM(SIZE, size);
        FUNCTION_TEST_PARAM(INT, base);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    char buffer[CVT_BASE10_BUFFER_SIZE + 1];
    ASSERT(size <= CVT_BASE10_BUFFER_SIZE);
    memcpy(buffer, value + offset, size);
    buffer[size] = '\0';

    FUNCTION_TEST_RETURN(INT, cvtZToIntBase(buffer, base));
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
cvtInt64ToZ(const int64_t value, char *const buffer, const size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT64, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    const size_t result = (size_t)snprintf(buffer, bufferSize, "%" PRId64, value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

FN_EXTERN int64_t
cvtZToInt64Base(const char *const value, const int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(INT64, cvtZToInt64Internal(value, "int64", base));
}

FN_EXTERN int64_t
cvtZToInt64(const char *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(INT64, cvtZToInt64Base(value, 10));
}

FN_EXTERN int64_t
cvtZSubNToInt64Base(const char *const value, const size_t offset, const size_t size, const int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
        FUNCTION_TEST_PARAM(SIZE, offset);
        FUNCTION_TEST_PARAM(SIZE, size);
        FUNCTION_TEST_PARAM(INT, base);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    char buffer[CVT_BASE10_BUFFER_SIZE + 1];
    ASSERT(size <= CVT_BASE10_BUFFER_SIZE);
    memcpy(buffer, value + offset, size);
    buffer[size] = '\0';

    FUNCTION_TEST_RETURN(INT64, cvtZToInt64Base(buffer, base));
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
cvtModeToZ(const mode_t value, char *const buffer, const size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MODE, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    const size_t result = (size_t)snprintf(buffer, bufferSize, "%04o", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

FN_EXTERN mode_t
cvtZToMode(const char *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(MODE, (mode_t)cvtZToUIntBase(value, 8));
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
cvtSizeToZ(const size_t value, char *const buffer, const size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    const size_t result = (size_t)snprintf(buffer, bufferSize, "%zu", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
cvtTimeToZ(const char *const format, const time_t value, char *const buffer, const size_t bufferSize, const CvtTimeToZParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, format);
        FUNCTION_TEST_PARAM(TIME, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
        FUNCTION_TEST_PARAM(BOOL, param.utc);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    struct tm timePart;

    // We can ignore this warning here since the format parameter of cvtTimeToZP() is checked
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    const size_t result = strftime(
        buffer, bufferSize, format, param.utc ? gmtime_r(&value, &timePart) : localtime_r(&value, &timePart));
#pragma GCC diagnostic pop

    if (result == 0)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

/**********************************************************************************************************************************/
// Helper to convert a time part, e.g. year
static int
cvtZNToTimePart(const char *const time, const char *const part, const size_t partSize)
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
cvtZToTime(const char *const time)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, time);
    FUNCTION_TEST_END();

    ASSERT(time != NULL);

    // Validate structure of date/time
    if (strlen(time) < 19 || time[4] != '-' || time[7] != '-' || time[10] != ' ' || time[13] != ':' || time[16] != ':')
        THROW_FMT(FormatError, "invalid date/time %s", time);

    // Parse date/time
    const int year = cvtZNToTimePart(time, time, 4);
    const int month = cvtZNToTimePart(time, time + 5, 2);
    const int day = cvtZNToTimePart(time, time + 8, 2);
    const int hour = cvtZNToTimePart(time, time + 11, 2);
    const int minute = cvtZNToTimePart(time, time + 14, 2);
    const int second = cvtZNToTimePart(time, time + 17, 2);

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
        const int offsetHour = cvtZNToTimePart(time, part + 1, 2) * (*part == '-' ? -1 : 1);
        part += 3;

        // Offset separator is optional
        if (*part == ':')
            part++;

        // Offset minutes are optional
        int offsetMinute = 0;

        if (strlen(part) == 2)
        {
            offsetMinute = cvtZNToTimePart(time, part, 2);
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

/**********************************************************************************************************************************/
FN_EXTERN size_t
cvtUIntToZ(const unsigned int value, char *const buffer, const size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    const size_t result = (size_t)snprintf(buffer, bufferSize, "%u", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

FN_EXTERN unsigned int
cvtZToUIntBase(const char *const value, const int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    const uint64_t result = cvtZToUInt64Internal(value, "unsigned int", base);

    // Don't allow negative numbers even though strtoull() does and check max value
    if (*value == '-' || result > UINT_MAX)
        THROW_FMT(FormatError, "unable to convert base %d string '%s' to unsigned int", base, value);

    FUNCTION_TEST_RETURN(UINT, (unsigned int)result);
}

FN_EXTERN unsigned int
cvtZToUInt(const char *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(UINT, cvtZToUIntBase(value, 10));
}

FN_EXTERN unsigned int
cvtZSubNToUIntBase(const char *const value, const size_t offset, const size_t size, const int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
        FUNCTION_TEST_PARAM(SIZE, offset);
        FUNCTION_TEST_PARAM(SIZE, size);
        FUNCTION_TEST_PARAM(INT, base);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    char buffer[CVT_BASE10_BUFFER_SIZE + 1];
    ASSERT(size <= CVT_BASE10_BUFFER_SIZE);
    memcpy(buffer, value + offset, size);
    buffer[size] = '\0';

    FUNCTION_TEST_RETURN(UINT, cvtZToUIntBase(buffer, base));
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
cvtUInt64ToZ(const uint64_t value, char *const buffer, const size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    const size_t result = (size_t)snprintf(buffer, bufferSize, "%" PRIu64, value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

FN_EXTERN uint64_t
cvtZToUInt64Base(const char *const value, const int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    const uint64_t result = cvtZToUInt64Internal(value, "uint64", base);

    // Don't allow negative numbers even though strtoull() does
    if (*value == '-')
        THROW_FMT(FormatError, "unable to convert base %d string '%s' to uint64", base, value);

    FUNCTION_TEST_RETURN(UINT64, result);
}

FN_EXTERN uint64_t
cvtZToUInt64(const char *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(UINT64, cvtZToUInt64Base(value, 10));
}

FN_EXTERN uint64_t
cvtZSubNToUInt64Base(const char *const value, const size_t offset, const size_t size, const int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
        FUNCTION_TEST_PARAM(SIZE, offset);
        FUNCTION_TEST_PARAM(SIZE, size);
        FUNCTION_TEST_PARAM(INT, base);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    char buffer[CVT_BASE10_BUFFER_SIZE + 1];
    ASSERT(size <= CVT_BASE10_BUFFER_SIZE);
    memcpy(buffer, value + offset, size);
    buffer[size] = '\0';

    FUNCTION_TEST_RETURN(UINT64, cvtZToUInt64Base(buffer, base));
}

/**********************************************************************************************************************************/
FN_EXTERN void
cvtUInt64ToVarInt128(uint64_t value, uint8_t *const buffer, size_t *const bufferPos, const size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, value);
        FUNCTION_TEST_PARAM_P(VOID, buffer);
        FUNCTION_TEST_PARAM(UINT64, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);
    ASSERT(bufferPos != NULL);
    ASSERT(bufferSize > *bufferPos);

    // Keep encoding bytes while the remaining value is greater than 7 bits
    while (value >= 0x80)
    {
        // Encode the lower order 7 bits, adding the continuation bit to indicate there is more data
        buffer[*bufferPos] = (unsigned char)value | 0x80;

        // Shift the value to remove bits that have been encoded
        value >>= 7;

        // Keep track of size so we know how many bytes to write out
        (*bufferPos)++;

        // Make sure the buffer won't overflow
        if (*bufferPos >= bufferSize)
            THROW(AssertError, "buffer overflow");
    }

    // Encode the last 7 bits of value
    buffer[*bufferPos] = (unsigned char)value;
    (*bufferPos)++;

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN uint64_t
cvtUInt64FromVarInt128(const uint8_t *const buffer, size_t *const bufferPos, const size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
        FUNCTION_TEST_PARAM_P(SIZE, bufferPos);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);
    ASSERT(bufferPos != NULL);

    // Decode all bytes
    uint64_t result = 0;
    uint8_t byte;

    for (unsigned int bufferIdx = 0; bufferIdx < CVT_VARINT128_BUFFER_SIZE; bufferIdx++)
    {
        // Error if the buffer position is beyond the buffer size
        if (*bufferPos >= bufferSize)
            THROW(FormatError, "buffer position is beyond buffer size");

        // Get the next encoded byte
        byte = buffer[*bufferPos];

        // Shift the lower order 7 encoded bits into the uint64 in reverse order
        result |= (uint64_t)(byte & 0x7f) << (7 * bufferIdx);

        // Increment buffer position to indicate that the byte has been processed
        (*bufferPos)++;

        // Done if the high order bit is not set to indicate more data
        if (byte < 0x80)
            break;
    }

    // By this point all bytes should have been read so error if this is not the case. This could be due to a coding error or
    // corruption in the data stream.
    if (byte >= 0x80)
        THROW(FormatError, "unterminated varint-128 integer");

    FUNCTION_TEST_RETURN(UINT64, result);
}
