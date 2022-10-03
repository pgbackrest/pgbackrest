/***********************************************************************************************************************************
Test Convert C Types
***********************************************************************************************************************************/
#include "common/stackTrace.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cvtBoolToZ()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtBoolToZ(true, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_UINT(cvtBoolToZ(true, buffer, STACK_TRACE_PARAM_MAX), 4, "convert true bool to string");
        TEST_RESULT_Z(buffer, "true", "    check buffer");
        TEST_RESULT_UINT(cvtBoolToZ(false, buffer, STACK_TRACE_PARAM_MAX), 5, "convert false bool to string");
        TEST_RESULT_Z(buffer, "false", "    check buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtCharToZ()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtCharToZ('A', buffer, 1), AssertError, "buffer overflow");

        TEST_RESULT_UINT(cvtCharToZ('C', buffer, STACK_TRACE_PARAM_MAX), 1, "convert char to string");
        TEST_RESULT_Z(buffer, "C", "    check buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtDoubleToZ() and cvtZToDouble()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtDoubleToZ(999.1234, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_UINT(cvtDoubleToZ(999.1234, buffer, STACK_TRACE_PARAM_MAX), 8, "convert double to string");
        TEST_RESULT_Z(buffer, "999.1234", "    check buffer");

        TEST_RESULT_UINT(cvtDoubleToZ(999999999.123456, buffer, STACK_TRACE_PARAM_MAX), 16, "convert double to string");
        TEST_RESULT_Z(buffer, "999999999.123456", "    check buffer");

        TEST_RESULT_UINT(cvtDoubleToZ(999.0, buffer, STACK_TRACE_PARAM_MAX), 3, "convert double to string");
        TEST_RESULT_Z(buffer, "999", "    check buffer");

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

        TEST_RESULT_UINT(cvtIntToZ(1234567890, buffer, STACK_TRACE_PARAM_MAX), 10, "convert int to string");
        TEST_RESULT_Z(buffer, "1234567890", "    check buffer");

        TEST_ERROR(cvtZToInt("FEF"), FormatError, "unable to convert base 10 string 'FEF' to int");
        TEST_ERROR(cvtZToInt("9223372036854775807"), FormatError, "unable to convert base 10 string '9223372036854775807' to int");
        TEST_ERROR(
            cvtZToInt("-9223372036854775807"), FormatError, "unable to convert base 10 string '-9223372036854775807' to int");

        TEST_RESULT_INT(cvtZToIntBase("-FF", 16), -255, "convert string to int");
        TEST_RESULT_INT(cvtZSubNToIntBase("XFFX", 1, 2, 16), 255, "convert string to int");
        TEST_RESULT_INT(cvtZToInt("0"), 0, "convert string to int");
        TEST_RESULT_INT(cvtZToInt("1234567890"), 1234567890, "convert string to int");
        TEST_RESULT_INT(cvtZSubNToInt("X1234567890X", 1, 10), 1234567890, "convert string to int");
        TEST_RESULT_INT(cvtZToInt("-1234567890"), -1234567890, "convert string to int");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtInt32ToZigZag(), cvtInt32FromZigZag(), cvtInt64ToZigZag(), and cvtInt64FromZigZag()"))
    {
        TEST_TITLE("32-bit zigzag");

        TEST_RESULT_UINT(cvtInt32ToZigZag(-1), 1, "-1 to zigzag");
        TEST_RESULT_UINT(cvtInt32ToZigZag(INT32_MIN), UINT32_MAX, "INT32_MIN to zigzag");
        TEST_RESULT_UINT(cvtInt32ToZigZag(INT32_MAX), UINT32_MAX - 1, "INT32_MAX to zigzag");
        TEST_RESULT_INT(cvtInt32FromZigZag(1), -1, "-1 to zigzag");
        TEST_RESULT_INT(cvtInt32FromZigZag(UINT32_MAX), INT32_MIN, "zigzag to INT32_MIN");
        TEST_RESULT_INT(cvtInt32FromZigZag(UINT32_MAX - 1), INT32_MAX, "zigzag to INT32_MAX");

        TEST_TITLE("64-bit zigzag");

        TEST_RESULT_UINT(cvtInt64ToZigZag(-1), 1, "-1 to zigzag");
        TEST_RESULT_UINT(cvtInt64ToZigZag(INT64_MIN), UINT64_MAX, "INT64_MIN to zigzag");
        TEST_RESULT_UINT(cvtInt64ToZigZag(INT64_MAX), UINT64_MAX - 1, "INT64_MAX to zigzag");
        TEST_RESULT_INT(cvtInt64FromZigZag(1), -1, "-1 to zigzag");
        TEST_RESULT_INT(cvtInt64FromZigZag(UINT64_MAX), INT64_MIN, "zigzag to INT64_MIN");
        TEST_RESULT_INT(cvtInt64FromZigZag(UINT64_MAX - 1), INT64_MAX, "zigzag to INT64_MAX");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtModeToZ()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtModeToZ(0750, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_UINT(cvtModeToZ(0777, buffer, STACK_TRACE_PARAM_MAX), 4, "convert mode to string");
        TEST_RESULT_Z(buffer, "0777", "    check buffer");

        TEST_RESULT_UINT(cvtZToMode("0700"), 0700, "convert string to mode");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtSizeToZ() and cvtSSizeToZ()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtSizeToZ(9999, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_UINT(cvtSizeToZ(4294967295, buffer, STACK_TRACE_PARAM_MAX), 10, "convert size to string");
        TEST_RESULT_Z(buffer, "4294967295", "    check buffer");

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(cvtSSizeToZ(-9999, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_UINT(cvtSSizeToZ(-9999, buffer, STACK_TRACE_PARAM_MAX), 5, "convert ssize to string");
        TEST_RESULT_Z(buffer, "-9999", "    check buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtTimeToZ()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtTimeToZ(9999, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_UINT(cvtTimeToZ(1573222014, buffer, STACK_TRACE_PARAM_MAX), 10, "convert time to string");
        TEST_RESULT_Z(buffer, "1573222014", "    check buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtUIntToZ() and cvtZToUInt()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtUIntToZ(9999, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_UINT(cvtUIntToZ(4294967295, buffer, STACK_TRACE_PARAM_MAX), 10, "convert unsigned int to string");
        TEST_RESULT_Z(buffer, "4294967295", "    check buffer");

        TEST_ERROR(cvtZToUInt("-1"), FormatError, "unable to convert base 10 string '-1' to unsigned int");
        TEST_ERROR(cvtZToUInt("5000000000"), FormatError, "unable to convert base 10 string '5000000000' to unsigned int");

        TEST_RESULT_UINT(cvtZToUIntBase("FF", 16), 255, "convert string to unsigned int");
        TEST_RESULT_UINT(cvtZSubNToUIntBase("XFFX", 1, 2, 16), 255, "convert string to unsigned int");
        TEST_RESULT_UINT(cvtZToUInt("3333333333"), 3333333333U, "convert string to unsigned int");
        TEST_RESULT_UINT(cvtZSubNToUInt("X3333333333X", 1, 10), 3333333333U, "convert string to unsigned int");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtInt64ToZ() and cvtZToInt64()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtInt64ToZ(9999, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_UINT(cvtInt64ToZ(9223372036854775807, buffer, STACK_TRACE_PARAM_MAX), 19, "convert int64 to string");
        TEST_RESULT_Z(buffer, "9223372036854775807", "    check buffer");

        TEST_ERROR(cvtZToInt64("123INV"), FormatError, "unable to convert base 10 string '123INV' to int64");
        TEST_ERROR(cvtZToInt64("FEF"), FormatError, "unable to convert base 10 string 'FEF' to int64");
        TEST_ERROR(cvtZToInt64(""), FormatError, "unable to convert base 10 string '' to int64");
        TEST_ERROR(cvtZToInt64(" 1"), FormatError, "unable to convert base 10 string ' 1' to int64");
        TEST_ERROR(cvtZToInt64("\t1"), FormatError, "unable to convert base 10 string '\t1' to int64");
        TEST_ERROR(cvtZToInt64("1\t"), FormatError, "unable to convert base 10 string '1\t' to int64");
        TEST_ERROR(
            cvtZToInt64("9223372036854775808"), FormatError, "unable to convert base 10 string '9223372036854775808' to int64");

        TEST_RESULT_INT(cvtZToInt64Base("-FF", 16), -255, "convert string to int64");
        TEST_RESULT_INT(cvtZSubNToInt64Base("X-FFX", 1, 3, 16), -255, "convert string to int64");
        TEST_RESULT_INT(cvtZToInt64("0"), 0, "convert string to int64");
        TEST_RESULT_INT(cvtZToInt64("9223372036854775807"), 9223372036854775807, "convert string to int64");
        TEST_RESULT_INT(cvtZToInt64("-9223372036854775807"), -9223372036854775807, "convert string to int64");
        TEST_RESULT_INT(cvtZSubNToInt64("X-9223372036854775807X", 1, 20), -9223372036854775807, "convert string to int64");
    }

    // *****************************************************************************************************************************
    if (testBegin("UInt64ToZ() and cvtZToUInt64()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_ERROR(cvtUInt64ToZ(9999, buffer, 4), AssertError, "buffer overflow");

        TEST_RESULT_UINT(cvtUInt64ToZ(0xFFFFFFFFFFFFFFFF, buffer, STACK_TRACE_PARAM_MAX), 20, "convert uint64 to string");
        TEST_RESULT_Z(buffer, "18446744073709551615", "    check buffer");

        TEST_ERROR(cvtZToUInt64("FEF"), FormatError, "unable to convert base 10 string 'FEF' to uint64");
        TEST_ERROR(cvtZToUInt64(" 10"), FormatError, "unable to convert base 10 string ' 10' to uint64");
        TEST_ERROR(cvtZToUInt64("10 "), FormatError, "unable to convert base 10 string '10 ' to uint64");
        TEST_ERROR(cvtZToUInt64("-1"), FormatError, "unable to convert base 10 string '-1' to uint64");

        TEST_RESULT_UINT(cvtZToUInt64Base("FF", 16), 255, "convert string to uint64");
        TEST_RESULT_UINT(cvtZSubNToUInt64Base("XFFX", 1, 2, 16), 255, "convert string to uint64");
        TEST_RESULT_UINT(cvtZToUInt64("18446744073709551615"), 18446744073709551615U, "convert string to uint64");
        TEST_RESULT_UINT(cvtZSubNToUInt64("X18446744073709551615X", 1, 20), 18446744073709551615U, "convert string to uint64");
    }

    // *****************************************************************************************************************************
    if (testBegin("cvtUInt64ToVarInt128() and cvtUInt64FromVarInt128()"))
    {
        uint8_t buffer[CVT_VARINT128_BUFFER_SIZE];
        size_t bufferPos;

        bufferPos = 0;
        TEST_ERROR(cvtUInt64ToVarInt128(9999, buffer, &bufferPos, 1), AssertError, "buffer overflow");

        bufferPos = 0;
        TEST_RESULT_VOID(cvtUInt64ToVarInt128(0xFFFFFFFFFFFFFFFF, buffer, &bufferPos, sizeof(buffer)), "to varint-128");
        TEST_RESULT_UINT(bufferPos, 10, "check buffer position");

        bufferPos = 0;
        TEST_ERROR(cvtUInt64FromVarInt128(buffer, &bufferPos, 1), FormatError, "buffer position is beyond buffer size");

        bufferPos = 0;
        TEST_RESULT_UINT(cvtUInt64FromVarInt128(buffer, &bufferPos, sizeof(buffer)), 0xFFFFFFFFFFFFFFFF, "to uint64");
        TEST_RESULT_UINT(bufferPos, 10, "check buffer position");

        bufferPos = 0;
        TEST_RESULT_VOID(cvtUInt64ToVarInt128(0, buffer, &bufferPos, sizeof(buffer)), "to varint-128");
        TEST_RESULT_UINT(bufferPos, 1, "check buffer position");

        bufferPos = 0;
        TEST_RESULT_UINT(cvtUInt64FromVarInt128(buffer, &bufferPos, sizeof(buffer)), 0, "to uint64");
        TEST_RESULT_UINT(bufferPos, 1, "check buffer position");

        uint8_t buffer2[CVT_VARINT128_BUFFER_SIZE + 1];
        memset(buffer2, 0xFF, sizeof(buffer2));
        bufferPos = 0;
        TEST_ERROR(cvtUInt64FromVarInt128(buffer2, &bufferPos, sizeof(buffer)), FormatError, "unterminated varint-128 integer");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
