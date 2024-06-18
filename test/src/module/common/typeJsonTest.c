/***********************************************************************************************************************************
Test JSON read/write
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("JsonRead"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on no data");

        TEST_ERROR(jsonReadStr(jsonReadNew(STRDEF(""))), JsonFormatError, "expected data");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid type");

        TEST_ERROR(jsonReadStr(jsonReadNew(STRDEF("x"))), JsonFormatError, "invalid type at: x");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing comma");

        JsonRead *read = NULL;

        TEST_ASSIGN(read, jsonReadNew(STRDEF("\r[\ttrue true]")), "new read");

        TEST_RESULT_VOID(jsonReadArrayBegin(read), "array begin");
        TEST_RESULT_BOOL(jsonReadBool(read), true, "bool true");
        jsonReadConsumeWhiteSpace(read);
        TEST_ERROR(jsonReadBool(read), JsonFormatError, "missing comma at: true]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on mismatched container end");

        TEST_ASSIGN(read, jsonReadNew(STRDEF("\r{ ] ")), "new read");

        TEST_RESULT_VOID(jsonReadObjectBegin(read), "object begin");
        jsonReadConsumeWhiteSpace(read);
        TEST_ERROR(jsonReadObjectEnd(read), JsonFormatError, "expected } but found ] at: ] ");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on no digits in number");

        TEST_ERROR(jsonReadNumber(jsonReadNew(STRDEF("a"))), JsonFormatError, "no digits found at: a");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on number larger than available buffer");

        TEST_ERROR(
            jsonReadNumber(jsonReadNew(STRDEF("9999999999999999999999999999999999999999999999999999999999999999"))),
            JsonFormatError, "invalid number at: 9999999999999999999999999999999999999999999999999999999999999999");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid ascii encoding");

        TEST_ERROR(jsonReadStr(jsonReadNew(STRDEF("\"\\u1111"))), JsonFormatError, "unable to decode at: \\u1111");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid escape");

        TEST_ERROR(jsonReadStr(jsonReadNew(STRDEF("\"\\xy"))), JsonFormatError, "invalid escape character at: \\xy");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on unterminated string");

        TEST_ERROR(jsonReadStr(jsonReadNew(STRDEF("\""))), JsonFormatError, "expected '\"' but found null delimiter");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid bool");

        TEST_ERROR(jsonReadBool(jsonReadNew(STRDEF("fase"))), JsonFormatError, "expected boolean at: fase");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid integers");

        TEST_ERROR(
            jsonReadInt(jsonReadNew(strNewFmt("%" PRId64, (int64_t)INT_MIN - 1))), JsonFormatError,
            "-2147483649 is out of range for int");
        TEST_ERROR(
            jsonReadInt(jsonReadNew(strNewFmt("%" PRId64, (int64_t)INT_MAX + 1))), JsonFormatError,
            "2147483648 is out of range for int");
        TEST_ERROR(
            jsonReadInt64(jsonReadNew(strNewFmt("%" PRIu64, (uint64_t)INT64_MAX + 1))), JsonFormatError,
            "9223372036854775808 is out of range for int64");
        TEST_ERROR(jsonReadUInt(jsonReadNew(STRDEF("-1"))), JsonFormatError, "-1 is out of range for uint");
        TEST_ERROR(
            jsonReadUInt(jsonReadNew(strNewFmt("%" PRIu64, (uint64_t)UINT_MAX + 1))), JsonFormatError,
            "4294967296 is out of range for uint");
        TEST_ERROR(jsonReadUInt64(jsonReadNew(STRDEF("-1"))), JsonFormatError, "-1 is out of range for uint64");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid null");

        TEST_ERROR(jsonReadNull(jsonReadNew(STRDEF("nil"))), JsonFormatError, "expected null at: nil");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on escape characters in key");

        TEST_ASSIGN(read, jsonReadNew(STRDEF("{\"\\\"")), "new read");

        TEST_RESULT_VOID(jsonReadObjectBegin(read), "object begin");
        jsonReadConsumeWhiteSpace(read);
        TEST_ERROR(jsonReadKey(read), JsonFormatError, "escape character not allowed in key at: \\\"");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on unterminated key");

        TEST_ASSIGN(read, jsonReadNew(STRDEF("{\"")), "new read");

        TEST_RESULT_VOID(jsonReadObjectBegin(read), "object begin");
        jsonReadConsumeWhiteSpace(read);
        TEST_ERROR(jsonReadKey(read), JsonFormatError, "expected '\"' but found null delimiter");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing : after key");

        TEST_ASSIGN(read, jsonReadNew(STRDEF("\r{ \"key1\"xxx")), "new read");

        TEST_RESULT_VOID(jsonReadObjectBegin(read), "object begin");
        TEST_ERROR(jsonReadKey(read), JsonFormatError, "expected : after key 'key1' at: xxx");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("scalar");

        TEST_ASSIGN(read, jsonReadNew(STRDEF("\rtrue\t")), "new read");

        TEST_RESULT_BOOL(jsonReadBool(read), true, "bool true");
        TEST_ERROR(jsonReadBool(read), FormatError, "JSON read is complete");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("complex");

        #define TEST_VARIANT                                        "[-1,true,\"zxv\",{\"key1\":null,\"key2\":777}]"

        const String *json = STRDEF(
            "\t\r\n "                                               // Leading whitespace
            "[\n"
            "    {"
            "        \"key1\"\t: \"\\u0076\\na\\tlu e\\f\\b\\/\\\\\\\"\\r1\","
            "        \"key2\"\t: 777,"
            "        \"key3\"\t: 777,"
            "        \"key4\"\t: 9223372036854775807"
            "    },"
            "    \"abc\\/\","
            "    \"def\","
            "    null,"
            "    [\"a\", \"b\"],"
            "    123,"
            "    18446744073709551615,"
            "    -1,"
            "    -9223372036854775807,"
            "    [true, {\"key1\":null,\"key1\":\"value1\"}],"
            "    true,"
            "    " TEST_VARIANT ","
            "    false\n"
            "]"
            " ");                                                  // Extra whitespace at the end

        TEST_ASSIGN(read, jsonReadNew(strNewFmt("%s]", strZ(json))), "new read");

        TEST_RESULT_VOID(jsonReadArrayBegin(read), "array begin");
        jsonReadConsumeWhiteSpace(read);
        TEST_ERROR_FMT(jsonReadArrayEnd(read), JsonFormatError, "expected ] but found { at: %s", read->json);

        TEST_RESULT_VOID(jsonReadObjectBegin(read), "object begin");
        TEST_RESULT_BOOL(jsonReadKeyExpect(read, STRDEF("key0")), false, "expect key0");
        TEST_RESULT_STR_Z(jsonReadKey(read), "key1", "key1");
        jsonReadConsumeWhiteSpace(read);
        TEST_ERROR_FMT(jsonReadKey(read), AssertError, "key has already been read at: %s", read->json);
        TEST_RESULT_STR_Z(jsonReadStr(read), "v\na\tlu e\f\b/\\\"\r1", "str");
        TEST_ERROR_FMT(jsonReadStr(read), AssertError, "key has not been read at: %s", read->json);
        TEST_ERROR(jsonReadKeyRequire(read, STRDEF("key1")), JsonFormatError, "required key 'key1' not found");
        TEST_RESULT_VOID(jsonReadKeyRequire(read, STRDEF("key2")), "key2");
        jsonReadConsumeWhiteSpace(read);
        TEST_ERROR_FMT(jsonReadStr(read), JsonFormatError, "expected 'str' but found 'nmbr' at: %s", read->json);
        TEST_RESULT_INT(jsonReadInt(read), 777, "int");
        TEST_RESULT_BOOL(jsonReadKeyExpectZ(read, "key4"), true, "expect key4");
        TEST_RESULT_INT(jsonReadInt64(read), INT64_MAX, "int");
        TEST_RESULT_BOOL(jsonReadKeyExpectStrId(read, STRID5("key5", 0xee4ab0)), false, "do not expect key5");
        TEST_ERROR(jsonReadKeyRequireStrId(read, STRID5("key5", 0xee4ab0)), JsonFormatError, "required key 'key5' not found");
        TEST_ERROR(jsonReadKeyRequireZ(read, "key5"), JsonFormatError, "required key 'key5' not found");
        TEST_RESULT_BOOL(jsonReadUntil(read, jsonTypeObjectEnd), false, "read until object end");
        TEST_RESULT_VOID(jsonReadObjectEnd(read), "object end");

        TEST_RESULT_STR_Z(jsonReadStr(read), "abc/", "str");
        TEST_RESULT_STR_Z(strIdToStr(jsonReadStrId(read)), "def", "strid");
        TEST_RESULT_STR_Z(jsonReadStr(read), NULL, "str null");
        TEST_RESULT_STRLST_Z(jsonReadStrLst(read), "a\nb\n", "str list");
        TEST_RESULT_UINT(jsonReadUInt(read), 123, "uint");
        TEST_RESULT_UINT(jsonReadUInt64(read), 18446744073709551615U, "uint64");
        TEST_RESULT_BOOL(jsonReadUntil(read, jsonTypeArrayEnd), true, "read until array end");
        TEST_RESULT_INT(jsonReadInt(read), -1, "int");
        TEST_RESULT_INT(jsonReadInt64(read), -9223372036854775807L, "int64");
        TEST_RESULT_VOID(jsonReadSkip(read), "skip");
        TEST_RESULT_BOOL(jsonReadBool(read), true, "bool true");
        TEST_RESULT_STR_Z(jsonFromVar(jsonReadVar(read)), TEST_VARIANT, "var");
        TEST_RESULT_BOOL(jsonReadBool(read), false, "bool false");

        TEST_RESULT_VOID(jsonReadArrayEnd(read), "array end");
        TEST_ERROR(jsonReadArrayEnd(read), FormatError, "JSON read is complete");

        TEST_RESULT_VOID(jsonReadFree(read), "free");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("variant");

        TEST_RESULT_STR_Z(varStr(jsonToVar(STRDEF("\"test\""))), "test", "var");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("validate");

        TEST_RESULT_VOID(jsonValidate(json), "valid");
        TEST_ERROR(jsonValidate(STRDEF("\"\\u000")), JsonFormatError, "unable to decode at: \\u000");
        TEST_ERROR(jsonValidate(STRDEF("\"")), JsonFormatError, "expected '\"' but found null delimiter");
        TEST_ERROR(jsonValidate(STRDEF("{\"key\"x")), JsonFormatError, "expected : after key at: x");
        TEST_ERROR(jsonValidate(strNewFmt("%s]", strZ(json))), JsonFormatError, "characters after JSON at: ]");
    }

    // *****************************************************************************************************************************
    if (testBegin("JsonWrite"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("complex");

        JsonWrite *write = NULL;

        TEST_ASSIGN(write, jsonWriteNewP(), "new write");

        TEST_RESULT_VOID(jsonWriteArrayBegin(write), "array begin");
        TEST_RESULT_VOID(jsonWriteBool(write, true), "bool true");
        TEST_RESULT_VOID(jsonWriteInt(write, 55), "int 55");
        TEST_RESULT_VOID(jsonWriteInt64(write, INT64_MIN), "int64 min");
        TEST_RESULT_VOID(jsonWriteStr(write, STRDEF("two\nlines")), "str with two lines");

        TEST_RESULT_VOID(jsonWriteObjectBegin(write), "object begin");
        TEST_ERROR(jsonWriteBool(write, false), JsonFormatError, "key has not been written");
        TEST_ERROR(
            jsonWriteKeyZ(write, "1234567890123456789012345678901234567890123456789012345678901234567"), AssertError,
            "key '1234567890123456789012345678901234567890123456789012345678901234567' must not be longer than 66 bytes");
        TEST_RESULT_VOID(jsonWriteKey(write, STRDEF("flag")), "key 'flag'");
        TEST_ERROR(jsonWriteKey(write, STRDEF("flag")), JsonFormatError, "key has already been written");
        TEST_RESULT_VOID(jsonWriteBool(write, false), "bool false");
        TEST_ERROR(jsonWriteKey(write, STRDEF("flag")), JsonFormatError, "key 'flag' is not after prior key 'flag'");
        TEST_RESULT_VOID(jsonWriteKeyStrId(write, STRID5("key5", 0xee4ab0)), "key 'key5'");
        TEST_RESULT_VOID(jsonWriteStrFmt(write, "%d", 898), "str fmt");
        TEST_RESULT_VOID(jsonWriteKeyZ(write, "val"), "key 'val'");
        TEST_RESULT_VOID(jsonWriteUInt64(write, UINT64_MAX), "uint64 max");
        TEST_RESULT_VOID(jsonWriteObjectEnd(write), "object end");

        StringList *list = strLstNew();
        strLstAddZ(list, "a");
        strLstAddZ(list, "\n\"a\\\r\t\bbc");

        TEST_RESULT_VOID(jsonWriteStrLst(write, list), "write str list");

        TEST_RESULT_VOID(jsonWriteUInt(write, 66), "write int 66");
        TEST_RESULT_VOID(jsonWriteJson(write, NULL), "null json");
        TEST_RESULT_VOID(jsonWriteJson(write, STRDEF("{}")), "json");
        TEST_RESULT_VOID(jsonWriteZ(write, NULL), "null z");
        TEST_RESULT_VOID(jsonWriteZ(write, "a string\f"), "z");
        TEST_RESULT_VOID(jsonWriteStrId(write, strIdFromZ("hilly")), "strid");
        TEST_RESULT_VOID(jsonWriteVar(write, varNewKv(NULL)), "null kv");
        TEST_RESULT_VOID(jsonWriteStr(write, NULL), "null str");
        TEST_RESULT_VOID(jsonWriteArrayEnd(write), "array end");

        TEST_ERROR(jsonWriteUInt(write, 66), JsonFormatError, "JSON write is complete");

        TEST_RESULT_STR_Z(
            jsonWriteResult(write),
            // {uncrustify_off - indentation}
            "["
                "true,"
                "55,"
                "-9223372036854775808,"
                "\"two\\nlines\","
                "{"
                    "\"flag\":false,"
                    "\"key5\":\"898\","
                    "\"val\":18446744073709551615"
                "},"
                "[\"a\",\"\\n\\\"a\\\\\\r\\t\\bbc\"],"
                "66,"
                "null,"
                "{},"
                "null,"
                "\"a string\\f\","
                "\"hilly\","
                "null,"
                "null"
            "]",
            // {uncrustify_on}
            "json result");

        TEST_RESULT_VOID(jsonWriteFree(write), "free");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("variant");

        TEST_RESULT_STR_Z(jsonFromVar(varNewStr(NULL)), "null", "null str");
        TEST_RESULT_STR_Z(jsonFromVar(varNewVarLst(NULL)), "null", "null var lst");
        TEST_RESULT_STR_Z(jsonFromVar(varNewUInt(47)), "47", "uint");
        TEST_RESULT_STR_Z(jsonFromVar(varNewInt(-99)), "-99", "int");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
