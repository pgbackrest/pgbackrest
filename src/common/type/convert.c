/***********************************************************************************************************************************
Convert Base Data Types
***********************************************************************************************************************************/
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/type/convert.h"

/***********************************************************************************************************************************
Check results of strto*() function for:
    * leading/trailing spaces
    * invalid characters
    * blank string
    * error in errno
***********************************************************************************************************************************/
static void
cvtZToIntValid(int errNo, const char *value, const char *endPtr, const char *type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, errNo);
        FUNCTION_TEST_PARAM(CHARP, value);
        FUNCTION_TEST_PARAM(CHARP, endPtr);
        FUNCTION_TEST_PARAM(CHARP, type);

        FUNCTION_TEST_ASSERT(value != NULL);
        FUNCTION_TEST_ASSERT(endPtr != NULL);
    FUNCTION_TEST_END();

    if (errNo != 0 || *value == '\0' || isspace(*value) || *endPtr != '\0')
        THROW_FMT(FormatError, "unable to convert string '%s' to %s", value, type);

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Convert zero-terminated string to int64 and validate result
***********************************************************************************************************************************/
static int64_t
cvtZToInt64Internal(const char *value, const char *type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHARP, value);
        FUNCTION_TEST_PARAM(CHARP, type);

        FUNCTION_TEST_ASSERT(value != NULL);
        FUNCTION_TEST_ASSERT(type != NULL);
    FUNCTION_TEST_END();

    // Convert from string
    errno = 0;
    char *endPtr = NULL;
    int64_t result = strtoll(value, &endPtr, 10);

    // Validate the result
    cvtZToIntValid(errno, value, endPtr, type);

    FUNCTION_TEST_RESULT(INT64, result);
}

/***********************************************************************************************************************************
Convert zero-terminated string to uint64 and validate result
***********************************************************************************************************************************/
static uint64_t
cvtZToUInt64Internal(const char *value, const char *type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHARP, value);
        FUNCTION_TEST_PARAM(CHARP, type);

        FUNCTION_TEST_ASSERT(value != NULL);
        FUNCTION_TEST_ASSERT(type != NULL);
    FUNCTION_TEST_END();

    // Convert from string
    errno = 0;
    char *endPtr = NULL;
    uint64_t result = strtoull(value, &endPtr, 10);

    // Validate the result
    cvtZToIntValid(errno, value, endPtr, type);

    FUNCTION_TEST_RESULT(UINT64, result);
}

/***********************************************************************************************************************************
Convert boolean to zero-terminated string
***********************************************************************************************************************************/
size_t
cvtBoolToZ(bool value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, value);
        FUNCTION_TEST_PARAM(CHARP, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_TEST_END();

    size_t result = (size_t)snprintf(buffer, bufferSize, "%s", cvtBoolToConstZ(value));

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RESULT(SIZE, result);
}

// Since booleans only have two possible values we can return a const with the value.  This is useful when a boolean needs to be
// output as part of a large string.
const char *
cvtBoolToConstZ(bool value)
{
    return value ? "true" : "false";
}

