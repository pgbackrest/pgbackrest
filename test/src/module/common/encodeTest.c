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

        TEST_ERROR(
            decodeToBin(encodeBase64, "cc$=", destinationDecode), FormatError, "base64 invalid character found at position 2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(decodeToBin(encodeBase64, "c3", destinationDecode), FormatError, "base64 size 2 is not evenly divisible by 4");
        TEST_ERROR(
            decodeToBin(encodeBase64, "c===", destinationDecode), FormatError,
            "base64 '=' character may only appear in last two positions");
        TEST_ERROR(
            decodeToBin(encodeBase64, "cc=c", destinationDecode), FormatError,
            "base64 last character must be '=' if second to last is");
    }

    // *****************************************************************************************************************************
    if (testBegin("base64url"))
    {
        TEST_TITLE("encode");

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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("decode unsupported");

        unsigned char destinationDecode[256];

        TEST_ERROR(decodeToBinSize(encodeBase64Url, "c3"), AssertError, "unsupported");
        TEST_ERROR(decodeToBin(encodeBase64Url, "c3", destinationDecode), AssertError, "unsupported");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
