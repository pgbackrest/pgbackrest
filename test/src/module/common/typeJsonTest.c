/***********************************************************************************************************************************
Test Convert JSON to/from KeyValue
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("jsonToBool() and jsonToBoolInternal()"))
    {
        TEST_ERROR(jsonToBool(strNew("z")), JsonFormatError, "expected boolean at 'z'");
        TEST_ERROR(jsonToBool(strNew("falsex")), JsonFormatError, "unexpected characters after boolean at 'x'");

        TEST_RESULT_BOOL(jsonToBool(strNew("true")), true, "bool is true");
        TEST_RESULT_BOOL(jsonToBool(strNew("false")), false, "bool is false");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonToInt(), jsonToInt64(), jsonToUInt(), jsonToUInt64() and jsonToIntInternal()"))
    {
        TEST_ERROR(jsonToUInt(strNew("-")), JsonFormatError, "found '-' with no integer at '-'");
        TEST_ERROR(jsonToUInt(strNew(" 555555555 A")), JsonFormatError, "unexpected characters after number at 'A'");

        TEST_RESULT_INT(jsonToInt(strNew("-555555555 ")), -555555555, "integer");
        TEST_RESULT_INT(jsonToInt64(strNew("-555555555555 ")), -555555555555, "integer");
        TEST_RESULT_UINT(jsonToUInt(strNew("\t555555555\n\r")), 555555555, "unsigned integer");
        TEST_RESULT_UINT(jsonToUInt64(strNew(" 555555555555 ")), 555555555555, "unsigned integer");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonToStr() and jsonToStrInternal()"))
    {
        TEST_ERROR(jsonToStr(strNew("\"\\j\"")), JsonFormatError, "invalid escape character 'j'");
        TEST_ERROR(jsonToStr(strNew("\"runonstring")), JsonFormatError, "expected '\"' but found null delimiter");
        TEST_ERROR(jsonToStr(strNew("\"normal\"L")), JsonFormatError, "unexpected characters after string at 'L'");
        TEST_ERROR(jsonToStr(strNew("nullz")), JsonFormatError, "unexpected characters after string at 'z'");

        TEST_RESULT_STR(jsonToStr(strNew("null")), NULL, "null string");
        TEST_RESULT_STR_Z(jsonToStr(strNew(" \"test\"")), "test", "simple string");
        TEST_RESULT_STR_Z(jsonToStr(strNew("\"te\\\"st\" ")), "te\"st", "string with quote");
        TEST_RESULT_STR_Z(jsonToStr(strNew("\"\\\"\\\\\\/\\b\\n\\r\\t\\f\"")), "\"\\/\b\n\r\t\f", "string with escapes");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonToKv() and jsonToKvInternal()"))
    {
        TEST_ERROR(jsonToKv(strNew("[")), JsonFormatError, "expected '{' at '['");
        TEST_ERROR(jsonToKv(strNew("{\"key1\"= 747}")), JsonFormatError, "expected ':' at '= 747}'");
        TEST_ERROR(jsonToKv(strNew("{\"key1\" : 747'")), JsonFormatError, "expected '}' at '''");
        TEST_ERROR(jsonToKv(strNew("{key1")), JsonFormatError, "expected '\"' at 'key1'");
        TEST_ERROR(jsonToKv(strNew("{}BOGUS")), JsonFormatError, "unexpected characters after object at 'BOGUS'");

        KeyValue *kv = NULL;
        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\": 747, \"key2\":\"value2\",\"key3\"\t:\t[\t] }")), "object");
        TEST_RESULT_UINT(varLstSize(kvKeyList(kv)), 3, "check key total");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, varNewStr(strNew("key1")))), 747, "check object uint");
        TEST_RESULT_STR_Z(varStr(kvGet(kv, varNewStr(strNew("key2")))), "value2", "check object str");
        TEST_RESULT_UINT(varLstSize(varVarLst(kvGet(kv, varNewStr(strNew("key3"))))), 0, "check empty array");

        TEST_ASSIGN(kv, jsonToKv(strNew("\t{\n} ")), "empty object");
        TEST_RESULT_UINT(varLstSize(kvKeyList(kv)), 0, "check key total");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonToVar()"))
    {
        TEST_ERROR(jsonToVar(strNew("")), JsonFormatError, "expected data");
        TEST_ERROR(jsonToVar(strNew(" \t\r\n ")), JsonFormatError, "expected data");
        TEST_ERROR(jsonToVar(strNew("z")), JsonFormatError, "invalid type at 'z'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(varStr(jsonToVar(strNew(" \"test\""))), "test", "simple string");
        TEST_RESULT_STR_Z(varStr(jsonToVar(strNew("\"te\\\"st\" "))), "te\"st", "string with quote");
        TEST_RESULT_STR_Z(varStr(jsonToVar(strNew("\"\\\"\\\\\\/\\b\\n\\r\\t\\f\""))), "\"\\/\b\n\r\t\f", "string with escapes");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(jsonToVar(strNew("ton")), JsonFormatError, "expected boolean at 'ton'");
        TEST_RESULT_BOOL(varBool(jsonToVar(strNew(" true"))), true, "boolean true");
        TEST_RESULT_BOOL(varBool(jsonToVar(strNew("false "))), false, "boolean false");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(jsonToVar(strNew("not")), JsonFormatError, "expected null at 'not'");
        TEST_RESULT_PTR(jsonToVar(strNew("null")), NULL, "null value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(jsonToVar(strNew("[1, \"test\", false")), JsonFormatError, "expected ']' at ''");

        VariantList *valueList = NULL;
        TEST_ASSIGN(valueList, varVarLst(jsonToVar(strNew("[1, \"test\", false]"))), "array");
        TEST_RESULT_UINT(varLstSize(valueList), 3, "check array size");
        TEST_RESULT_UINT(varUInt64(varLstGet(valueList, 0)), 1, "check array int");
        TEST_RESULT_STR_Z(varStr(varLstGet(valueList, 1)), "test", "check array str");
        TEST_RESULT_BOOL(varBool(varLstGet(valueList, 2)), false, "check array bool");

        TEST_ASSIGN(valueList, varVarLst(jsonToVar(strNew("[ ]"))), "empty array");
        TEST_RESULT_UINT(varLstSize(valueList), 0, "check array size");

        // -------------------------------------------------------------------------------------------------------------------------
        KeyValue *kv = NULL;
        TEST_ASSIGN(kv, varKv(jsonToVar(strNew("\t{\n} "))), "empty object");
        TEST_RESULT_UINT(varLstSize(kvKeyList(kv)), 0, "check key total");
    }

    // *****************************************************************************************************************************
    if (testBegin("jsonToVarLst() and jsonToArrayInternal()"))
    {
        TEST_ERROR(jsonToVarLst(strNew("{")), JsonFormatError, "expected '[' at '{'");
        TEST_ERROR(jsonToVarLst(strNew("[")), JsonFormatError, "expected data");
        TEST_ERROR(jsonToVarLst(strNew(" [] ZZZ")), JsonFormatError, "unexpected characters after array at 'ZZZ'");

        TEST_RESULT_STR_Z(strLstJoin(strLstNewVarLst(jsonToVarLst(strNew("[\"e1\", \"e2\"]"))), "|"), "e1|e2", "json list");
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
        TEST_RESULT_STR_Z(jsonFromStr(strNew("simple string")), "\"simple string\"", "simple string");
        TEST_RESULT_STR_Z(jsonFromStr(strNew("\"\\/\b\n\r\t\f")), "\"\\\"\\\\/\\b\\n\\r\\t\\f\"", "string escapes");
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

        kvPutKv(keyValue, varNewStr(strNew("archive")));
        kvPutKv(keyValue, varNewStr(strNew("empty")));
        kvPut(keyValue, varNewStrZ("bool1"), varNewBool(true));
        kvPut(keyValue, varNewStrZ("bool2"), varNewBool(false));
        kvPut(keyValue, varNewStrZ("null-str"), varNewStr(NULL));
        kvPut(keyValue, varNewStrZ("checknull"), (Variant *)NULL);

        VariantList *dbList = varLstNew();
        Variant *dbInfo = varNewKv(kvNew());
        kvPut(varKv(dbInfo), varNewStr(strNew("id")), varNewInt(1));
        kvPut(varKv(dbInfo), varNewStr(strNew("version")),  varNewStr(strNew("9.4")));
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
                    strNew(
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
        Variant *sectionKey = varNewStr(strNew("section"));
        KeyValue *sectionKv = kvPutKv(varKv(keyValue), sectionKey);
        kvAdd(sectionKv, varNewStr(strNew("key1")), varNewStr(strNew("value1")));
        kvAdd(sectionKv, varNewStr(strNew("key2")), (Variant *)NULL);
        kvAdd(sectionKv, varNewStr(strNew("key3")), varNewStr(strNew("value2")));
        kvAdd(sectionKv, varNewStr(strNew("escape")), varNewStr(strNew("\"\\/\b\n\r\t\f")));

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
        varLstAdd(varVarLst(varListOuter), varNewVarLst(varLstNew()));
        varLstAdd(varVarLst(varListOuter), NULL);
        varLstAdd(varVarLst(varListOuter), keyValue);

        TEST_ASSIGN(json, jsonFromVar(varListOuter), "VariantList");
        TEST_RESULT_STR_Z(
            json,
            "[\"ASTRING\",9223372036854775807,2147483647,true,[],null,{\"backup-info-size-delta\":1982702,"
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
            "[\"ASTRING\",9223372036854775807,2147483647,true,[],null,{\"backup-info-size-delta\":1982702,"
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

    FUNCTION_HARNESS_RESULT_VOID();
}
