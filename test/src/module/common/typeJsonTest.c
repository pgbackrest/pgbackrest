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
        // CSHANG Should we handle this as an empty array? Should we set it to NULL? Right now in the backup.info file, it appears
        // that only the backup-reference is an array - and if that is empty, something is REALLY wrong since it's only set on non-full
        TEST_ERROR(jsonToKv(strNew("{\"key1\":[]}")), JsonFormatError, "unknown array value type");

        // -------------------------------------------------------------------------------------------------------------------------
        KeyValue *kv = NULL;

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":123}")), "single integer value");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, varNewStr(strNew("key1")))), 123, "  check integer");

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":\"value1\"}")), "single string value");
        TEST_RESULT_STR(strPtr(varStr(kvGet(kv, varNewStr(strNew("key1"))))), "value1", "  check string");

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":true}")), "boolean true");
        TEST_RESULT_BOOL(varBool(kvGet(kv, varNewStr(strNew("key1")))), true, "  check boolean");

        TEST_ASSIGN(kv, jsonToKv(strNew("{\"key1\":false}")), "boolean true");
        TEST_RESULT_BOOL(varBool(kvGet(kv, varNewStr(strNew("key1")))), false, "  check boolean");

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

        // CSHANG If empty string, should it be converted to NULL as the output? Probably not - should only do that if null is an
        // element of the array - do we even have that in the info or manifest files? I know we have {} so is that a NULL object or
        // empty object?
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
                "\"checksum-page-error\":[1],\"backup-timestamp-start\":1482182951}")),
            "multpile values with array");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, varNewStr(strNew("backup-info-size-delta")))), 1982702, "  check integer");
        TEST_RESULT_STR(strPtr(varStr(kvGet(kv, varNewStr(strNew("backup-prior"))))), "20161219-212741F_20161219-212803I",
            "  check string");
        TEST_RESULT_STR(strPtr(varStr(varLstGet(varVarLst(kvGet(kv, varNewStr(strNew("backup-reference")))), 0))),
            "20161219-212741F", "  check string array[0]");
        TEST_RESULT_STR(strPtr(varStr(varLstGet(varVarLst(kvGet(kv, varNewStr(strNew("backup-reference")))), 1))),
            "20161219-212741F_20161219-212803I", "  check string array[1]");
        TEST_RESULT_UINT(varUInt64(varLstGet(varVarLst(kvGet(kv, varNewStr(strNew("checksum-page-error")))), 0)), 1,
            "  check integer array[0]");
        TEST_RESULT_UINT(varUInt64(kvGet(kv, varNewStr(strNew("backup-timestamp-start")))), 1482182951, "  check integer");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
