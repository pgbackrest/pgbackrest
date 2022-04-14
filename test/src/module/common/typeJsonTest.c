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
        TEST_TITLE("scalar");

        JsonRead *read = NULL;

        TEST_ASSIGN(read, jsonReadNew(STRDEF("\rtrue\t")), "new read");

        TEST_RESULT_BOOL(jsonReadBool(read), true, "bool true");
        TEST_ERROR(jsonReadBool(read), FormatError, "JSON read is complete");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("complex");

        const String *json = STRDEF(
            "\t\r\n "                                               // Leading whitespace
            "[\n"
            "    {"
            "        \"key1\"\t: \"value1\","
            "        \"key2\"\t: 777,"
            "        \"key3\"\t: 777,"
            "        \"key4\"\t: 888"
            "    },"
            "    \"abc\","
            "    123,"
            "    18446744073709551615,"
            "    -1,"
            "    -9223372036854775807,"
            "    true,"
            "    false\n"
            "]"
            "] ");                                                  // Extra JSON and whitespace at the end

        TEST_ASSIGN(read, jsonReadNew(json), "new read");

        TEST_RESULT_VOID(jsonReadArrayBegin(read), "array begin");
        TEST_ERROR(jsonReadArrayEnd(read), FormatError, "expected ] but found {");

        TEST_RESULT_VOID(jsonReadObjectBegin(read), "object begin");
        TEST_RESULT_STR_Z(jsonReadKey(read), "key1", "key1");
        TEST_RESULT_STR_Z(jsonReadStr(read), "value1", "str");
        TEST_RESULT_STR_Z(jsonReadKey(read), "key2", "key2");
        TEST_RESULT_INT(jsonReadInt(read), 777, "int");
        TEST_RESULT_BOOL(jsonReadKeyExpect(read, STRDEF("key4")), true, "expect key4");
        TEST_RESULT_INT(jsonReadInt(read), 888, "int");
        TEST_RESULT_BOOL(jsonReadKeyExpect(read, STRDEF("key5")), false, "do not expect key5");
        TEST_RESULT_VOID(jsonReadObjectEnd(read), "object end");

        TEST_RESULT_STR_Z(jsonReadStr(read), "abc", "str");
        TEST_RESULT_UINT(jsonReadUInt(read), 123, "uint");
        TEST_RESULT_UINT(jsonReadUInt64(read), 18446744073709551615U, "uint64");
        TEST_RESULT_INT(jsonReadInt(read), -1, "int");
        TEST_RESULT_INT(jsonReadInt64(read), -9223372036854775807L, "int64");
        TEST_RESULT_BOOL(jsonReadBool(read), true, "bool true");
        TEST_RESULT_BOOL(jsonReadBool(read), false, "bool false");

        TEST_RESULT_VOID(jsonReadArrayEnd(read), "array end");
        TEST_ERROR(jsonReadArrayEnd(read), FormatError, "JSON read is complete");

        TEST_RESULT_VOID(jsonReadFree(read), "free");
    }

    // *****************************************************************************************************************************
    if (testBegin("JsonWrite"))
    {
        // JsonWrite *write = NULL;

        // TEST_ASSIGN(write, jsonWriteNewP(), "new write");

        // TEST_RESULT_VOID(jsonWriteArrayBegin(write), "write array begin");
        // TEST_RESULT_VOID(jsonWriteBool(write, true), "write bool true");
        // TEST_RESULT_VOID(jsonWriteInt(write, 55), "write int 55");
        // TEST_RESULT_VOID(jsonWriteStr(write, STRDEF("two\nlines")), "write str with two lines");

        // TEST_RESULT_VOID(jsonWriteObjectBegin(write), "write object begin");
        // TEST_ERROR(jsonWriteBool(write, false), FormatError, "key has not been defined");
        // TEST_RESULT_VOID(jsonWriteKey(write, STRDEF("flag")), "write key 'flag'");
        // TEST_ERROR(jsonWriteKey(write, STRDEF("flag")), FormatError, "key has already been defined");
        // TEST_RESULT_VOID(jsonWriteBool(write, false), "write bool false");
        // TEST_RESULT_VOID(jsonWriteKey(write, STRDEF("val")), "write key 'val'");
        // TEST_RESULT_VOID(jsonWriteUInt64(write, UINT64_MAX), "write uint64 max");
        // TEST_ERROR(jsonWriteKey(write, STRDEF("a")), FormatError, "key 'a' is not after prior key 'val'");
        // TEST_RESULT_VOID(jsonWriteObjectEnd(write), "write object end");

        // TEST_RESULT_VOID(jsonWriteUInt(write, 66), "write int 66");
        // TEST_RESULT_VOID(jsonWriteArrayEnd(write), "write array end");

        // TEST_ERROR(jsonWriteUInt(write, 66), FormatError, "JSON is complete and nothing more may be added");

        // TEST_RESULT_STR_Z(
        //     jsonWriteResult(write), "[true,55,\"two\\nlines\",{\"flag\":false,\"val\":18446744073709551615},66]",
        //     "json result");

        // TEST_RESULT_VOID(jsonWriteFree(write), "free");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
