/***********************************************************************************************************************************
Test Binary to String Encode/Decode
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("base64"))
    {
        const unsigned char *encode = (const unsigned char *)"string_to_encode\r\n";
        char destinationEncode[256];

        encodeToStr(encodeBase64, encode, 1, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "cw==", "1 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64, 1), strlen(destinationEncode), "check size");

        encodeToStr(encodeBase64, encode, 2, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3Q=", "2 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64, 2), strlen(destinationEncode), "check size");

        encodeToStr(encodeBase64, encode, 3, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3Ry", "3 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64, 3), strlen(destinationEncode), "check size");

        encodeToStr(encodeBase64, encode, strlen((char *)encode) - 2, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ==", "encode full string");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64, strlen((char *)encode) - 2), strlen(destinationEncode), "check size");

        encodeToStr(encodeBase64, encode, strlen((char *)encode), destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ0K", "encode full string with \\r\\n");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64, strlen((char *)encode)), strlen(destinationEncode), "check size");

        encodeToStr(encodeBase64, encode, strlen((char *)encode) + 1, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ0KAA==", "encode full string with \\r\\n and null");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64, strlen((char *)encode) + 1), strlen(destinationEncode), "check size");

        TEST_ERROR(encodeToStr(999, encode, strlen((char *)encode), destinationEncode), AssertError, "invalid encode type 999");
        TEST_ERROR(encodeToStrSize(999, strlen((char *)encode)), AssertError, "invalid encode type 999");

        // -------------------------------------------------------------------------------------------------------------------------
        unsigned char destinationDecode[256];

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        const char *decode = "c3RyaW5nX3RvX2VuY29kZQ0KAA==";
        decodeToBin(encodeBase64, decode, destinationDecode);
        TEST_RESULT_Z((char *)destinationDecode, (char *)encode, "full string with \\r\\n and null decode");
        TEST_RESULT_INT(destinationDecode[strlen((char *)encode) + 1], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodeBase64, decode), strlen((char *)encode) + 1, "check size");

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        decode = "c3RyaW5nX3RvX2VuY29kZQ0K";
        decodeToBin(encodeBase64, decode, destinationDecode);
        TEST_RESULT_INT(memcmp(destinationDecode, encode, strlen((char *)encode)), 0, "full string with \\r\\n decode");
        TEST_RESULT_INT(destinationDecode[strlen((char *)encode)], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodeBase64, decode), strlen((char *)encode), "check size");

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        decode = "c3RyaW5nX3RvX2VuY29kZQ==";
        decodeToBin(encodeBase64, decode, destinationDecode);
        TEST_RESULT_INT(memcmp(destinationDecode, encode, strlen((char *)encode) - 2), 0, "full string decode");
        TEST_RESULT_INT(destinationDecode[strlen((char *)encode) - 2], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodeBase64, decode), strlen((char *)encode) - 2, "check size");

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        decode = "c3Ry";
        decodeToBin(encodeBase64, decode, destinationDecode);
        TEST_RESULT_INT(memcmp(destinationDecode, encode, 3), 0, "3 character decode");
        TEST_RESULT_INT(destinationDecode[3], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodeBase64, decode), 3, "check size");

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        decode = "c3Q=";
        decodeToBin(encodeBase64, decode, destinationDecode);
        TEST_RESULT_INT(memcmp(destinationDecode, encode, 2), 0, "2 character decode");
        TEST_RESULT_INT(destinationDecode[2], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodeBase64, decode), 2, "check size");

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        decode = "cw==";
        decodeToBin(encodeBase64, decode, destinationDecode);
        TEST_RESULT_INT(memcmp(destinationDecode, encode, 1), 0, "1 character decode");
        TEST_RESULT_INT(destinationDecode[1], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodeBase64, decode), 1, "check size");

        TEST_ERROR(decodeToBin(9999, decode, destinationDecode), AssertError, "invalid encode type 9999");
        TEST_ERROR(decodeToBinSize(9999, decode), AssertError, "invalid encode type 9999");
        TEST_ERROR(decodeToBin(encodeBase64, "cc$=", destinationDecode), FormatError, "base64 invalid character found at position 2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(decodeToBinValidate(encodeBase64, "c3"), FormatError, "base64 size 2 is not evenly divisible by 4");
        TEST_ERROR(
            decodeToBinValidate(encodeBase64, "c==="), FormatError, "base64 '=' character may only appear in last two positions");
        TEST_ERROR(
            decodeToBinValidate(encodeBase64, "cc=c"), FormatError, "base64 last character must be '=' if second to last is");

        TEST_ERROR(decodeToBinValidate(9999, "cc=c"), AssertError, "invalid encode type 9999");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(decodeToBinValid(encodeBase64, "CCCCCCCCCCC"), false, "base64 string not valid");
        TEST_RESULT_BOOL(decodeToBinValid(encodeBase64, "CCCCCCCCCCCC"), true, "base64 string valid");

        TEST_ERROR(decodeToBinValid(9999, "CCCCCCCCCCCC"), AssertError, "invalid encode type 9999");
    }

    // *****************************************************************************************************************************
    if (testBegin("base64url"))
    {
        const unsigned char *encode = (const unsigned char *)"string_to_encode\r\n";
        char destinationEncode[256];

        encodeToStr(encodeBase64Url, encode, 1, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "cw", "1 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64Url, 1), strlen(destinationEncode), "check size");

        encodeToStr(encodeBase64Url, encode, 2, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3Q", "2 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64Url, 2), strlen(destinationEncode), "check size");

        encodeToStr(encodeBase64Url, encode, 3, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3Ry", "3 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64Url, 3), strlen(destinationEncode), "check size");

        encodeToStr(encodeBase64Url, encode, strlen((char *)encode) - 2, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ", "encode full string");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64Url, strlen((char *)encode) - 2), strlen(destinationEncode), "check size");

        encodeToStr(encodeBase64Url, encode, strlen((char *)encode), destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ0K", "encode full string with \\r\\n");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64Url, strlen((char *)encode)), strlen(destinationEncode), "check size");

        encodeToStr(encodeBase64Url, encode, strlen((char *)encode) + 1, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ0KAA", "encode full string with \\r\\n and null");
        TEST_RESULT_UINT(encodeToStrSize(encodeBase64Url, strlen((char *)encode) + 1), strlen(destinationEncode), "check size");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
