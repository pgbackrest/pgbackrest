/***********************************************************************************************************************************
Test Key Value Data Type
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("kvNew() and kvFree()"))
    {
        KeyValue *store = NULL;

        TEST_ASSIGN(store, kvNew(), "new store");
        TEST_RESULT_PTR_NE(store->list, NULL, "list set");
        TEST_RESULT_INT(lstSize(store->list), 0, "list empty");

        TEST_RESULT_VOID(kvFree(store), "free kv");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("kvPut(), kvAdd(), kvKeyExists(), kvKeyList(), kvGet(), kvGetDefault(), kvGetList(), and kvDup()"))
    {
        KeyValue *store = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(store, kvNew(), "new store");
            TEST_RESULT_PTR(kvMove(NULL, memContextPrior()), NULL, "move null to old context");
            TEST_RESULT_PTR(kvMove(store, memContextPrior()), store, "move kv to old context");
        }
        MEM_CONTEXT_TEMP_END();

        // Set various data types
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(kvPut(store, VARSTRDEF("str-key"), VARSTRDEF("str-value")), store, "put string/string");
        TEST_RESULT_PTR(kvPut(store, varNewInt(42), varNewInt(57)), store, "put int/int");
        TEST_RESULT_PTR(kvPut(store, VARSTRDEF("str-key-int"), varNewInt(99)), store, "put string/int");
        TEST_RESULT_PTR(kvPut(store, varNewInt(78), NULL), store, "put int/null");

        // Get the types and make sure they have the correct value
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(varStr(kvGet(store, VARSTRDEF("str-key"))), "str-value", "get string/string");
        TEST_RESULT_INT(varInt(kvGet(store, varNewInt(42))), 57, "get int/int");
        TEST_RESULT_INT(varInt(varLstGet(kvGetList(store, varNewInt(42)), 0)), 57, "get int/int");
        TEST_RESULT_INT(varInt(kvGet(store, VARSTRDEF("str-key-int"))), 99, "get string/int");
        TEST_RESULT_PTR(kvGet(store, varNewInt(78)), NULL, "get int/null");
        TEST_RESULT_PTR(kvGetDefault(store, varNewInt(78), varNewInt(999)), NULL, "get int/null (default ignored)");
        TEST_RESULT_PTR(kvGet(store, varNewInt(777)), NULL, "get missing key");
        TEST_RESULT_INT(varInt(kvGetDefault(store, varNewInt(777), varNewInt(888))), 888, "get missing key with default");

        // Check key exists
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(kvKeyExists(store, VARSTRDEF("str-key")), true, "key exists");
        TEST_RESULT_BOOL(kvKeyExists(store, VARSTRDEF(BOGUS_STR)), false, "key does not exist");

        // Check that a null value can be changed to non-null
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(kvPut(store, varNewInt(78), varNewInt(66)), store, "update int/null to int/int");
        TEST_RESULT_INT(varInt(kvGet(store, varNewInt(78))), 66, "get int/int");

        // Check that a value can be changed
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(kvPut(store, varNewInt(78), varNewBool(false)), store, "update int/int to int/bool");
        TEST_RESULT_INT(varBool(kvGet(store, varNewInt(78))), false, "get int/bool");

        // Use add to create variant list
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(kvAdd(store, varNewInt(99), NULL), store, "add int/null");
        TEST_RESULT_PTR(kvAdd(store, varNewInt(99), varNewInt(1)), store, "add int/int");
        TEST_RESULT_PTR(kvAdd(store, varNewInt(99), varNewInt(2)), store, "add int/int");
        TEST_RESULT_PTR(kvAdd(store, varNewInt(99), varNewInt(3)), store, "add int/int");

        TEST_RESULT_INT(varInt(varLstGet(varVarLst(kvGet(store, varNewInt(99))), 0)), 1, "get int/int");
        TEST_RESULT_INT(varInt(varLstGet(varVarLst(kvGet(store, varNewInt(99))), 1)), 2, "get int/int");
        TEST_RESULT_INT(varInt(varLstGet(varVarLst(kvGet(store, varNewInt(99))), 2)), 3, "get int/int");

        TEST_RESULT_INT(varInt(varLstGet(kvGetList(store, varNewInt(99)), 2)), 3, "get int/int");
        TEST_RESULT_PTR(varLstGet(kvGetList(store, varNewInt(777)), 0), NULL, "get NULL list");

        // Check item in key list
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varInt(varLstGet(kvKeyList(store), 1)), 42, "key list");

        // Create a new kv and add it to this kv
        // -------------------------------------------------------------------------------------------------------------------------
        KeyValue *storeSub = kvPutKv(store, VARSTRDEF("kv-key"));

        kvPut(storeSub, VARSTRDEF("str-sub-key"), VARSTRDEF("str-sub-value"));
        TEST_RESULT_STR_Z(
            varStr(kvGet(varKv(kvGet(store, VARSTRDEF("kv-key"))), VARSTRDEF("str-sub-key"))),
            "str-sub-value", "get string/kv");

        // Duplicate the kv
        // -------------------------------------------------------------------------------------------------------------------------
        KeyValue *storeDup = kvDup(store);

        TEST_RESULT_INT(varBool(kvGet(store, varNewInt(78))), false, "get int/bool");
        TEST_RESULT_INT(varInt(varLstGet(varVarLst(kvGet(store, varNewInt(99))), 2)), 3, "get int/int");
        TEST_RESULT_STR_Z(
            varStr(kvGet(varKv(kvGet(storeDup, VARSTRDEF("kv-key"))), VARSTRDEF("str-sub-key"))),
            "str-sub-value", "get string/kv");

        TEST_RESULT_VOID(kvFree(storeDup), "free dup store");
        TEST_RESULT_VOID(kvFree(store), "free store");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
