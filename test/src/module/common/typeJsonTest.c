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
    if (testBegin("jsonToVar()"))
    {
        TEST_ERROR(jsonToVar(strNew("")), JsonFormatError, "expected data");
        TEST_ERROR(jsonToVar(strNew(" \t\r\n ")), JsonFormatError, "expected data");
        TEST_ERROR(jsonToVar(strNew("z")), JsonFormatError, "invalid type at 'z'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(jsonToVar(strNew("\"\\j\"")), JsonFormatError, "invalid escape character 'j'");
        TEST_ERROR(jsonToVar(strNew("\"runonstring")), JsonFormatError, "expected '\"' but found null delimiter");

        TEST_RESULT_STR(strPtr(varStr(jsonToVar(strNew(" \"test\"")))), "test", "simple string");
        TEST_RESULT_STR(strPtr(varStr(jsonToVar(strNew("\"te\\\"st\" ")))), "te\"st", "string with quote");
        TEST_RESULT_STR(
            strPtr(varStr(jsonToVar(strNew("\"\\\"\\\\\\/\\b\\n\\r\\t\\f\"")))), "\"\\/\b\n\r\t\f", "string with escapes");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(jsonToVar(strNew("-")), JsonFormatError, "found '-' with no integer at '-'");
        TEST_RESULT_INT(varUInt64(jsonToVar(strNew(" 5555555555"))), 5555555555, "simple integer");
        TEST_RESULT_INT(varInt64(jsonToVar(strNew("-5555555555 "))), -5555555555, "negative integer");

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
        TEST_RESULT_INT(varUInt64(varLstGet(valueList, 0)), 1, "check array int");
        TEST_RESULT_STR(strPtr(varStr(varLstGet(valueList, 1))), "test", "check array str");
        TEST_RESULT_BOOL(varBool(varLstGet(valueList, 2)), false, "check array bool");

        TEST_ASSIGN(valueList, varVarLst(jsonToVar(strNew("[ ]"))), "empty array");
        TEST_RESULT_UINT(varLstSize(valueList), 0, "check array size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(jsonToVar(strNew("{\"key1\"= 747}")), JsonFormatError, "expected ':' at '= 747}'");
        TEST_ERROR(jsonToVar(strNew("{\"key1\" : 747'")), JsonFormatError, "expected '}' at '''");
        TEST_ERROR(jsonToVar(strNew("{key1")), JsonFormatError, "expected '\"' at 'key1'");

        KeyValue *kv = NULL;
        TEST_ASSIGN(kv, varKv(jsonToVar(strNew("{\"key1\": 747, \"key2\":\"value2\",\"key3\"\t:\t[\t] }"))), "object");
        TEST_RESULT_UINT(varLstSize(kvKeyList(kv)), 3, "check key total");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, varNewStr(strNew("key1")))), 747, "check object uint");
        TEST_RESULT_STR(strPtr(varStr(kvGet(kv, varNewStr(strNew("key2"))))), "value2", "check object str");
        TEST_RESULT_UINT(varLstSize(varVarLst(kvGet(kv, varNewStr(strNew("key3"))))), 0, "check empty array");

        TEST_ASSIGN(kv, varKv(jsonToVar(strNew("\t{\n} "))), "empty object");
        TEST_RESULT_UINT(varLstSize(kvKeyList(kv)), 0, "check key total");
    }

    // *****************************************************************************************************************************
    if (testBegin("kvToJson(), kvToJsonInternal()"))
    {
        KeyValue *keyValue = kvNew();
        String *json = NULL;

        TEST_ASSIGN(json, kvToJson(keyValue, 0), "kvToJson - empty, no indent");
        TEST_RESULT_STR(strPtr(json), "{}", "  empty curly brackets");

        TEST_ASSIGN(json, kvToJson(keyValue, 2), "kvToJson - empty, indent 2");
        TEST_RESULT_STR(strPtr(json), "{}\n", "  empty curly brackets with carriage return");

        kvPut(keyValue, varNewStrZ("backup"), varNewVarLst(varLstNew()));
        TEST_ASSIGN(json, kvToJson(keyValue, 0), "kvToJson - kv with empty array, no indent");
        TEST_RESULT_STR(strPtr(json), "{\"backup\":[]}", "  kv with empty array brackets");

        kvPut(keyValue, varNewStrZ("backup"), varNewVarLst(varLstNew()));
        TEST_ASSIGN(json, kvToJson(keyValue, 2), "kvToJson - kv with empty array, indent 2");
        TEST_RESULT_STR(strPtr(json),
            "{\n"
            "  \"backup\" : []\n"
            "}\n", "  formatted kv with empty array brackets");

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

        TEST_ASSIGN(json, kvToJson(keyValue, 2), "kvToJson - kv with empty array, indent 2");
        TEST_RESULT_STR(strPtr(json),
            "{\n"
            "  \"archive\" : {},\n"
            "  \"backup\" : [],\n"
            "  \"bool1\" : true,\n"
            "  \"bool2\" : false,\n"
            "  \"checknull\" : null,\n"
            "  \"db\" : [\n"
            "    {\n"
            "      \"id\" : 1,\n"
            "      \"version\" : \"9.4\"\n"
            "    },\n"
            "    null\n"
            "  ],\n"
            "  \"empty\" : {},\n"
            "  \"null-list\" : null,\n"
            "  \"null-str\" : null\n"
            "}\n", "  formatted kv with empty array, kv, varList with values");

        TEST_ASSIGN(
            keyValue,
            varKv(
                jsonToVar(
                    strNew(
                    "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
                    "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\"],"
                    "\"checksum-page-error\":[1],\"backup-timestamp-start\":1482182951}"))),
            "multpile values with array");
        TEST_ASSIGN(json, kvToJson(keyValue, 0), "  kvToJson - sorted, no indent");
        TEST_RESULT_STR(strPtr(json),
            "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\"],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1]}",
            "  check string no pretty print");
    }

    // *****************************************************************************************************************************
    if (testBegin("varToJson()"))
    {
        TEST_ERROR(varToJson(varNewUInt64(100), 0), JsonFormatError, "variant type is invalid");

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

        TEST_ASSIGN(json, varToJson(keyValue, 0), "KeyValue no indent");
        TEST_RESULT_STR(strPtr(json),
            "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\",null],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1],"
            "\"section\":{\"escape\":\"\\\"\\\\\\/\\b\\n\\r\\t\\f\",\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}}",
            "  sorted json string result, no pretty print");

        TEST_ASSIGN(json, varToJson(keyValue, 4), "KeyValue - indent 4");
        TEST_RESULT_STR(strPtr(json),
            "{\n"
            "    \"backup-info-size-delta\" : 1982702,\n"
            "    \"backup-prior\" : \"20161219-212741F_20161219-212803I\",\n"
            "    \"backup-reference\" : [\n"
            "        \"20161219-212741F\",\n"
            "        \"20161219-212741F_20161219-212803I\",\n"
            "        null\n"
            "    ],\n"
            "    \"backup-timestamp-start\" : 1482182951,\n"
            "    \"checksum-page-error\" : [\n"
            "        1\n"
            "    ],\n"
            "    \"section\" : {\n"
            "        \"escape\" : \"\\\"\\\\\\/\\b\\n\\r\\t\\f\",\n"
            "        \"key1\" : \"value1\",\n"
            "        \"key2\" : null,\n"
            "        \"key3\" : \"value2\"\n"
            "    }\n"
            "}\n",
            "  sorted json string result");

        //--------------------------------------------------------------------------------------------------------------------------
        Variant *varListOuter = NULL;

        TEST_ASSIGN(json, varToJson(varNewVarLst(varLstNew()), 0), "VariantList");
        TEST_RESULT_STR(strPtr(json), "[]", "  empty list no pretty print");

        TEST_ASSIGN(varListOuter, varNewVarLst(varLstNew()), "new variant list with keyValues");
        varLstAdd(varVarLst(varListOuter), keyValue);

        TEST_ASSIGN(json, varToJson(varListOuter, 0), "VariantList - no indent");
        TEST_RESULT_STR(strPtr(json),
            "[{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\",null],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1],"
            "\"section\":{\"escape\":\"\\\"\\\\\\/\\b\\n\\r\\t\\f\",\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}}]",
            "  sorted json string result no pretty print");

        Variant *keyValue2 = varDup(keyValue);
        varLstAdd(varVarLst(varListOuter), keyValue2);

        TEST_ASSIGN(json, varToJson(varListOuter, 0), "VariantList - no indent - multiple elements");
        TEST_RESULT_STR(strPtr(json),
            "[{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\",null],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1],"
            "\"section\":{\"escape\":\"\\\"\\\\\\/\\b\\n\\r\\t\\f\",\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}},"
            "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\",null],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1],"
            "\"section\":{\"escape\":\"\\\"\\\\\\/\\b\\n\\r\\t\\f\",\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}}]",
            "  sorted json string result no pretty print");

        TEST_ASSIGN(json, varToJson(varListOuter, 2), "VariantList - indent 2 - multiple elements");
        TEST_RESULT_STR(strPtr(json),
            "[\n"
            "  {\n"
            "    \"backup-info-size-delta\" : 1982702,\n"
            "    \"backup-prior\" : \"20161219-212741F_20161219-212803I\",\n"
            "    \"backup-reference\" : [\n"
            "      \"20161219-212741F\",\n"
            "      \"20161219-212741F_20161219-212803I\",\n"
            "      null\n"
            "    ],\n"
            "    \"backup-timestamp-start\" : 1482182951,\n"
            "    \"checksum-page-error\" : [\n"
            "      1\n"
            "    ],\n"
            "    \"section\" : {\n"
            "      \"escape\" : \"\\\"\\\\\\/\\b\\n\\r\\t\\f\",\n"
            "      \"key1\" : \"value1\",\n"
            "      \"key2\" : null,\n"
            "      \"key3\" : \"value2\"\n"
            "    }\n"
            "  },\n"
            "  {\n"
            "    \"backup-info-size-delta\" : 1982702,\n"
            "    \"backup-prior\" : \"20161219-212741F_20161219-212803I\",\n"
            "    \"backup-reference\" : [\n"
            "      \"20161219-212741F\",\n"
            "      \"20161219-212741F_20161219-212803I\",\n"
            "      null\n"
            "    ],\n"
            "    \"backup-timestamp-start\" : 1482182951,\n"
            "    \"checksum-page-error\" : [\n"
            "      1\n"
            "    ],\n"
            "    \"section\" : {\n"
            "      \"escape\" : \"\\\"\\\\\\/\\b\\n\\r\\t\\f\",\n"
            "      \"key1\" : \"value1\",\n"
            "      \"key2\" : null,\n"
            "      \"key3\" : \"value2\"\n"
            "    }\n"
            "  }\n"
            "]\n",
            "  sorted json string result, pretty print");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
