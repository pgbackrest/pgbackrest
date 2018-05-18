/***********************************************************************************************************************************
Convert Base Data Types
***********************************************************************************************************************************/
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/type/convert.h"

/***********************************************************************************************************************************
Convert uint64 to zero-terminated string
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

    size_t result = (size_t)snprintf(buffer, bufferSize, "%s", value ? "true" : "false");

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

    int result = atoi(value);

    if (result == 0 && strcmp(value, "0") != 0)
        THROW_FMT(FormatError, "unable to convert string '%s' to int", value);

    FUNCTION_TEST_RESULT(INT, result);
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

    int64_t result = atoll(value);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%" PRId64, result);

    if (strcmp(value, buffer) != 0)
        THROW_FMT(FormatError, "unable to convert string '%s' to int64", value);

    FUNCTION_TEST_RESULT(INT64, result);
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

/***********************************************************************************************************************************
Convert uint64 to zero-terminated string
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
