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
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
