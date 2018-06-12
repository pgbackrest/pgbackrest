/***********************************************************************************************************************************
Test Convert Base Data Types
***********************************************************************************************************************************/
#include "common/stackTrace.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cvtBoolToZ()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtBoolToZ(true, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_INT(cvtBoolToZ(true, buffer, STACK_TRACE_PARAM_MAX), 4, "convert true bool to string");
        TEST_RESULT_STR(buffer, "true", "    check buffer");
        TEST_RESULT_INT(cvtBoolToZ(false, buffer, STACK_TRACE_PARAM_MAX), 5, "convert false bool to string");
        TEST_RESULT_STR(buffer, "false", "    check buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtDoubleToZ() and cvtZToDouble()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtDoubleToZ(999.1234, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_INT(cvtDoubleToZ(999.1234, buffer, STACK_TRACE_PARAM_MAX), 8, "convert double to string");
        TEST_RESULT_STR(buffer, "999.1234", "    check buffer");

        TEST_RESULT_INT(cvtDoubleToZ(999999999.123456, buffer, STACK_TRACE_PARAM_MAX), 16, "convert double to string");
        TEST_RESULT_STR(buffer, "999999999.123456", "    check buffer");

        TEST_RESULT_INT(cvtDoubleToZ(999.0, buffer, STACK_TRACE_PARAM_MAX), 3, "convert double to string");
        TEST_RESULT_STR(buffer, "999", "    check buffer");

        TEST_ERROR(cvtZToDouble("AAA"), FormatError, "unable to convert string 'AAA' to double");
        TEST_RESULT_DOUBLE(cvtZToDouble("0"), 0, "convert string to double");
        TEST_RESULT_DOUBLE(cvtZToDouble("123.123"), 123.123, "convert string to double");
        TEST_RESULT_DOUBLE(cvtZToDouble("-999999999.123456"), -999999999.123456, "convert string to double");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtIntToZ() and cvtZToInt()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtIntToZ(9999, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_INT(cvtIntToZ(1234567890, buffer, STACK_TRACE_PARAM_MAX), 10, "convert int to string");
        TEST_RESULT_STR(buffer, "1234567890", "    check buffer");

        TEST_ERROR(cvtZToInt("FEF"), FormatError, "unable to convert string 'FEF' to int");
        TEST_RESULT_DOUBLE(cvtZToInt("0"), 0, "convert string to int");
        TEST_RESULT_DOUBLE(cvtZToInt("1234567890"), 1234567890, "convert string to int");
        TEST_RESULT_DOUBLE(cvtZToInt("-1234567890"), -1234567890, "convert string to int");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtModeToZ()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtModeToZ(0750, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_INT(cvtModeToZ(0777, buffer, STACK_TRACE_PARAM_MAX), 4, "convert mode to string");
        TEST_RESULT_STR(buffer, "0777", "    check buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtSizeToZ()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtSizeToZ(9999, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_INT(cvtSizeToZ(4294967295, buffer, STACK_TRACE_PARAM_MAX), 10, "convert size to string");
        TEST_RESULT_STR(buffer, "4294967295", "    check buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtUIntToZ()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtUIntToZ(9999, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_INT(cvtUIntToZ(4294967295, buffer, STACK_TRACE_PARAM_MAX), 10, "convert uint to string");
        TEST_RESULT_STR(buffer, "4294967295", "    check buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtInt64ToZ() and cvtZToInt64()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtInt64ToZ(9999, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_INT(cvtInt64ToZ(9223372036854775807, buffer, STACK_TRACE_PARAM_MAX), 19, "convert int64 to string");
        TEST_RESULT_STR(buffer, "9223372036854775807", "    check buffer");

        TEST_ERROR(cvtZToInt64("FEF"), FormatError, "unable to convert string 'FEF' to int64");
        TEST_RESULT_INT(cvtZToInt64("0"), 0, "convert string to int64");
        TEST_RESULT_INT(cvtZToInt64("9223372036854775807"), 9223372036854775807, "convert string to int64");
        TEST_RESULT_INT(cvtZToInt64("-9223372036854775807"), -9223372036854775807, "convert string to int64");
    }

    // *****************************************************************************************************************************
    if (testBegin("UInt64ToZ() and cvtZToUInt64()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtUInt64ToZ(9999, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_INT(cvtUInt64ToZ(0xFFFFFFFFFFFFFFFF, buffer, STACK_TRACE_PARAM_MAX), 20, "convert uint64 to string");
        TEST_RESULT_STR(buffer, "18446744073709551615", "    check buffer");

        TEST_ERROR(cvtZToUInt64("FEF"), FormatError, "unable to convert string 'FEF' to uint64");
        TEST_ERROR(cvtZToUInt64(" 10"), FormatError, "unable to convert string ' 10' to uint64"); // number but leading space
        TEST_ERROR(cvtZToUInt64("10 "), FormatError, "unable to convert string '10 ' to uint64"); // number but trailing space
        TEST_ERROR(cvtZToUInt64("-1"), FormatError, "unable to convert string '-1' to uint64"); // number but trailing space

        TEST_RESULT_DOUBLE(cvtZToUInt64("18446744073709551615"), 18446744073709551615U, "convert string to uint64");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