/***********************************************************************************************************************************
Convert char to zero-terminated string
***********************************************************************************************************************************/
size_t
cvtCharToZ(char value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, value);
        FUNCTION_TEST_PARAM(CHARP, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_TEST_END();

    size_t result = (size_t)snprintf(buffer, bufferSize, "%c", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RESULT(SIZE, result);
}

/***********************************************************************************************************************************
Convert double to zero-terminated string and vice versa
***********************************************************************************************************************************/
size_t
cvtDoubleToZ(double value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DOUBLE, value);
        FUNCTION_TEST_PARAM(CHARP, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_TEST_END();

    // Convert to a string
    size_t result = (size_t)snprintf(buffer, bufferSize, "%lf", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    // Any formatted double should be at least 8 characters, i.e. 0.000000
    ASSERT_DEBUG(strlen(buffer) >= 8);
    // Any formatted double should have a decimal point
    ASSERT_DEBUG(strchr(buffer, '.') != NULL);

    // Strip off any final 0s and the decimal point if there are no non-zero digits after it
    char *end = buffer + strlen(buffer) - 1;

    while (*end == '0' || *end == '.')
    {
        // It should not be possible to go past the beginning because format "%lf" will always write a decimal point
        ASSERT_DEBUG(end > buffer);

        end--;

        if (*(end + 1) == '.')
            break;
    }

    // Zero terminate the string
    end[1] = 0;

    // Return string length
    FUNCTION_TEST_RESULT(SIZE, (size_t)(end - buffer + 1));
}

double
cvtZToDouble(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHARP, value);

        FUNCTION_TEST_ASSERT(value != NULL);
    FUNCTION_TEST_END();

    double result = 0;
    sscanf(value, "%lf", &result);

    if (result == 0 && strcmp(value, "0") != 0)
        THROW_FMT(FormatError, "unable to convert string '%s' to double", value);

    FUNCTION_TEST_RESULT(DOUBLE, result);
}

/***********************************************************************************************************************************
Convert int to zero-terminated string and vice versa
***********************************************************************************************************************************/
size_t
cvtIntToZ(int value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, value);
        FUNCTION_TEST_PARAM(CHARP, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_TEST_END();

    size_t result = (size_t)snprintf(buffer, bufferSize, "%d", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RESULT(SIZE, result);
}

int
cvtZToInt(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHARP, value);

        FUNCTION_TEST_ASSERT(value != NULL);
    FUNCTION_TEST_END();

    int64_t result = cvtZToInt64Internal(value, "int");

    if (result > INT_MAX || result < INT_MIN)
        THROW_FMT(FormatError, "unable to convert string '%s' to int", value);

    FUNCTION_TEST_RESULT(INT, (int)result);
}

/***********************************************************************************************************************************
Convert int64 to zero-terminated string and vice versa
***********************************************************************************************************************************/
size_t
cvtInt64ToZ(int64_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT64, value);
        FUNCTION_TEST_PARAM(CHARP, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_TEST_END();

    size_t result = (size_t)snprintf(buffer, bufferSize, "%" PRId64, value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RESULT(SIZE, result);
}

int64_t
cvtZToInt64(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHARP, value);

        FUNCTION_TEST_ASSERT(value != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(INT64, cvtZToInt64Internal(value, "int64"));
}

/***********************************************************************************************************************************
Convert mode to zero-terminated string
***********************************************************************************************************************************/
size_t
cvtModeToZ(mode_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MODE, value);
        FUNCTION_TEST_PARAM(CHARP, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_TEST_END();

    size_t result = (size_t)snprintf(buffer, bufferSize, "%04o", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RESULT(SIZE, result);
}

/***********************************************************************************************************************************
Convert size to zero-terminated string
***********************************************************************************************************************************/
size_t
cvtSizeToZ(size_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, value);
        FUNCTION_TEST_PARAM(CHARP, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_TEST_END();

    size_t result = (size_t)snprintf(buffer, bufferSize, "%zu", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RESULT(SIZE, result);
}

/***********************************************************************************************************************************
Convert uint to zero-terminated string
***********************************************************************************************************************************/
size_t
cvtUIntToZ(unsigned int value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, value);
        FUNCTION_TEST_PARAM(CHARP, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_TEST_END();

    size_t result = (size_t)snprintf(buffer, bufferSize, "%u", value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RESULT(SIZE, result);
}

unsigned int
cvtZToUInt(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHARP, value);

        FUNCTION_TEST_ASSERT(value != NULL);
    FUNCTION_TEST_END();

    uint64_t result = cvtZToUInt64Internal(value, "unsigned int");

    // Don't allow negative numbers even though strtoull() does and check max value
    if (*value == '-' || result > UINT_MAX)
        THROW_FMT(FormatError, "unable to convert string '%s' to unsigned int", value);

    FUNCTION_TEST_RESULT(UINT, (unsigned int)result);
}

/***********************************************************************************************************************************
Convert uint64 to zero-terminated string and visa versa
***********************************************************************************************************************************/
size_t
cvtUInt64ToZ(uint64_t value, char *buffer, size_t bufferSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, value);
        FUNCTION_TEST_PARAM(CHARP, buffer);
        FUNCTION_TEST_PARAM(SIZE, bufferSize);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_TEST_END();

    size_t result = (size_t)snprintf(buffer, bufferSize, "%" PRIu64, value);

    if (result >= bufferSize)
        THROW(AssertError, "buffer overflow");

    FUNCTION_TEST_RESULT(SIZE, result);
}

uint64_t
cvtZToUInt64(const char *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHARP, value);

        FUNCTION_TEST_ASSERT(value != NULL);
    FUNCTION_TEST_END();

    uint64_t result = cvtZToUInt64Internal(value, "uint64");

    // Don't allow negative numbers even though strtoull() does
    if (*value == '-')
        THROW_FMT(FormatError, "unable to convert string '%s' to uint64", value);

    FUNCTION_TEST_RESULT(UINT64, result);
}
