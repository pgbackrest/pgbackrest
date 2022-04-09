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
    if (testBegin("jsonToBool() and jsonToBoolInternal()"))
    {
        TEST_ERROR(jsonToBool(STRDEF("z")), JsonFormatError, "expected boolean at 'z'");
        TEST_ERROR(jsonToBool(STRDEF("falsex")), JsonFormatError, "unexpected characters after boolean at 'x'");

        TEST_RESULT_BOOL(jsonToBool(STRDEF("true")), true, "bool is true");
        TEST_RESULT_BOOL(jsonToBool(STRDEF("false")), false, "bool is false");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonToInt(), jsonToInt64(), jsonToUInt(), jsonToUInt64() and jsonToIntInternal()"))
    {
        TEST_ERROR(jsonToUInt(STRDEF("-")), JsonFormatError, "found '-' with no integer at '-'");
        TEST_ERROR(jsonToUInt(STRDEF(" 555555555 A")), JsonFormatError, "unexpected characters after number at 'A'");

        TEST_RESULT_INT(jsonToInt(STRDEF("-555555555 ")), -555555555, "integer");
        TEST_RESULT_INT(jsonToInt64(STRDEF("-555555555555 ")), -555555555555, "integer");
        TEST_RESULT_UINT(jsonToUInt(STRDEF("\t555555555\n\r")), 555555555, "unsigned integer");
        TEST_RESULT_UINT(jsonToUInt64(STRDEF(" 555555555555 ")), 555555555555, "unsigned integer");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonToStr() and jsonToStrInternal()"))
    {
        TEST_ERROR(jsonToStr(STRDEF("\"\\j\"")), JsonFormatError, "invalid escape character 'j'");
        TEST_ERROR(jsonToStr(STRDEF("\"\\uffff\"")), JsonFormatError, "unable to decode 'ffff'");
        TEST_ERROR(jsonToStr(STRDEF("\"runonstring")), JsonFormatError, "expected '\"' but found null delimiter");
        TEST_ERROR(jsonToStr(STRDEF("\"normal\"L")), JsonFormatError, "unexpected characters after string at 'L'");
        TEST_ERROR(jsonToStr(STRDEF("nullz")), JsonFormatError, "unexpected characters after string at 'z'");

        TEST_RESULT_STR(jsonToStr(STRDEF("null")), NULL, "null string");
        TEST_RESULT_STR_Z(jsonToStr(STRDEF(" \"test\"")), "test", "simple string");
        TEST_RESULT_STR_Z(jsonToStr(STRDEF("\"te\\\"st\" ")), "te\"st", "string with quote");
        TEST_RESULT_STR_Z(jsonToStr(STRDEF("\"\\\"\\\\\\/\\b\\n\\r\\t\\f\\u0026\"")), "\"\\/\b\n\r\t\f&", "string with escapes");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonToKv() and jsonToKvInternal()"))
    {
        TEST_ERROR(jsonToKv(STRDEF("[")), JsonFormatError, "expected '{' at '['");
        TEST_ERROR(jsonToKv(STRDEF("{\"key1\"= 747}")), JsonFormatError, "expected ':' at '= 747}'");
        TEST_ERROR(jsonToKv(STRDEF("{\"key1\" : 747'")), JsonFormatError, "expected '}' at '''");
        TEST_ERROR(jsonToKv(STRDEF("{key1")), JsonFormatError, "expected '\"' at 'key1'");
        TEST_ERROR(jsonToKv(STRDEF("{}BOGUS")), JsonFormatError, "unexpected characters after object at 'BOGUS'");

        KeyValue *kv = NULL;
        TEST_ASSIGN(kv, jsonToKv(STRDEF("{\"key1\": 747, \"key2\":\"value2\",\"key3\"\t:\t[\t] }")), "object");
        TEST_RESULT_UINT(varLstSize(kvKeyList(kv)), 3, "check key total");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, VARSTRDEF("key1"))), 747, "check object uint");
        TEST_RESULT_STR_Z(varStr(kvGet(kv, VARSTRDEF("key2"))), "value2", "check object str");
        TEST_RESULT_UINT(varLstSize(varVarLst(kvGet(kv, VARSTRDEF("key3")))), 0, "check empty array");

        TEST_ASSIGN(kv, jsonToKv(STRDEF("\t{\n} ")), "empty object");
        TEST_RESULT_UINT(varLstSize(kvKeyList(kv)), 0, "check key total");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonToVar()"))
    {
        TEST_ERROR(jsonToVar(strNew()), JsonFormatError, "expected data");
        TEST_ERROR(jsonToVar(STRDEF(" \t\r\n ")), JsonFormatError, "expected data");
        TEST_ERROR(jsonToVar(STRDEF("z")), JsonFormatError, "invalid type at 'z'");
        TEST_ERROR(jsonToVar(STRDEF("3 =")), JsonFormatError, "unexpected characters after JSON at '='");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(varStr(jsonToVar(STRDEF(" \"test\""))), "test", "simple string");
        TEST_RESULT_STR_Z(varStr(jsonToVar(STRDEF("\"te\\\"st\" "))), "te\"st", "string with quote");
        TEST_RESULT_STR_Z(varStr(jsonToVar(STRDEF("\"\\\"\\\\\\/\\b\\n\\r\\t\\f\""))), "\"\\/\b\n\r\t\f", "string with escapes");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(jsonToVar(STRDEF("ton")), JsonFormatError, "expected boolean at 'ton'");
        TEST_RESULT_BOOL(varBool(jsonToVar(STRDEF(" true"))), true, "boolean true");
        TEST_RESULT_BOOL(varBool(jsonToVar(STRDEF("false "))), false, "boolean false");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(jsonToVar(STRDEF("not")), JsonFormatError, "expected null at 'not'");
        TEST_RESULT_PTR(jsonToVar(STRDEF("null")), NULL, "null value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(jsonToVar(STRDEF("[1, \"test\", false")), JsonFormatError, "expected ']' at ''");

        VariantList *valueList = NULL;
        TEST_ASSIGN(valueList, varVarLst(jsonToVar(STRDEF("[1, \"test\", false]"))), "array");
        TEST_RESULT_UINT(varLstSize(valueList), 3, "check array size");
        TEST_RESULT_UINT(varUInt64(varLstGet(valueList, 0)), 1, "check array int");
        TEST_RESULT_STR_Z(varStr(varLstGet(valueList, 1)), "test", "check array str");
        TEST_RESULT_BOOL(varBool(varLstGet(valueList, 2)), false, "check array bool");

        TEST_ASSIGN(valueList, varVarLst(jsonToVar(STRDEF("[ ]"))), "empty array");
        TEST_RESULT_UINT(varLstSize(valueList), 0, "check array size");

        // -------------------------------------------------------------------------------------------------------------------------
        KeyValue *kv = NULL;
        TEST_ASSIGN(kv, varKv(jsonToVar(STRDEF("\t{\n} "))), "empty object");
        TEST_RESULT_UINT(varLstSize(kvKeyList(kv)), 0, "check key total");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonToVarLst() and jsonToArrayInternal()"))
    {
        TEST_ERROR(jsonToVarLst(STRDEF("{")), JsonFormatError, "expected '[' at '{'");
        TEST_ERROR(jsonToVarLst(STRDEF("[")), JsonFormatError, "expected data");
        TEST_ERROR(jsonToVarLst(STRDEF(" [] ZZZ")), JsonFormatError, "unexpected characters after array at 'ZZZ'");

        TEST_RESULT_STRLST_Z(strLstNewVarLst(jsonToVarLst(STRDEF("[\"e1\", \"e2\"]"))), "e1\ne2\n", "json list");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonFromBool()"))
    {
        TEST_RESULT_STR_Z(jsonFromBool(true), "true", "json bool true");
        TEST_RESULT_STR_Z(jsonFromBool(false), "false", "json bool true");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonFromInt(), jsonFromInt64(), jsonFromUInt() and jsonFromUInt64()"))
    {
        TEST_RESULT_STR_Z(jsonFromInt(-2147483648), "-2147483648", "json int");
        TEST_RESULT_STR_Z(jsonFromInt64(-9223372036854775807L), "-9223372036854775807", "json int64");
        TEST_RESULT_STR_Z(jsonFromUInt(4294967295), "4294967295", "json uint");
        TEST_RESULT_STR_Z(jsonFromUInt64(18446744073709551615UL), "18446744073709551615", "json uint64");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonFromStr() and jsonFromStrInternal()"))
    {
        TEST_RESULT_STR_Z(jsonFromStr(NULL), "null", "null string");
        TEST_RESULT_STR_Z(jsonFromStr(STRDEF("simple string")), "\"simple string\"", "simple string");
        TEST_RESULT_STR_Z(jsonFromStr(STRDEF("\"\\/\b\n\r\t\f")), "\"\\\"\\\\/\\b\\n\\r\\t\\f\"", "string escapes");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonFromKv(), jsonFromKvInternal()"))
    {
        KeyValue *keyValue = kvNew();
        String *json = NULL;

        TEST_ASSIGN(json, jsonFromKv(keyValue), "kvToJson - empty");
        TEST_RESULT_STR_Z(json, "{}", "  empty curly brackets");

        kvPut(keyValue, varNewStrZ("backup"), varNewVarLst(varLstNew()));
        TEST_ASSIGN(json, jsonFromKv(keyValue), "kvToJson - kv with empty array");
        TEST_RESULT_STR_Z(json, "{\"backup\":[]}", "  kv with empty array brackets");

        kvPutKv(keyValue, VARSTRDEF("archive"));
        kvPutKv(keyValue, VARSTRDEF("empty"));
        kvPut(keyValue, varNewStrZ("bool1"), varNewBool(true));
        kvPut(keyValue, varNewStrZ("bool2"), varNewBool(false));
        kvPut(keyValue, varNewStrZ("null-str"), varNewStr(NULL));
        kvPut(keyValue, varNewStrZ("checknull"), (Variant *)NULL);

        VariantList *dbList = varLstNew();
        Variant *dbInfo = varNewKv(kvNew());
        kvPut(varKv(dbInfo), VARSTRDEF("id"), varNewInt(1));
        kvPut(varKv(dbInfo), VARSTRDEF("version"), VARSTRDEF("9.4"));
        varLstAdd(dbList, dbInfo);
        varLstAdd(dbList, NULL);
        kvPut(keyValue, varNewStrZ("db"), varNewVarLst(dbList));
        kvPut(keyValue, varNewStrZ("null-list"), varNewVarLst(NULL));

        TEST_ASSIGN(json, jsonFromKv(keyValue), "kvToJson - kv with empty array, kv, varList with values");
        TEST_RESULT_STR_Z(
            json,
            "{"
                "\"archive\":{},"
                "\"backup\":[],"
                "\"bool1\":true,"
                "\"bool2\":false,"
                "\"checknull\":null,"
                "\"db\":["
                    "{"
                        "\"id\":1,"
                        "\"version\":\"9.4\""
                    "},"
                    "null"
                "],"
                "\"empty\":{},"
                "\"null-list\":null,"
                "\"null-str\":null"
            "}",
            "  kv with empty array, kv, varList with values");

        TEST_ASSIGN(
            keyValue,
            varKv(
                jsonToVar(
                    STRDEF(
                    "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
                    "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\"],"
                    "\"checksum-page-error\":[1,[4,6]],\"backup-timestamp-start\":1482182951}"))),
            "multiple values with array");
        TEST_ASSIGN(json, jsonFromKv(keyValue), "  kvToJson - sorted");
        TEST_RESULT_STR_Z(
            json,
            "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\"],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1,[4,6]]}",
            "  check string no pretty print");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonFromVar()"))
    {
        TEST_ERROR(jsonFromVar(varNewInt(1)), JsonFormatError, "variant type is invalid");

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
    if (testBegin("JsonRead and JsonWrite"))
    {
        JsonWrite *write = NULL;

        TEST_ASSIGN(write, jsonWriteNewP(), "new write");

        TEST_RESULT_VOID(jsonWriteArrayBegin(write), "write array begin");
        TEST_RESULT_VOID(jsonWriteBool(write, true), "write bool true");
        TEST_RESULT_VOID(jsonWriteInt(write, 55), "write int 55");
        TEST_RESULT_VOID(jsonWriteStr(write, STRDEF("two\nlines")), "write str with two lines");

        TEST_RESULT_VOID(jsonWriteObjectBegin(write), "write object begin");
        TEST_ERROR(jsonWriteBool(write, false), FormatError, "key has not been defined");
        TEST_RESULT_VOID(jsonWriteKey(write, STRDEF("flag")), "write key 'flag'");
        TEST_ERROR(jsonWriteKey(write, STRDEF("flag")), FormatError, "key has already been defined");
        TEST_RESULT_VOID(jsonWriteBool(write, false), "write bool false");
        TEST_RESULT_VOID(jsonWriteKey(write, STRDEF("val")), "write key 'val'");
        TEST_RESULT_VOID(jsonWriteUInt64(write, UINT64_MAX), "write uint64 max");
        TEST_RESULT_VOID(jsonWriteObjectEnd(write), "write object end");

        TEST_RESULT_VOID(jsonWriteUInt(write, 66), "write int 66");
        TEST_RESULT_VOID(jsonWriteArrayEnd(write), "write array end");

        TEST_ERROR(jsonWriteUInt(write, 66), FormatError, "JSON is complete and nothing more may be added");

        TEST_RESULT_STR_Z(
            strNewBuf(jsonWriteResult(write)), "[true,55,\"two\\nlines\",{\"flag\":false,\"val\":18446744073709551615},66]",
            "json result");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
