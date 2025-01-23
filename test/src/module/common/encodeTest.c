/***********************************************************************************************************************************
Test Binary to String Encode/Decode
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("base64"))
    {
        const unsigned char *encode = (const unsigned char *)"string_to_encode\r\n";
        char destinationEncode[256];

        TEST_RESULT_UINT(encodeToStrSize(encodingBase64, 0), 0, "check zero size");

        encodeToStr(encodingBase64, encode, 1, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "cw==", "1 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodingBase64, 1), strlen(destinationEncode), "check size");

        encodeToStr(encodingBase64, encode, 2, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3Q=", "2 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodingBase64, 2), strlen(destinationEncode), "check size");

        encodeToStr(encodingBase64, encode, 3, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3Ry", "3 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodingBase64, 3), strlen(destinationEncode), "check size");

        encodeToStr(encodingBase64, encode, strlen((const char *)encode) - 2, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ==", "encode full string");
        TEST_RESULT_UINT(
            encodeToStrSize(encodingBase64, strlen((const char *)encode) - 2), strlen(destinationEncode), "check size");

        encodeToStr(encodingBase64, encode, strlen((const char *)encode), destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ0K", "encode full string with \\r\\n");
        TEST_RESULT_UINT(encodeToStrSize(encodingBase64, strlen((const char *)encode)), strlen(destinationEncode), "check size");

        encodeToStr(encodingBase64, encode, strlen((const char *)encode) + 1, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ0KAA==", "encode full string with \\r\\n and null");
        TEST_RESULT_UINT(
            encodeToStrSize(encodingBase64, strlen((const char *)encode) + 1), strlen(destinationEncode), "check size");

        // -------------------------------------------------------------------------------------------------------------------------
        unsigned char destinationDecode[256];

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        const char *decode = "c3RyaW5nX3RvX2VuY29kZQ0KAA==";
        decodeToBin(encodingBase64, decode, destinationDecode);
        TEST_RESULT_Z((char *)destinationDecode, (const char *)encode, "full string with \\r\\n and null decode");
        TEST_RESULT_INT(destinationDecode[strlen((const char *)encode) + 1], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodingBase64, decode), strlen((const char *)encode) + 1, "check size");

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        decode = "c3RyaW5nX3RvX2VuY29kZQ0K";
        decodeToBin(encodingBase64, decode, destinationDecode);
        TEST_RESULT_INT(memcmp(destinationDecode, encode, strlen((const char *)encode)), 0, "full string with \\r\\n decode");
        TEST_RESULT_INT(destinationDecode[strlen((const char *)encode)], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodingBase64, decode), strlen((const char *)encode), "check size");

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        decode = "c3RyaW5nX3RvX2VuY29kZQ==";
        decodeToBin(encodingBase64, decode, destinationDecode);
        TEST_RESULT_INT(memcmp(destinationDecode, encode, strlen((const char *)encode) - 2), 0, "full string decode");
        TEST_RESULT_INT(destinationDecode[strlen((const char *)encode) - 2], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodingBase64, decode), strlen((const char *)encode) - 2, "check size");

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        decode = "c3Ry";
        decodeToBin(encodingBase64, decode, destinationDecode);
        TEST_RESULT_INT(memcmp(destinationDecode, encode, 3), 0, "3 character decode");
        TEST_RESULT_INT(destinationDecode[3], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodingBase64, decode), 3, "check size");

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        decode = "c3Q=";
        decodeToBin(encodingBase64, decode, destinationDecode);
        TEST_RESULT_INT(memcmp(destinationDecode, encode, 2), 0, "2 character decode");
        TEST_RESULT_INT(destinationDecode[2], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodingBase64, decode), 2, "check size");

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        decode = "cw==";
        decodeToBin(encodingBase64, decode, destinationDecode);
        TEST_RESULT_INT(memcmp(destinationDecode, encode, 1), 0, "1 character decode");
        TEST_RESULT_INT(destinationDecode[1], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodingBase64, decode), 1, "check size");

        TEST_ERROR(
            decodeToBin(encodingBase64, "cc$=", destinationDecode), FormatError, "base64 invalid character found at position 2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(decodeToBin(encodingBase64, "c3", destinationDecode), FormatError, "base64 size 2 is not evenly divisible by 4");
        TEST_ERROR(
            decodeToBin(encodingBase64, "c===", destinationDecode), FormatError,
            "base64 '=' character may only appear in last two positions");
        TEST_ERROR(
            decodeToBin(encodingBase64, "cc=c", destinationDecode), FormatError,
            "base64 last character must be '=' if second to last is");
    }

    // *****************************************************************************************************************************
    if (testBegin("base64url"))
    {
        TEST_TITLE("encode");

        const unsigned char *encode = (const unsigned char *)"string_to_encode\r\n";
        char destinationEncode[256];

        TEST_RESULT_UINT(encodeToStrSize(encodingBase64Url, 0), 0, "check zero size");

        encodeToStr(encodingBase64Url, encode, 1, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "cw", "1 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodingBase64Url, 1), strlen(destinationEncode), "check size");

        encodeToStr(encodingBase64Url, encode, 2, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3Q", "2 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodingBase64Url, 2), strlen(destinationEncode), "check size");

        encodeToStr(encodingBase64Url, encode, 3, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3Ry", "3 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodingBase64Url, 3), strlen(destinationEncode), "check size");

        encodeToStr(encodingBase64Url, encode, strlen((const char *)encode) - 2, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ", "encode full string");
        TEST_RESULT_UINT(
            encodeToStrSize(encodingBase64Url, strlen((const char *)encode) - 2), strlen(destinationEncode), "check size");

        encodeToStr(encodingBase64Url, encode, strlen((const char *)encode), destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ0K", "encode full string with \\r\\n");
        TEST_RESULT_UINT(encodeToStrSize(encodingBase64Url, strlen((const char *)encode)), strlen(destinationEncode), "check size");

        encodeToStr(encodingBase64Url, encode, strlen((const char *)encode) + 1, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "c3RyaW5nX3RvX2VuY29kZQ0KAA", "encode full string with \\r\\n and null");
        TEST_RESULT_UINT(
            encodeToStrSize(encodingBase64Url, strlen((const char *)encode) + 1), strlen(destinationEncode), "check size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("decode unsupported");

        unsigned char destinationDecode[256];

        TEST_ERROR(decodeToBinSize(encodingBase64Url, "c3"), AssertError, "unsupported");
        TEST_ERROR(decodeToBin(encodingBase64Url, "c3", destinationDecode), AssertError, "unsupported");
    }

    // *****************************************************************************************************************************
    if (testBegin("hex"))
    {
        TEST_TITLE("encode");

        const unsigned char *encode = (const unsigned char *)"string_to_encode\r\n";
        char destinationEncode[513];

        encodeToStr(encodingHex, encode, 1, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "73", "1 character encode");
        TEST_RESULT_UINT(encodeToStrSize(encodingHex, 1), strlen(destinationEncode), "check size");

        encodeToStr(encodingHex, encode, strlen((const char *)encode), destinationEncode);
        TEST_RESULT_Z(destinationEncode, "737472696e675f746f5f656e636f64650d0a", "encode full string with \\r\\n");
        TEST_RESULT_UINT(encodeToStrSize(encodingHex, strlen((const char *)encode)), strlen(destinationEncode), "check size");

        encodeToStr(encodingHex, encode, strlen((const char *)encode) + 1, destinationEncode);
        TEST_RESULT_Z(destinationEncode, "737472696e675f746f5f656e636f64650d0a00", "encode full string with \\r\\n and null");
        TEST_RESULT_UINT(encodeToStrSize(encodingHex, strlen((const char *)encode) + 1), strlen(destinationEncode), "check size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("decode");

        unsigned char destinationDecode[256];

        memset(destinationDecode, 0xFF, sizeof(destinationDecode));
        const char *decode = "737472696e675f746f5f656e636f64650d0a00";
        decodeToBin(encodingHex, decode, destinationDecode);
        TEST_RESULT_Z((char *)destinationDecode, (const char *)encode, "full string with \\r\\n and null decode");
        TEST_RESULT_INT(destinationDecode[strlen((const char *)encode) + 1], 0xFF, "check for overrun");
        TEST_RESULT_UINT(decodeToBinSize(encodingHex, decode), strlen((const char *)encode) + 1, "check size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("decode/encode with mixed case");

        const char *decodeMixed = "0123456789AaBbCcDdEeFf";

        TEST_RESULT_VOID(decodeToBin(encodingHex, decodeMixed, destinationDecode), "decode");
        TEST_RESULT_VOID(
            encodeToStr(encodingHex, destinationDecode, decodeToBinSize(encodingHex, decodeMixed), destinationEncode), "encode");
        TEST_RESULT_Z(destinationEncode, "0123456789aabbccddeeff", "check encoded hex");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("decode/encode all values");

        unsigned char decodedAll[256];

        for (unsigned int decodeIdx = 0; decodeIdx < sizeof(decodedAll); decodeIdx++)
            decodedAll[decodeIdx] = (unsigned char)decodeIdx;

        encodeToStr(encodingHex, decodedAll, sizeof(decodedAll), destinationEncode);
        TEST_RESULT_UINT(strlen(destinationEncode), 512, "all values encoded size");
        TEST_RESULT_INT(memcmp(destinationEncode, encodeHexLookup, 512), 0, "all values encoded");
        decodeToBin(encodingHex, destinationEncode, destinationDecode);
        TEST_RESULT_INT(memcmp(decodedAll, destinationDecode, sizeof(decodedAll)), 0, "all values decoded");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("decode errors");

        TEST_ERROR(decodeToBin(encodingHex, "c", destinationDecode), FormatError, "hex size 1 is not evenly divisible by 2");

        TEST_ERROR(
            decodeToBin(encodingHex, "hh", destinationDecode), FormatError,
            "hex invalid character found at position 0");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
