/***********************************************************************************************************************************
Test Binary to String Encode/Decode
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("base64"))
    {
        char *source = "string_to_encode\r\n";
        unsigned char destination[256];

        encodeToStr(encodeBase64, source, 1, destination);
        TEST_RESULT_STR(destination, "c3==", "1 character encode");
        TEST_RESULT_INT(encodeToStrSize(encodeBase64, 1), strlen(destination), "check size");

        encodeToStr(encodeBase64, source, 2, destination);
        TEST_RESULT_STR(destination, "c3R=", "2 character encode");
        TEST_RESULT_INT(encodeToStrSize(encodeBase64, 2), strlen(destination), "check size");

        encodeToStr(encodeBase64, source, 3, destination);
        TEST_RESULT_STR(destination, "c3Ry", "3 character encode");
        TEST_RESULT_INT(encodeToStrSize(encodeBase64, 3), strlen(destination), "check size");

        encodeToStr(encodeBase64, source, strlen(source) - 2, destination);
        TEST_RESULT_STR(destination, "c3RyaW5nX3RvX2VuY29kZQ==", "encode full string");
        TEST_RESULT_INT(encodeToStrSize(encodeBase64, strlen(source) - 2), strlen(destination), "check size");

        encodeToStr(encodeBase64, source, strlen(source), destination);
        TEST_RESULT_STR(destination, "c3RyaW5nX3RvX2VuY29kZQ0K", "encode full string with \\r\\n");
        TEST_RESULT_INT(encodeToStrSize(encodeBase64, strlen(source)), strlen(destination), "check size");

        encodeToStr(encodeBase64, source, strlen(source) + 1, destination);
        TEST_RESULT_STR(destination, "c3RyaW5nX3RvX2VuY29kZQ0KAG==", "encode full string with \\r\\n and null");
        TEST_RESULT_INT(encodeToStrSize(encodeBase64, strlen(source) + 1), strlen(destination), "check size");

        TEST_ERROR(encodeToStr(999, source, strlen(source), destination), AssertError, "invalid encode type 999");
        TEST_ERROR(encodeToStrSize(999, strlen(source)), AssertError, "invalid encode type 999");

        // -------------------------------------------------------------------------------------------------------------------------
        memset(destination, 0xFF, sizeof(destination));
        char *decode = "c3RyaW5nX3RvX2VuY29kZQ0KAG==";
        decodeToBin(encodeBase64, decode, destination);
        TEST_RESULT_STR(destination, source, "full string with \\r\\n and null decode");
        TEST_RESULT_INT(destination[strlen(source) + 1], 0xFF, "check for overrun");
        TEST_RESULT_INT(decodeToBinSize(encodeBase64, decode), strlen(source) + 1, "check size");

        memset(destination, 0xFF, sizeof(destination));
        decode = "c3RyaW5nX3RvX2VuY29kZQ0K";
        decodeToBin(encodeBase64, decode, destination);
        TEST_RESULT_INT(memcmp(destination, source, strlen(source)), 0, "full string with \\r\\n decode");
        TEST_RESULT_INT(destination[strlen(source)], 0xFF, "check for overrun");
        TEST_RESULT_INT(decodeToBinSize(encodeBase64, decode), strlen(source), "check size");

        memset(destination, 0xFF, sizeof(destination));
        decode = "c3RyaW5nX3RvX2VuY29kZQ==";
        decodeToBin(encodeBase64, decode, destination);
        TEST_RESULT_INT(memcmp(destination, source, strlen(source) - 2), 0, "full string decode");
        TEST_RESULT_INT(destination[strlen(source) - 2], 0xFF, "check for overrun");
        TEST_RESULT_INT(decodeToBinSize(encodeBase64, decode), strlen(source) - 2, "check size");

        memset(destination, 0xFF, sizeof(destination));
        decode = "c3Ry";
        decodeToBin(encodeBase64, decode, destination);
        TEST_RESULT_INT(memcmp(destination, source, 3), 0, "3 character decode");
        TEST_RESULT_INT(destination[3], 0xFF, "check for overrun");
        TEST_RESULT_INT(decodeToBinSize(encodeBase64, decode), 3, "check size");

        memset(destination, 0xFF, sizeof(destination));
        decode = "c3R=";
        decodeToBin(encodeBase64, decode, destination);
        TEST_RESULT_INT(memcmp(destination, source, 2), 0, "2 character decode");
        TEST_RESULT_INT(destination[2], 0xFF, "check for overrun");
        TEST_RESULT_INT(decodeToBinSize(encodeBase64, decode), 2, "check size");

        memset(destination, 0xFF, sizeof(destination));
        decode = "c3==";
        decodeToBin(encodeBase64, decode, destination);
        TEST_RESULT_INT(memcmp(destination, source, 1), 0, "1 character decode");
        TEST_RESULT_INT(destination[1], 0xFF, "check for overrun");
        TEST_RESULT_INT(decodeToBinSize(encodeBase64, decode), 1, "check size");

        TEST_ERROR(decodeToBin(-1, decode, destination), AssertError, "invalid encode type -1");
        TEST_ERROR(decodeToBinSize(-1, decode), AssertError, "invalid encode type -1");
        TEST_ERROR(decodeToBin(encodeBase64, "cc$=", destination), FormatError, "base64 invalid character found at position 2");

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

        TEST_ERROR(decodeToBinValid(-999, "CCCCCCCCCCCC"), AssertError, "invalid encode type -999");
    }
}
