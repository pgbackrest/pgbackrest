/***********************************************************************************************************************************
Test Convert JSON to/from KeyValue
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("jsonToVar()"))
    {
        TEST_ERROR(jsonToVar(strNew()), JsonFormatError, "expected data");
        TEST_ERROR(jsonToVar(STRDEF(" \t\r\n ")), JsonFormatError, "expected data");
        TEST_ERROR(jsonToVar(STRDEF("z")), JsonFormatError, "invalid type at 'z'");
        // TEST_ERROR(jsonToVar(STRDEF("3 =")), JsonFormatError, "unexpected characters after JSON at '='");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(varStr(jsonToVar(STRDEF(" \"test\""))), "test", "simple string");
        TEST_RESULT_STR_Z(varStr(jsonToVar(STRDEF("\"te\\\"st\" "))), "te\"st", "string with quote");
        TEST_RESULT_STR_Z(varStr(jsonToVar(STRDEF("\"\\\"\\\\\\/\\b\\n\\r\\t\\f\""))), "\"\\/\b\n\r\t\f", "string with escapes");

        // -------------------------------------------------------------------------------------------------------------------------
        // TEST_ERROR(jsonToVar(STRDEF("ton")), JsonFormatError, "expected boolean at 'ton'");
        TEST_RESULT_BOOL(varBool(jsonToVar(STRDEF(" true"))), true, "boolean true");
        TEST_RESULT_BOOL(varBool(jsonToVar(STRDEF("false "))), false, "boolean false");

        // -------------------------------------------------------------------------------------------------------------------------
        // TEST_ERROR(jsonToVar(STRDEF("not")), JsonFormatError, "expected null at 'not'");
        TEST_RESULT_PTR(jsonToVar(STRDEF("null")), NULL, "null value");

        // -------------------------------------------------------------------------------------------------------------------------
        // TEST_ERROR(jsonToVar(STRDEF("[1, \"test\", false")), JsonFormatError, "expected ']' at ''");

        VariantList *valueList = NULL;
        TEST_ASSIGN(valueList, varVarLst(jsonToVar(STRDEF("[1, \"test\", false]"))), "array");
        TEST_RESULT_UINT(varLstSize(valueList), 3, "check array size");
        TEST_RESULT_UINT(varUInt64(varLstGet(valueList, 0)), 1, "check array int");
        TEST_RESULT_STR_Z(varStr(varLstGet(valueList, 1)), "test", "check array str");
        TEST_RESULT_BOOL(varBool(varLstGet(valueList, 2)), false, "check array bool");

        TEST_ASSIGN(valueList, varVarLst(jsonToVar(STRDEF("[ ]"))), "empty array");
        TEST_RESULT_UINT(varLstSize(valueList), 0, "check array size");

        TEST_RESULT_VOID(jsonToVar(STRDEF("{\"path\":\"/home/vagrant/test/test-0/pg\",\"type\":\"path\"}")), "object");

        // -------------------------------------------------------------------------------------------------------------------------
        KeyValue *kv = NULL;
        TEST_ASSIGN(kv, varKv(jsonToVar(STRDEF("\t{\n} "))), "empty object");
        TEST_RESULT_UINT(varLstSize(kvKeyList(kv)), 0, "check key total");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonFromVar()"))
    {
        String *json = NULL;
        Variant *keyValue = NULL;

        TEST_ASSIGN(keyValue, varNewKv(kvNew()), "build new kv");
        kvPut(varKv(keyValue), varNewStrZ("backup-info-size-delta"), varNewInt(1982702));
        kvPut(varKv(keyValue), varNewStrZ("backup-prior"), varNewStrZ("20161219-212741F_20161219-212803I"));

        Variant *listVar = NULL;
        TEST_ASSIGN(listVar, varNewVarLst(varLstNew()), "  new string array to kv");
        varLstAdd(varVarLst(listVar), varNewStrZ("20161219-212741F"));
        varLstAdd(varVarLst(listVar), varNewStrZ("20161219-212741F_20161219-212803I"));
        varLstAdd(varVarLst(listVar), varNewStrZ(NULL));
        kvPut(varKv(keyValue), varNewStrZ("backup-reference"), listVar);
        kvPut(varKv(keyValue), varNewStrZ("backup-timestamp-start"), varNewInt(1482182951));

        Variant *listVar2 = NULL;
        TEST_ASSIGN(listVar2, varNewVarLst(varLstNew()), "  new int array to kv");
        varLstAdd(varVarLst(listVar2), varNewInt(1));
        kvPut(varKv(keyValue), varNewStrZ("checksum-page-error"), listVar2);

        // Embed a keyValue section to test recursion
        const Variant *sectionKey = VARSTRDEF("section");
        KeyValue *sectionKv = kvPutKv(varKv(keyValue), sectionKey);
        kvPut(sectionKv, VARSTRDEF("key1"), VARSTRDEF("value1"));
        kvPut(sectionKv, VARSTRDEF("key2"), (Variant *)NULL);
        kvPut(sectionKv, VARSTRDEF("key3"), VARSTRDEF("value2"));
        kvAdd(sectionKv, VARSTRDEF("escape"), VARSTRDEF("\"\\/\b\n\r\t\f"));

        TEST_ASSIGN(json, jsonFromVar(keyValue), "KeyValue");
        TEST_RESULT_STR_Z(
            json,
            "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\",null],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1],"
            "\"section\":{\"escape\":\"\\\"\\\\/\\b\\n\\r\\t\\f\",\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}}",
            "  sorted json string result, no pretty print");

        //--------------------------------------------------------------------------------------------------------------------------
        Variant *varListOuter = NULL;

        TEST_ASSIGN(json, jsonFromVar(varNewVarLst(varLstNew())), "VariantList");
        TEST_RESULT_STR_Z(json, "[]", "  empty list no pretty print");

        TEST_ASSIGN(varListOuter, varNewVarLst(varLstNew()), "new variant list with keyValues");
        varLstAdd(varVarLst(varListOuter), varNewStrZ("ASTRING"));
        varLstAdd(varVarLst(varListOuter), varNewInt64(9223372036854775807LL));
        varLstAdd(varVarLst(varListOuter), varNewInt(2147483647));
        varLstAdd(varVarLst(varListOuter), varNewBool(true));
        varLstAdd(varVarLst(varListOuter), varNewVarLst(NULL));
        varLstAdd(varVarLst(varListOuter), varNewVarLst(varLstNew()));
        varLstAdd(varVarLst(varListOuter), NULL);
        varLstAdd(varVarLst(varListOuter), keyValue);

        TEST_ASSIGN(json, jsonFromVar(varListOuter), "VariantList");
        TEST_RESULT_STR_Z(
            json,
            "[\"ASTRING\",9223372036854775807,2147483647,true,null,[],null,{\"backup-info-size-delta\":1982702,"
            "\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\",null],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1],"
            "\"section\":{\"escape\":\"\\\"\\\\/\\b\\n\\r\\t\\f\",\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}}]",
            "  sorted json string result no pretty print");

        Variant *keyValue2 = varDup(keyValue);
        varLstAdd(varVarLst(varListOuter), keyValue2);

        TEST_ASSIGN(json, jsonFromVar(varListOuter), "VariantList - multiple elements");
        TEST_RESULT_STR_Z(
            json,
            "[\"ASTRING\",9223372036854775807,2147483647,true,null,[],null,{\"backup-info-size-delta\":1982702,"
            "\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\",null],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1],"
            "\"section\":{\"escape\":\"\\\"\\\\/\\b\\n\\r\\t\\f\",\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}},"
            "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\",null],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1],"
            "\"section\":{\"escape\":\"\\\"\\\\/\\b\\n\\r\\t\\f\",\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}}]",
            "  sorted json string result no pretty print");

        VariantList *varList = varLstNew();
        varLstAdd(varList, varNewUInt(32));
        varLstAdd(varList, varNewUInt64(10000000000));

        TEST_RESULT_STR_Z(jsonFromVar(varNewVarLst(varList)), "[32,10000000000]", "list various types");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(jsonFromVar(NULL), "null", "null variant");
        TEST_RESULT_STR_Z(jsonFromVar(varNewBool(true)), "true", "bool variant");
        TEST_RESULT_STR_Z(jsonFromVar(varNewUInt(66)), "66", "uint variant");
        TEST_RESULT_STR_Z(jsonFromVar(varNewUInt64(10000000001)), "10000000001", "uint64 variant");
        TEST_RESULT_STR_Z(jsonFromVar(varNewStrZ("test \" string")), "\"test \\\" string\"", "string variant");
    }

    // *****************************************************************************************************************************
    if (testBegin("JsonRead"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing comma");

        JsonRead *read = NULL;

        TEST_ASSIGN(read, jsonReadNew(STRDEF("\r[\ttrue true]")), "new read");

        TEST_RESULT_VOID(jsonReadArrayBegin(read), "array begin");
        TEST_RESULT_BOOL(jsonReadBool(read), true, "bool true");
        jsonReadConsumeWhiteSpace(read);
        TEST_ERROR(jsonReadBool(read), JsonFormatError, "missing comma at: true]");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing comma");

        TEST_ASSIGN(read, jsonReadNew(STRDEF("\r{ ] ")), "new read");

        TEST_RESULT_VOID(jsonReadObjectBegin(read), "objects begin");
        jsonReadConsumeWhiteSpace(read);
        TEST_ERROR(jsonReadObjectEnd(read), JsonFormatError, "expected } but found ] at: ] ");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on no digits in number");

        TEST_ERROR(jsonReadNumber(jsonReadNew(STRDEF("a"))), JsonFormatError, "no digits found at: a");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on number larger than available buffer");

        TEST_ERROR(
            jsonReadNumber(jsonReadNew(STRDEF("9999999999999999999999999999999999999999999999999999999999999999"))),
            JsonFormatError, "invalid number at: 9999999999999999999999999999999999999999999999999999999999999999");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid ascii encoding");

        TEST_ERROR(jsonReadStr(jsonReadNew(STRDEF("\"\\u1111"))), JsonFormatError, "unable to decode at: \\u1111");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid escape");

        TEST_ERROR(jsonReadStr(jsonReadNew(STRDEF("\"\\xy"))), JsonFormatError, "invalid escape character at: \\xy");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on unterminated string");

        TEST_ERROR(jsonReadStr(jsonReadNew(STRDEF("\""))), JsonFormatError, "expected '\"' but found null delimiter");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid bool");

        TEST_ERROR(jsonReadBool(jsonReadNew(STRDEF("fase"))), JsonFormatError, "expected boolean at: fase");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid integers");

        TEST_ERROR(
            jsonReadInt(jsonReadNew(STRDEF("-9223372036854775807"))), JsonFormatError,
            "-9223372036854775807 is out of range for int");
        TEST_ERROR(
            jsonReadInt(jsonReadNew(STRDEF("18446744073709551615"))), JsonFormatError,
            "18446744073709551615 is out of range for int");
        TEST_ERROR(
            jsonReadInt64(jsonReadNew(STRDEF("18446744073709551615"))), JsonFormatError,
            "18446744073709551615 is out of range for int64");
        TEST_ERROR(
            jsonReadUInt(jsonReadNew(STRDEF("-9223372036854775807"))), JsonFormatError,
            "-9223372036854775807 is out of range for uint");
        TEST_ERROR(
            jsonReadUInt(jsonReadNew(STRDEF("18446744073709551615"))), JsonFormatError,
            "18446744073709551615 is out of range for uint");
        TEST_ERROR(
            jsonReadUInt64(jsonReadNew(STRDEF("-9223372036854775807"))), JsonFormatError,
            "-9223372036854775807 is out of range for uint64");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid null");

        TEST_ERROR(jsonReadNull(jsonReadNew(STRDEF("nil"))), JsonFormatError, "expected null at: nil");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing : after key");

        TEST_ASSIGN(read, jsonReadNew(STRDEF("\r{ \"key1\"xxx")), "new read");

        TEST_RESULT_VOID(jsonReadObjectBegin(read), "objects begin");
        TEST_ERROR(jsonReadKey(read), JsonFormatError, "expected : after key 'key1' at: xxx");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("scalar");

        TEST_ASSIGN(read, jsonReadNew(STRDEF("\rtrue\t")), "new read");

        TEST_RESULT_BOOL(jsonReadBool(read), true, "bool true");
        TEST_ERROR(jsonReadBool(read), FormatError, "JSON read is complete");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("complex");

        const String *json = STRDEF(
            "\t\r\n "                                               // Leading whitespace
            "[\n"
            "    {"
            "        \"key1\"\t: \"\\u0076alue1\","
            "        \"key2\"\t: 777,"
            "        \"key3\"\t: 777,"
            "        \"key4\"\t: 9223372036854775807"
            "    },"
            "    \"abc\","
            "    null,"
            "    [\"a\", \"b\"],"
            "    123,"
            "    18446744073709551615,"
            "    -1,"
            "    -9223372036854775807,"
            "    [true, {\"key1\":null,\"key1\":\"value1\"}],"
            "    true,"
            "    [-1],"
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
        TEST_RESULT_STR_Z(jsonReadStr(read), "value1", "str");
        TEST_ERROR_FMT(jsonReadStr(read), AssertError, "key has not been read at: %s", read->json);
        TEST_ERROR(jsonReadKeyRequire(read, STRDEF("key1")), JsonFormatError, "required key 'key1' not found");
        TEST_RESULT_VOID(jsonReadKeyRequire(read, STRDEF("key2")), "key2");
        jsonReadConsumeWhiteSpace(read);
        TEST_ERROR_FMT(jsonReadStr(read), JsonFormatError, "expected 3 but found 2 at: %s", read->json);
        TEST_RESULT_INT(jsonReadInt(read), 777, "int");
        TEST_RESULT_BOOL(jsonReadKeyExpectZ(read, "key4"), true, "expect key4");
        TEST_RESULT_INT(jsonReadInt64(read), INT64_MAX, "int");
        TEST_RESULT_BOOL(jsonReadKeyExpectStrId(read, STRID5("key5", 0xee4ab0)), false, "do not expect key5");
        TEST_ERROR(jsonReadKeyRequireStrId(read, STRID5("key5", 0xee4ab0)), JsonFormatError, "required key 'key5' not found");
        TEST_ERROR(jsonReadKeyRequireZ(read, "key5"), JsonFormatError, "required key 'key5' not found");
        TEST_RESULT_VOID(jsonReadObjectEnd(read), "object end");

        TEST_RESULT_STR_Z(jsonReadStr(read), "abc", "str");
        TEST_RESULT_STR_Z(jsonReadStr(read), NULL, "str null");
        TEST_RESULT_STRLST_Z(jsonReadStrLst(read), "a\nb\n", "str list");
        TEST_RESULT_UINT(jsonReadUInt(read), 123, "uint");
        TEST_RESULT_UINT(jsonReadUInt64(read), 18446744073709551615U, "uint64");
        TEST_RESULT_INT(jsonReadInt(read), -1, "int");
        TEST_RESULT_INT(jsonReadInt64(read), -9223372036854775807L, "int64");
        TEST_RESULT_VOID(jsonReadSkip(read), "skip");
        TEST_RESULT_BOOL(jsonReadBool(read), true, "bool true");
        TEST_RESULT_STR_Z(jsonFromVar(jsonReadVar(read)), "[-1]", "var");
        TEST_RESULT_BOOL(jsonReadBool(read), false, "bool false");

        TEST_RESULT_VOID(jsonReadArrayEnd(read), "array end");
        TEST_ERROR(jsonReadArrayEnd(read), FormatError, "JSON read is complete");

        TEST_RESULT_VOID(jsonReadFree(read), "free");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("validate");

        TEST_RESULT_VOID(jsonValidate(json), "valid");
        TEST_ERROR(jsonValidate(strNewFmt("%s]", strZ(json))), JsonFormatError, "characters after JSON at: ]");
    }

    // *****************************************************************************************************************************
    if (testBegin("JsonWrite"))
    {
        JsonWrite *write = NULL;

        TEST_ASSIGN(write, jsonWriteNewP(), "new write");

        TEST_RESULT_VOID(jsonWriteArrayBegin(write), "array begin");
        TEST_RESULT_VOID(jsonWriteBool(write, true), "bool true");
        TEST_RESULT_VOID(jsonWriteInt(write, 55), "int 55");
        TEST_RESULT_VOID(jsonWriteInt64(write, INT64_MIN), "int64 min");
        TEST_RESULT_VOID(jsonWriteStr(write, STRDEF("two\nlines")), "str with two lines");

        TEST_RESULT_VOID(jsonWriteObjectBegin(write), "object begin");
        TEST_ERROR(jsonWriteBool(write, false), JsonFormatError, "key has not been defined");
        TEST_RESULT_VOID(jsonWriteKey(write, STRDEF("flag")), "key 'flag'");
        TEST_ERROR(jsonWriteKey(write, STRDEF("flag")), JsonFormatError, "key has already been defined");
        TEST_RESULT_VOID(jsonWriteBool(write, false), "bool false");
        TEST_ERROR(jsonWriteKey(write, STRDEF("flag")), JsonFormatError, "key 'flag' is not after prior key 'flag'");
        TEST_RESULT_VOID(jsonWriteKeyStrId(write, STRID5("key5", 0xee4ab0)), "key 'key5'");
        TEST_RESULT_VOID(jsonWriteStrFmt(write, "%d", 898), "str fmt");
        TEST_RESULT_VOID(jsonWriteKeyZ(write, "val"), "key 'val'");
        TEST_RESULT_VOID(jsonWriteUInt64(write, UINT64_MAX), "uint64 max");
        TEST_RESULT_VOID(jsonWriteObjectEnd(write), "object end");

        StringList *list = strLstNew();
        strLstAddZ(list, "a");
        strLstAddZ(list, "abc");

        TEST_RESULT_VOID(jsonWriteStrLst(write, list), "write str list");

        TEST_RESULT_VOID(jsonWriteUInt(write, 66), "write int 66");
        TEST_RESULT_VOID(jsonWriteJson(write, NULL), "null json");
        TEST_RESULT_VOID(jsonWriteJson(write, STRDEF("{}")), "json");
        TEST_RESULT_VOID(jsonWriteZ(write, NULL), "null z");
        TEST_RESULT_VOID(jsonWriteZ(write, "a string"), "z");
        TEST_RESULT_VOID(jsonWriteVar(write, varNewKv(NULL)), "null kv");
        TEST_RESULT_VOID(jsonWriteStr(write, NULL), "null str");
        TEST_RESULT_VOID(jsonWriteArrayEnd(write), "array end");

        TEST_ERROR(jsonWriteUInt(write, 66), JsonFormatError, "JSON write is complete");

        TEST_RESULT_STR_Z(
            jsonWriteResult(write),
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
                "[\"a\",\"abc\"],"
                "66,"
                "null,"
                "{},"
                "null,"
                "\"a string\","
                "null,"
                "null"
            "]",
            "json result");

        TEST_RESULT_VOID(jsonWriteFree(write), "free");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
