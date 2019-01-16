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
    if (testBegin("jsonToKv()"))
    {
        TEST_ERROR(jsonToKv(strNew("[")), JsonFormatError, "arrays not supported");
        TEST_ERROR(jsonToKv(strNew("<")), JsonFormatError, "expected '{' but found '<'");
        TEST_ERROR(jsonToKv(strNew("{>")), JsonFormatError, "expected '\"' but found '>'");
        TEST_ERROR(jsonToKv(strNew("{\"\"")), JsonFormatError, "zero-length key not allowed");
        TEST_ERROR(jsonToKv(strNew("{\"key1\"")), JsonFormatError, "expected ':' but found '");
        TEST_ERROR(jsonToKv(strNew("{\"key1'")), JsonFormatError, "expected '\"' but found '''");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":x")), JsonFormatError, "unknown value type");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":t")), JsonFormatError, "expected boolean but found 't'");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":toe")), JsonFormatError, "expected 'true' or 'false' but found 'toe'");
        TEST_ERROR(jsonToKv(strNew("{\"checksum\":\"6721d92c9fcdf4248acff1f9a13\",\"master\":falsehood,\"reference\":\"backup\"}")),
            JsonFormatError, "expected '}' but found 'h'");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":123")), JsonFormatError, "expected '}' but found '3'");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":123>")), JsonFormatError, "expected '}' but found '>'");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":\"123'}")), JsonFormatError, "expected '\"' but found '}'");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":[\"\",]}")), JsonFormatError, "unknown array value type");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":[\"\",1]}")), JsonFormatError, "number found in array of type 's'");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":[1,\"\"]}")), JsonFormatError, "string found in array of type 'n'");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":[1}")), JsonFormatError, "expected array delimeter ']' but found '}'");
        TEST_ERROR(jsonToKv(strNew("{}")), JsonFormatError, "expected '\"' but found '}'");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":nu")), JsonFormatError, "expected null delimit but found 'u'");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":nul")), JsonFormatError, "null delimit not found");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":nult")), JsonFormatError, "null delimit not found");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":noll")), JsonFormatError, "expected 'null' but found 'noll'");
        TEST_ERROR(jsonToKv(strNew("{\"key1\":nully}")), JsonFormatError, "expected '}' but found 'y'");

        // -------------------------------------------------------------------------------------------------------------------------
        KeyValue *kv = NULL;

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":123}")), "single integer value");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, varNewStr(strNew("key1")))), 123, "  check integer");

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":\"value1\"}")), "single string value");
        TEST_RESULT_STR(strPtr(varStr(kvGet(kv, varNewStr(strNew("key1"))))), "value1", "  check string");

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":true}")), "boolean true");
        TEST_RESULT_BOOL(varBool(kvGet(kv, varNewStr(strNew("key1")))), true, "  check boolean");

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":false}")), "boolean false");
        TEST_RESULT_BOOL(varBool(kvGet(kv, varNewStr(strNew("key1")))), false, "  check boolean");

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":null}")), "single null value");
        TEST_RESULT_PTR(varStr(kvGet(kv, varNewStr(strNew("key1")))), NULL, "  check null");

        TEST_ASSIGN(
            kv,
            jsonToKv(
                strNew(
                    "{\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6116111691796124355,"
                    "\"db-version\":\"9.4\"}")),
            "multiple values");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, varNewStr(strNew("db-catalog-version")))), 201409291, "  check integer");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, varNewStr(strNew("db-control-version")))), 942, "  check integer");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, varNewStr(strNew("db-system-id")))), 6116111691796124355, "  check integer");
        TEST_RESULT_STR(strPtr(varStr(kvGet(kv, varNewStr(strNew("db-version"))))), "9.4", "  check string");

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":[\"\"]}")), "single-dimension empty string array");
        TEST_RESULT_STR(strPtr(varStr(varLstGet(varVarLst(kvGet(kv, varNewStr(strNew("key1")))), 0))), "", "  array[0]");

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":[\"12AB\",\"34-EF\"]}")), "single-dimension string array");
        TEST_RESULT_STR(strPtr(varStr(varLstGet(varVarLst(kvGet(kv, varNewStr(strNew("key1")))), 0))), "12AB", "  array[0]");
        TEST_RESULT_STR(strPtr(varStr(varLstGet(varVarLst(kvGet(kv, varNewStr(strNew("key1")))), 1))), "34-EF", "  array[1]");

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":[12,34]}")), "single-dimension number array");
        TEST_RESULT_UINT(varUInt64(varLstGet(varVarLst(kvGet(kv, varNewStr(strNew("key1")))), 0)), 12, "  array[0]");
        TEST_RESULT_UINT(varUInt64(varLstGet(varVarLst(kvGet(kv, varNewStr(strNew("key1")))), 1)), 34, "  array[1]");

        TEST_ASSIGN(
            kv,
            jsonToKv(
                strNew(
                "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
                "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\"],"
                "\"empty-array\":[],"
                "\"null-value\":null,\"checksum-page-error\":[1],\"backup-timestamp-start\":1482182951}")),
            "multpile values with array and null");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, varNewStr(strNew("backup-info-size-delta")))), 1982702, "  check integer");
        TEST_RESULT_STR(strPtr(varStr(kvGet(kv, varNewStr(strNew("backup-prior"))))), "20161219-212741F_20161219-212803I",
            "  check string");
        TEST_RESULT_STR(strPtr(varStr(varLstGet(varVarLst(kvGet(kv, varNewStr(strNew("backup-reference")))), 0))),
            "20161219-212741F", "  check string array[0]");
        TEST_RESULT_STR(strPtr(varStr(varLstGet(varVarLst(kvGet(kv, varNewStr(strNew("backup-reference")))), 1))),
            "20161219-212741F_20161219-212803I", "  check string array[1]");
        TEST_RESULT_UINT(varLstSize(varVarLst(kvGet(kv, varNewStr(strNew("empty-array"))))), 0, "  check empty array");
        TEST_RESULT_PTR(varStr(kvGet(kv, varNewStr(strNew("null-value")))), NULL, "  check null");
        TEST_RESULT_UINT(varUInt64(varLstGet(varVarLst(kvGet(kv, varNewStr(strNew("checksum-page-error")))), 0)), 1,
            "  check integer array[0]");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, varNewStr(strNew("backup-timestamp-start")))), 1482182951, "  check integer");
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
        Variant *dbInfo = varNewKv();
        kvPut(varKv(dbInfo), varNewStr(strNew("id")), varNewInt(1));
        kvPut(varKv(dbInfo), varNewStr(strNew("version")),  varNewStr(strNew("9.4")));
        varLstAdd(dbList, dbInfo);
        kvPut(keyValue, varNewStrZ("db"), varNewVarLst(dbList));

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
            "    }\n"
            "  ],\n"
            "  \"empty\" : {},\n"
            "  \"null-str\" : null\n"
            "}\n", "  formatted kv with empty array, kv, varList with values");

        TEST_ASSIGN(
            keyValue,
            jsonToKv(
                strNew(
                "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
                "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\"],"
                "\"checksum-page-error\":[1],\"backup-timestamp-start\":1482182951}")),
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

        TEST_ASSIGN(keyValue, varNewKv(), "build new kv");
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

        TEST_ASSIGN(json, varToJson(keyValue, 0), "KeyValue no indent");
        TEST_RESULT_STR(strPtr(json),
            "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\",null],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1],"
            "\"section\":{\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}}",
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
            "\"section\":{\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}}]",
            "  sorted json string result no pretty print");

        Variant *keyValue2 = varDup(keyValue);
        varLstAdd(varVarLst(varListOuter), keyValue2);

        TEST_ASSIGN(json, varToJson(varListOuter, 0), "VariantList - no indent - multiple elements");
        TEST_RESULT_STR(strPtr(json),
            "[{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\",null],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1],"
            "\"section\":{\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}},"
            "{\"backup-info-size-delta\":1982702,\"backup-prior\":\"20161219-212741F_20161219-212803I\","
            "\"backup-reference\":[\"20161219-212741F\",\"20161219-212741F_20161219-212803I\",null],"
            "\"backup-timestamp-start\":1482182951,\"checksum-page-error\":[1],"
            "\"section\":{\"key1\":\"value1\",\"key2\":null,\"key3\":\"value2\"}}]",
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
