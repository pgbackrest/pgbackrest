/***********************************************************************************************************************************
Test Variant Data Type
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("bool"))
    {
        Variant *boolean = varNewBool(false);

        boolean->type = varTypeString;
        TEST_ERROR(varBool(boolean), AssertError, "variant type is not bool");

        boolean->type = varTypeBool;
        varFree(boolean);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varBool(varNewBool(true)), true, "true bool variant");
        TEST_RESULT_BOOL(varBool(varNewBool(false)), false, "false bool variant");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varBoolForce(varNewBool(false)), false, "force bool to bool");
        TEST_RESULT_BOOL(varBoolForce(varNewInt(1)), true, "force int to bool");
        TEST_RESULT_BOOL(varBoolForce(varNewInt64(false)), false, "force int64 to bool");

        TEST_ERROR(varBoolForce(varNewVarLstEmpty()), FormatError, "unable to force VariantList to bool");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(varFree(NULL), "ok to free null variant");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varBool(varDup(varNewBool(true))), true, "dup bool");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varEq(varNewBool(true), varNewInt(1)), false, "bool, int not eq");

        TEST_RESULT_BOOL(varEq(varNewBool(true), varNewBool(true)), true, "bool, bool eq");
        TEST_RESULT_BOOL(varEq(varNewBool(false), varNewBool(true)), false, "bool, bool not eq");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("double"))
    {
        Variant *var = varNewDbl(44.44);
        TEST_RESULT_DOUBLE(varDbl(var), 44.44, "double variant");
        varFree(var);

        // -------------------------------------------------------------------------------------------------------------------------
        var = varNewDbl(-1.1);

        var->type = varTypeString;
        TEST_ERROR(varDbl(var), AssertError, "variant type is not double");

        var->type = varTypeDouble;
        varFree(var);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_DOUBLE(varDblForce(varNewDbl(4.567)), 4.567, "force double to double");
        TEST_RESULT_DOUBLE(varDblForce(varNewBool(false)), 0, "force bool to double");
        TEST_RESULT_DOUBLE(varDblForce(varNewInt(123)), 123, "force int to double");
        TEST_RESULT_DOUBLE(varDblForce(varNewInt64(999999999999)), 999999999999, "force int64 to double");
        TEST_RESULT_DOUBLE(varDblForce(varNewStr(strNew("879.01"))), 879.01, "force String to double");
        TEST_RESULT_DOUBLE(varDblForce(varNewStr(strNew("0"))), 0, "force String to double");

        TEST_ERROR(varDblForce(varNewStr(strNew("AAA"))), FormatError, "unable to force String 'AAA' to double");
        TEST_ERROR(varDblForce(varNewVarLstEmpty()), FormatError, "unable to force VariantList to double");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_DOUBLE(varDbl(varDup(varNewDbl(3.1415))), 3.1415, "dup double");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varEq(varNewDbl(1.234), varNewDbl(1.234)), true, "double, double eq");
        TEST_RESULT_BOOL(varEq(varNewDbl(4.321), varNewDbl(1.234)), false, "double, double not eq");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("int"))
    {
        Variant *integer = varNewInt(44);
        TEST_RESULT_INT(varInt(integer), 44, "int variant");
        TEST_RESULT_INT(varIntForce(integer), 44, "force int to int");
        varFree(integer);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varIntForce(varNewBool(true)), 1, "force bool to int");
        TEST_ERROR(varIntForce(varNewVarLstEmpty()), FormatError, "unable to force VariantList to int");
        TEST_RESULT_INT(varIntForce(varNewInt64(999)), 999, "force int64 to int");
        TEST_ERROR(varIntForce(varNewInt64(2147483648)), AssertError, "unable to convert int64 2147483648 to int");
        TEST_ERROR(varIntForce(varNewInt64(-2147483649)), AssertError, "unable to convert int64 -2147483649 to int");

        // -------------------------------------------------------------------------------------------------------------------------
        integer = varNewInt(-1);

        integer->type = varTypeString;
        TEST_ERROR(varInt(integer), AssertError, "variant type is not int");

        integer->type = varTypeInt;
        varFree(integer);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varInt(varDup(varNewInt(88976))), 88976, "dup int");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varEq(NULL, NULL), true, "null, null eq");
        TEST_RESULT_BOOL(varEq(NULL, varNewInt(123)), false, "null, int not eq");
        TEST_RESULT_BOOL(varEq(varNewInt(123), NULL), false, "int, null not eq");

        TEST_RESULT_BOOL(varEq(varNewInt(123), varNewInt(123)), true, "int, int eq");
        TEST_RESULT_BOOL(varEq(varNewInt(444), varNewInt(123)), false, "int, int not eq");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("int64"))
    {
        Variant *integer = varNewInt64(44);
        TEST_RESULT_INT(varInt64(integer), 44, "int64 variant");
        TEST_RESULT_INT(varInt64Force(integer), 44, "force int64 to int64");
        varFree(integer);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varInt64Force(varNewBool(true)), 1, "force bool to int64");
        TEST_RESULT_INT(varInt64Force(varNewInt(2147483647)), 2147483647, "force int to int64");
        TEST_RESULT_INT(varInt64Force(varNewStrZ("9223372036854775807")), 9223372036854775807L, "force str to int64");

        TEST_ERROR(
            varInt64Force(varNewStrZ("9923372036854775807")), FormatError,
            "unable to convert String '9923372036854775807' to int64");
        TEST_ERROR(varInt64Force(varNewVarLstEmpty()), FormatError, "unable to force VariantList to int64");

        // -------------------------------------------------------------------------------------------------------------------------
        integer = varNewInt64(-1);

        integer->type = varTypeString;
        TEST_ERROR(varInt64(integer), AssertError, "variant type is not int64");

        integer->type = varTypeInt;
        varFree(integer);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varInt64(varDup(varNewInt64(88976))), 88976, "dup int64");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varEq(NULL, NULL), true, "null, null eq");
        TEST_RESULT_BOOL(varEq(NULL, varNewInt64(123)), false, "null, int64 not eq");

        TEST_RESULT_BOOL(varEq(varNewInt64(9223372036854775807L), varNewInt64(9223372036854775807L)), true, "int64, int64 eq");
        TEST_RESULT_BOOL(varEq(varNewInt64(444), varNewInt64(123)), false, "int64, int64 not eq");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("keyValue"))
    {
        TEST_ERROR(varKv(varNewInt(66)), AssertError, "variant type is not 'KeyValue'");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *keyValue = NULL;

        TEST_ASSIGN(keyValue, varNewKv(), "new");
        TEST_RESULT_PTR(kvPut(varKv(keyValue), varNewInt(44), varNewInt(55)), varKv(keyValue), "    put int/int");
        TEST_RESULT_INT(varInt(kvGet(varKv(keyValue), varNewInt(44))), 55, "    get int/int");
        TEST_RESULT_PTR(varKv(NULL), NULL, "get null kv");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *keyValueDup = NULL;

        TEST_ASSIGN(keyValueDup, varDup(keyValue), "duplicate");
        TEST_RESULT_INT(varInt(kvGet(varKv(keyValueDup), varNewInt(44))), 55, "    get int/int");

        varFree(keyValue);
        varFree(keyValueDup);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(varEq(varNewKv(), varNewKv()), AssertError, "unable to test equality for KeyValue");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("String"))
    {
        TEST_ERROR(varNewStr(NULL), AssertError, "string variant cannot be NULL");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *string = varNewStr(strNew("test-str"));
        TEST_RESULT_STR(strPtr(varStr(string)), "test-str", "string pointer");
        varFree(string);

        TEST_RESULT_PTR(varStr(NULL), NULL, "get null string variant");

        // -------------------------------------------------------------------------------------------------------------------------
        string = varNewStr(strNew("not-a-string"));
        string->type = varTypeInt;
        TEST_ERROR(strPtr(varStr(string)), AssertError, "variant type is not string");
        varFree(string);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varIntForce(varNewStrZ("777")), 777, "int from string");
        TEST_RESULT_INT(varIntForce(varNewStrZ("0")), 0, "int from string");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varBoolForce(varNewStr(strNew("y"))), true, "bool from string");
        TEST_RESULT_INT(varBoolForce(varNewStr(strNew("on"))), true, "bool from string");

        TEST_RESULT_INT(varBoolForce(varNewStr(strNew("n"))), false, "bool from string");
        TEST_RESULT_INT(varBoolForce(varNewStr(strNew("off"))), false, "bool from string");

        TEST_RESULT_INT(varBoolForce(varNewStr(strNew("OFF"))), false, "bool from string");

        TEST_ERROR(varBoolForce(varNewStr(strNew(BOGUS_STR))), FormatError, "unable to convert str 'BOGUS' to bool");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(varStrForce(varNewStr(strNew("teststring")))), "teststring", "force string to string");
        TEST_RESULT_STR(strPtr(varStrForce(varNewInt(999))), "999", "force int to string");
        TEST_RESULT_STR(strPtr(varStrForce(varNewInt64(9223372036854775807L))), "9223372036854775807", "force int64 to string");
        TEST_RESULT_STR(strPtr(varStrForce(varNewDbl(999.1234))), "999.1234", "force double to string");
        TEST_RESULT_STR(strPtr(varStrForce(varNewDbl((double)999999999.123456))), "999999999.123456", "force double to string");
        TEST_RESULT_STR(strPtr(varStrForce(varNewDbl(999.0))), "999", "force double to string");
        TEST_RESULT_STR(strPtr(varStrForce(varNewDbl(9990.0))), "9990", "force double to string");
        TEST_RESULT_STR(strPtr(varStrForce(varNewBool(true))), "true", "force bool to string");
        TEST_RESULT_STR(strPtr(varStrForce(varNewBool(false))), "false", "force bool to string");

        TEST_ERROR(varStrForce(varNewKv()), FormatError, "unable to force KeyValue to String");

        // -------------------------------------------------------------------------------------------------------------------------
        string = varNewStr(strNew("not-an-int"));
        TEST_ERROR(varIntForce(string), FormatError, "unable to convert str 'not-an-int' to int");
        varFree(string);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(varStr(varNewStrZ("test-z-str"))), "test-z-str", "new zero-terminated string");
        TEST_ERROR(varNewStrZ(NULL), AssertError, "zero-terminated string cannot be NULL");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(varStr(varDup(varNewStr(strNew("yabba-dabba-doo"))))), "yabba-dabba-doo", "dup string");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(varDup(NULL), NULL, "dup NULL");
        TEST_RESULT_BOOL(varEq(varNewStr(strNew("expect-equal")), varNewStr(strNew("expect-equal"))), true, "string, string eq");
        TEST_RESULT_BOOL(varEq(varNewStr(strNew("Y")), varNewStr(strNew("X"))), false, "string, string not eq");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("VariantList"))
    {
        TEST_ERROR(varVarLst(varNewInt(66)), AssertError, "variant type is not 'VariantList'");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *listVar = NULL;

        TEST_ASSIGN(listVar, varNewVarLstEmpty(), "new empty");

        TEST_RESULT_INT(varLstSize(varVarLst(listVar)), 0, "    empty size");
        TEST_RESULT_PTR(varVarLst(NULL), NULL, "get null var list");

        TEST_RESULT_PTR(varLstAdd(varVarLst(listVar), varNewBool(true)), varVarLst(listVar), "    add bool");
        TEST_RESULT_PTR(varLstAdd(varVarLst(listVar), varNewInt(55)), varVarLst(listVar), "    add int");

        TEST_RESULT_INT(varLstSize(varVarLst(listVar)), 2, "    size with items");

        TEST_RESULT_BOOL(varBool(varLstGet(varVarLst(listVar), 0)), true, "    get bool");
        TEST_RESULT_INT(varInt(varLstGet(varVarLst(listVar), 1)), 55, "    get int");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(listVar, varNewVarLst(varVarLst(listVar)), "new with items");
        TEST_RESULT_INT(varLstSize(varVarLst(listVar)), 2, "    size with items");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *listVarDup = NULL;

        TEST_ASSIGN(listVarDup, varDup(listVar), "duplicate");
        TEST_RESULT_INT(varLstSize(varVarLst(listVarDup)), 2, "    size with items");

        varFree(listVar);
        varFree(listVarDup);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(varEq(varNewVarLstEmpty(), varNewVarLstEmpty()), AssertError, "unable to test equality for VariantList");
    }
}
