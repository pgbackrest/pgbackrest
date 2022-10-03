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
#include "common/type/convert.h"

/***********************************************************************************************************************************
Check results of strto*() function for:
    * leading/trailing spaces
    * invalid characters
    * blank string
    * error in errno
***********************************************************************************************************************************/
static void
cvtZToIntValid(int errNo, int base, const char *value, const char *endPtr, const char *type)
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
cvtZToInt64Internal(const char *value, const char *type, int base)
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
    int64_t result = strtoll(value, &endPtr, base);

    // Validate the result
    cvtZToIntValid(errno, base, value, endPtr, type);

    FUNCTION_TEST_RETURN(INT64, result);
}

/***********************************************************************************************************************************
Convert zero-terminated string to uint64 and validate result
***********************************************************************************************************************************/
static uint64_t
cvtZToUInt64Internal(const char *value, const char *type, int base)
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
    uint64_t result = strtoull(value, &endPtr, base);

    // Validate the result
    cvtZToIntValid(errno, base, value, endPtr, type);

    FUNCTION_TEST_RETURN(UINT64, result);
}

/**********************************************************************************************************************************/
size_t
cvtBoolToZ(bool value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%s", cvtBoolToConstZ(value));

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

const char *
cvtBoolToConstZ(bool value)
{
    return value ? TRUE_Z : FALSE_Z;
}

/**********************************************************************************************************************************/
size_t
cvtCharToZ(char value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%c", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

/**********************************************************************************************************************************/
size_t
cvtDoubleToZ(double value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DOUBLE, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Convert to a string
    size_t result = (size_t)snprintf(buffer, bufferSize, "%lf", value);

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

double
cvtZToDouble(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    double result = 0;
    sscanf(value, "%lf", &result);

    if (result == 0 && strcmp(value, "0") != 0)
        THROW_FMT(FormatError, "unable to convert string '%s' to double", value);

    FUNCTION_TEST_RETURN(DOUBLE, result);
}

/**********************************************************************************************************************************/
size_t
cvtIntToZ(int value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%d", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

int
cvtZToIntBase(const char *value, int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    int64_t result = cvtZToInt64Internal(value, "int", base);

    if (result > INT_MAX || result < INT_MIN)
        THROW_FMT(FormatError, "unable to convert base %d string '%s' to int", base, value);

    FUNCTION_TEST_RETURN(INT, (int)result);
}

int
cvtZToInt(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(INT, cvtZToIntBase(value, 10));
}

int
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
    strncpy(buffer, value + offset, size);
    buffer[size] = '\0';

    FUNCTION_TEST_RETURN(INT, cvtZToIntBase(buffer, base));
}

/**********************************************************************************************************************************/
size_t
cvtInt64ToZ(int64_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT64, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%" PRId64, value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

int64_t
cvtZToInt64Base(const char *value, int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(INT64, cvtZToInt64Internal(value, "int64", base));
}

int64_t
cvtZToInt64(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(INT64, cvtZToInt64Base(value, 10));
}

int64_t
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
    strncpy(buffer, value + offset, size);
    buffer[size] = '\0';

    FUNCTION_TEST_RETURN(INT64, cvtZToInt64Base(buffer, base));
}

/**********************************************************************************************************************************/
size_t
cvtModeToZ(mode_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MODE, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%04o", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

mode_t
cvtZToMode(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(MODE, (mode_t)cvtZToUIntBase(value, 8));
}

/**********************************************************************************************************************************/
size_t
cvtSizeToZ(size_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%zu", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

size_t
cvtSSizeToZ(ssize_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SSIZE, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%zd", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

/**********************************************************************************************************************************/
size_t
cvtTimeToZ(time_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TIME, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    struct tm timePart;
    size_t result = strftime(buffer, bufferSize, "%s", localtime_r(&value, &timePart));

    if (result == 0)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

/**********************************************************************************************************************************/
size_t
cvtUIntToZ(unsigned int value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%u", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

unsigned int
cvtZToUIntBase(const char *value, int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    uint64_t result = cvtZToUInt64Internal(value, "unsigned int", base);

    // Don't allow negative numbers even though strtoull() does and check max value
    if (*value == '-' || result > UINT_MAX)
        THROW_FMT(FormatError, "unable to convert base %d string '%s' to unsigned int", base, value);

    FUNCTION_TEST_RETURN(UINT, (unsigned int)result);
}

unsigned int
cvtZToUInt(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(UINT, cvtZToUIntBase(value, 10));
}

unsigned int
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
    strncpy(buffer, value + offset, size);
    buffer[size] = '\0';

    FUNCTION_TEST_RETURN(UINT, cvtZToUIntBase(buffer, base));
}

/**********************************************************************************************************************************/
size_t
cvtUInt64ToZ(uint64_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, value);
        FUNCTION_TEST_PARAM_P(CHARDATA, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    size_t result = (size_t)snprintf(buffer, bufferSize, "%" PRIu64, value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RETURN(SIZE, result);
}

uint64_t
cvtZToUInt64Base(const char *value, int base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    uint64_t result = cvtZToUInt64Internal(value, "uint64", base);

    // Don't allow negative numbers even though strtoull() does
    if (*value == '-')
        THROW_FMT(FormatError, "unable to convert base %d string '%s' to uint64", base, value);

    FUNCTION_TEST_RETURN(UINT64, result);
}

uint64_t
cvtZToUInt64(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(value != NULL);

    FUNCTION_TEST_RETURN(UINT64, cvtZToUInt64Base(value, 10));
}

uint64_t
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
    strncpy(buffer, value + offset, size);
    buffer[size] = '\0';

    FUNCTION_TEST_RETURN(UINT64, cvtZToUInt64Base(buffer, base));
}

/**********************************************************************************************************************************/
void
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

uint64_t
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
