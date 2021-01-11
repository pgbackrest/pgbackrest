/***********************************************************************************************************************************
Test Variant Data Type
***********************************************************************************************************************************/
#include "common/stackTrace.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("bool"))
    {
        // Ensure type sizes are as expected
        TEST_RESULT_UINT(sizeof(VariantBoolConst), 8, "check VariantBoolConst size");
        TEST_RESULT_UINT(sizeof(VariantBool), TEST_64BIT() ? 16 : 12, "check VariantBool size");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *boolean = varNewBool(false);
        TEST_RESULT_INT(varType(boolean), varTypeBool, "get bool type");
        varFree(boolean);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(varBool(varNewStrZ("string")), AssertError, "assertion 'this->type == varTypeBool' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varBool(BOOL_TRUE_VAR), true, "true bool variant");
        TEST_RESULT_BOOL(varBool(BOOL_FALSE_VAR), false, "false bool variant");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varBoolForce(VARBOOL(false)), false, "force bool to bool");
        TEST_RESULT_BOOL(varBoolForce(VARINT(1)), true, "force int to bool");
        TEST_RESULT_BOOL(varBoolForce(VARUINT(0)), false, "force uint to bool");
        TEST_RESULT_BOOL(varBoolForce(VARINT64(false)), false, "force int64 to bool");
        TEST_RESULT_BOOL(varBoolForce(VARUINT64(12)), true, "force uint64 to bool");

        TEST_ERROR(varBoolForce(varNewVarLst(varLstNew())), AssertError, "unable to force VariantList to bool");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(varFree(NULL), "ok to free null variant");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varBool(varDup(BOOL_TRUE_VAR)), true, "dup bool");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varEq(BOOL_TRUE_VAR, VARINT(1)), false, "bool, int not eq");

        TEST_RESULT_BOOL(varEq(BOOL_TRUE_VAR, BOOL_TRUE_VAR), true, "bool, bool eq");
        TEST_RESULT_BOOL(varEq(BOOL_FALSE_VAR, BOOL_TRUE_VAR), false, "bool, bool not eq");
    }

    // *****************************************************************************************************************************
    if (testBegin("int"))
    {
        // Ensure type sizes are as expected
        TEST_RESULT_UINT(sizeof(VariantIntConst), 8, "check VariantIntConst size");
        TEST_RESULT_UINT(sizeof(VariantInt), TEST_64BIT() ? 16 : 12, "check VariantInt size");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *integer = varNewInt(44);
        TEST_RESULT_INT(varInt(integer), 44, "int variant");
        TEST_RESULT_INT(varIntForce(integer), 44, "force int to int");
        varFree(integer);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varIntForce(varNewBool(true)), 1, "force bool to int");
        TEST_ERROR(varIntForce(varNewVarLst(varLstNew())), AssertError, "unable to force VariantList to int");
        TEST_RESULT_INT(varIntForce(VARINT64(999)), 999, "force int64 to int");
        TEST_ERROR(varIntForce(VARINT64(2147483648)), FormatError, "unable to convert int64 2147483648 to int");
        TEST_ERROR(varIntForce(VARINT64(-2147483649)), FormatError, "unable to convert int64 -2147483649 to int");
        TEST_RESULT_INT(varIntForce(VARUINT(54321)), 54321, "force uint to int");
        TEST_ERROR(varIntForce(VARUINT(2147483648)), FormatError, "unable to convert unsigned int 2147483648 to int");
        TEST_RESULT_INT(varIntForce(VARUINT64(12345)), 12345, "force uint64 to int");
        TEST_ERROR(varIntForce(VARUINT64(2147483648)), FormatError, "unable to convert uint64 2147483648 to int");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(varInt(varNewStrZ("string")), AssertError, "assertion 'this->type == varTypeInt' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varInt(varDup(VARINT(88976))), 88976, "dup int");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varEq(NULL, NULL), true, "null, null eq");
        TEST_RESULT_BOOL(varEq(NULL, VARINT(123)), false, "null, int not eq");
        TEST_RESULT_BOOL(varEq(VARINT(123), NULL), false, "int, null not eq");

        TEST_RESULT_BOOL(varEq(VARINT(123), VARINT(123)), true, "int, int eq");
        TEST_RESULT_BOOL(varEq(VARINT(444), VARINT(123)), false, "int, int not eq");
    }

    // *****************************************************************************************************************************
    if (testBegin("int64"))
    {
        // Ensure type sizes are as expected
        TEST_RESULT_UINT(sizeof(VariantInt64Const), TEST_64BIT() ? 16 : 12, "check VariantInt64Const size");
        TEST_RESULT_UINT(sizeof(VariantInt64), TEST_64BIT() ? 24 : 16, "check VariantInt64 size");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *integer = varNewInt64(44);
        TEST_RESULT_INT(varInt64(integer), 44, "int64 variant");
        TEST_RESULT_INT(varInt64Force(integer), 44, "force int64 to int64");
        varFree(integer);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varInt64Force(varNewBool(true)), 1, "force bool to int64");
        TEST_RESULT_INT(varInt64Force(VARINT(2147483647)), 2147483647, "force int to int64");
        TEST_RESULT_INT(varInt64Force(VARUINT(4294967295)), 4294967295, "force uint to int64");
        TEST_RESULT_INT(varInt64Force(varNewStrZ("9223372036854775807")), 9223372036854775807L, "force str to int64");
        TEST_RESULT_INT(varInt64Force(VARUINT64(9223372036854775807U)), 9223372036854775807L, "force uint64 to int64");

        TEST_ERROR(
            varInt64Force(varNewStrZ("9223372036854775808")), FormatError,
            "unable to convert base 10 string '9223372036854775808' to int64");
        TEST_ERROR(varInt64Force(varNewVarLst(varLstNew())), AssertError, "unable to force VariantList to int64");
        TEST_ERROR(varInt64Force(VARUINT64(9223372036854775808U)), FormatError,
            "unable to convert uint64 9223372036854775808 to int64");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(varInt64(varNewStrZ("string")), AssertError, "assertion 'this->type == varTypeInt64' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varInt64(varDup(VARINT64(88976))), 88976, "dup int64");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varEq(NULL, NULL), true, "null, null eq");
        TEST_RESULT_BOOL(varEq(NULL, VARINT64(123)), false, "null, int64 not eq");

        TEST_RESULT_BOOL(varEq(VARINT64(9223372036854775807L), VARINT64(9223372036854775807L)), true, "int64, int64 eq");
        TEST_RESULT_BOOL(varEq(VARINT64(444), VARINT64(123)), false, "int64, int64 not eq");
    }

    // *****************************************************************************************************************************
    if (testBegin("unsigned int"))
    {
        // Ensure type sizes are as expected
        TEST_RESULT_UINT(sizeof(VariantUIntConst), 8, "check VariantUIntConst size");
        TEST_RESULT_UINT(sizeof(VariantUInt), TEST_64BIT() ? 16 : 12, "check VariantUInt size");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *unsignedint = varNewUInt(787);
        TEST_RESULT_UINT(varUInt(unsignedint), 787, "uint variant");
        TEST_RESULT_UINT(varUIntForce(unsignedint), 787, "force uint to uint");
        varFree(unsignedint);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(varUIntForce(varNewBool(true)), 1, "force bool to uint");
        TEST_RESULT_UINT(varUIntForce(VARINT(2147483647)), 2147483647, "force int to uint");
        TEST_RESULT_UINT(varUIntForce(VARINT64(2147483647)), 2147483647, "force int64 to uint");
        TEST_RESULT_UINT(varUIntForce(varNewStrZ("4294967295")), 4294967295, "force str to uint");
        TEST_RESULT_UINT(varUIntForce(VARUINT64(4294967295U)), 4294967295U, "force uint64 to uint");

        TEST_ERROR(
            varUIntForce(varNewStrZ("4294967296")), FormatError,
            "unable to convert base 10 string '4294967296' to unsigned int");   // string value is out of bounds for uint
        TEST_ERROR(varUIntForce(varNewStrZ(" 16")), FormatError,"unable to convert base 10 string ' 16' to unsigned int");
        TEST_ERROR(varUIntForce(varNewVarLst(varLstNew())), AssertError, "unable to force VariantList to unsigned int");
        TEST_ERROR(varUIntForce(VARINT64(4294967296L)), FormatError, "unable to convert int64 4294967296 to unsigned int");
        TEST_ERROR(varUIntForce(VARUINT64(4294967296U)), FormatError, "unable to convert uint64 4294967296 to unsigned int");
        TEST_ERROR(varUIntForce(VARINT64(-1)), FormatError, "unable to convert int64 -1 to unsigned int");
        TEST_ERROR(varUIntForce(VARINT(-1)), FormatError, "unable to convert int -1 to unsigned int");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(varUInt(varNewStrZ("string")), AssertError, "assertion 'this->type == varTypeUInt' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(varUInt(varDup(VARUINT(88976))), 88976, "dup uint");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varEq(VARUINT(9999), VARUINT(9999)), true, "uint, uint eq");
        TEST_RESULT_BOOL(varEq(VARUINT(444), VARUINT(123)), false, "uint, uint not eq");
    }

    // *****************************************************************************************************************************
    if (testBegin("uint64"))
    {
        // Ensure type sizes are as expected
        TEST_RESULT_UINT(sizeof(VariantUInt64Const), TEST_64BIT() ? 16 : 12, "check VariantUInt64Const size");
        TEST_RESULT_UINT(sizeof(VariantUInt64), TEST_64BIT() ? 24 : 16, "check VariantUInt64 size");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *uint64 = varNewUInt64(44);
        TEST_RESULT_UINT(varUInt64(uint64), 44, "uint64 variant");
        TEST_RESULT_UINT(varUInt64Force(uint64), 44, "force uint64 to uint64");
        varFree(uint64);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(varUInt64Force(varNewBool(true)), 1, "force bool to uint64");
        TEST_RESULT_UINT(varUInt64Force(VARINT(2147483647)), 2147483647, "force int to uint64");
        TEST_RESULT_UINT(varUInt64Force(VARUINT(4294967295)), 4294967295, "force uint to uint64");
        TEST_RESULT_UINT(varUInt64Force(VARINT64(2147483647)), 2147483647, "force int64 to uint64");
        TEST_RESULT_UINT(varUInt64Force(varNewStrZ("18446744073709551615")), 18446744073709551615U, "force str to uint64");
        TEST_RESULT_UINT(varUInt64Force(VARUINT64(18446744073709551615U)), 18446744073709551615U, "force uint64 to uint64");

        TEST_ERROR(
            varUInt64Force(varNewStrZ("18446744073709551616")), FormatError,
            "unable to convert base 10 string '18446744073709551616' to uint64");   // string value is out of bounds for uint64
        TEST_ERROR(varUInt64Force(varNewStrZ(" 16")), FormatError,"unable to convert base 10 string ' 16' to uint64");
        TEST_ERROR(varUInt64Force(varNewVarLst(varLstNew())), AssertError, "unable to force VariantList to uint64");
        TEST_ERROR(varUInt64Force(VARINT64(-1)), FormatError, "unable to convert int64 -1 to uint64");
        TEST_ERROR(varUInt64Force(VARINT(-1)), FormatError, "unable to convert int -1 to uint64");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(varUInt64(varNewStrZ("string")), AssertError, "assertion 'this->type == varTypeUInt64' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(varUInt64(varDup(VARUINT64(88976))), 88976, "dup uint64");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(varEq(NULL, NULL), true, "null, null eq");
        TEST_RESULT_BOOL(varEq(NULL, VARUINT64(123)), false, "null, uint64 not eq");

        TEST_RESULT_BOOL(varEq(VARUINT64(9223372036854775807L), VARUINT64(9223372036854775807L)), true, "uint64, uint64 eq");
        TEST_RESULT_BOOL(varEq(VARUINT64(444), VARUINT64(123)), false, "uint64, uint64 not eq");
    }

    // *****************************************************************************************************************************
    if (testBegin("keyValue"))
    {
        // Ensure type sizes are as expected
        TEST_RESULT_UINT(sizeof(VariantKeyValue), TEST_64BIT() ? 24 : 12, "check VariantKeyValue size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(varKv(VARINT(66)), AssertError, "assertion 'this->type == varTypeKeyValue' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *keyValue = NULL;

        TEST_ASSIGN(keyValue, varNewKv(NULL), "new null");
        TEST_RESULT_PTR(varKv(keyValue), NULL, "    kv is null");

        TEST_ASSIGN(keyValue, varNewKv(kvNew()), "new");
        TEST_RESULT_PTR(kvPut(varKv(keyValue), VARINT(44), VARINT(55)), varKv(keyValue), "    put int/int");
        TEST_RESULT_INT(varInt(kvGet(varKv(keyValue), VARINT(44))), 55, "    get int/int");
        TEST_RESULT_PTR(varKv(NULL), NULL, "get null kv");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *keyValueDup = NULL;

        TEST_ASSIGN(keyValueDup, varDup(keyValue), "duplicate");
        TEST_RESULT_INT(varInt(kvGet(varKv(keyValueDup), VARINT(44))), 55, "    get int/int");

        varFree(keyValue);
        varFree(keyValueDup);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(varEq(varNewKv(kvNew()), varNewKv(kvNew())), AssertError, "unable to test equality for KeyValue");
    }

    // *****************************************************************************************************************************
    if (testBegin("String"))
    {
        // Ensure type sizes are as expected
        TEST_RESULT_UINT(sizeof(VariantStringConst), TEST_64BIT() ? 16 : 8, "check VariantStringConst size");
        TEST_RESULT_UINT(sizeof(VariantString), TEST_64BIT() ? 24 : 12, "check VariantString size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(varStr(varNewStr(NULL)), NULL, "new null str");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *string = varNewStr(strNew("test-str"));
        TEST_RESULT_STR_Z(varStr(string), "test-str", "string pointer");
        varFree(string);

        TEST_RESULT_STR(varStr(NULL), NULL, "get null string variant");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(varStr(varNewBool(true)), AssertError, "assertion 'this->type == varTypeString' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varIntForce(VARSTR(STRDEF("777"))), 777, "int from string");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(varBoolForce(VARSTRDEF("y")), true, "bool from string");
        TEST_RESULT_INT(varBoolForce(VARSTRZ("on")), true, "bool from string");

        TEST_RESULT_INT(varBoolForce(varNewStrZ("n")), false, "bool from string");
        TEST_RESULT_INT(varBoolForce(VARSTRDEF("off")), false, "bool from string");

        TEST_RESULT_INT(varBoolForce(VARSTRDEF("OFF")), false, "bool from string");

        TEST_ERROR(varBoolForce(VARSTRDEF(BOGUS_STR)), FormatError, "unable to convert str 'BOGUS' to bool");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(varStrForce(VARSTRDEF("teststring")), "teststring", "force string to string");
        TEST_RESULT_STR_Z(varStrForce(VARINT(999)), "999", "force int to string");
        TEST_RESULT_STR_Z(varStrForce(VARINT64(9223372036854775807L)), "9223372036854775807", "force int64 to string");
        TEST_RESULT_STR_Z(varStrForce(varNewBool(true)), "true", "force bool to string");
        TEST_RESULT_STR_Z(varStrForce(varNewBool(false)), "false", "force bool to string");
        TEST_RESULT_STR_Z(varStrForce(VARUINT64(18446744073709551615U)), "18446744073709551615", "force uint64 to string");

        TEST_ERROR(varStrForce(varNewKv(kvNew())), FormatError, "unable to force KeyValue to String");

        // -------------------------------------------------------------------------------------------------------------------------
        string = varNewStrZ("not-an-int");
        TEST_ERROR(varIntForce(string), FormatError, "unable to convert base 10 string 'not-an-int' to int");
        varFree(string);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(varStr(VARSTRDEF("test-z-str")), "test-z-str", "new zero-terminated string");
        TEST_RESULT_STR(varStr(VARSTR(NULL)), NULL, "new null strz");
        TEST_RESULT_STR(varStr(varNewStrZ(NULL)), NULL, "new null strz");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(varStr(varDup(VARSTRDEF("yabba-dabba-doo"))), "yabba-dabba-doo", "dup string");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(varDup(NULL), NULL, "dup NULL");
        TEST_RESULT_BOOL(varEq(VARSTRDEF("expect-equal"), VARSTRDEF("expect-equal")), true, "string, string eq");
        TEST_RESULT_BOOL(varEq(VARSTRDEF("Y"), VARSTRDEF("X")), false, "string, string not eq");
    }

    // *****************************************************************************************************************************
    if (testBegin("VariantList"))
    {
        // Ensure type sizes are as expected
        TEST_RESULT_UINT(sizeof(VariantVariantList), TEST_64BIT() ? 24 : 12, "check VariantVariantList size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(varVarLst(VARINT(66)), AssertError, "assertion 'this->type == varTypeVariantList' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        Variant *listVar = NULL;

        TEST_RESULT_PTR(varVarLst(varNewVarLst(NULL)), NULL, "new null");
        TEST_ASSIGN(listVar, varNewVarLst(varLstNew()), "new empty");

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
        TEST_ERROR(varEq(varNewVarLst(varLstNew()), varNewVarLst(varLstNew())), AssertError, "unable to test equality for VariantList");
    }

    // *****************************************************************************************************************************
    if (testBegin("varToLog"))
    {
        TEST_RESULT_STR_Z(varToLog(varNewStrZ("testme")), "{\"testme\"}", "format String");
        TEST_RESULT_STR_Z(varToLog(varNewBool(false)), "{false}", "format bool");
        TEST_RESULT_STR_Z(varToLog(varNewKv(kvNew())), "{KeyValue}", "format KeyValue");
        TEST_RESULT_STR_Z(varToLog(varNewVarLst(varLstNew())), "{VariantList}", "format VariantList");
        TEST_RESULT_STR_Z(varToLog(NULL), "null", "format null");
    }

    // *****************************************************************************************************************************
    if (testBegin("varLstNew(), varLstAdd(), varLstSize(), varLstGet(), and varListFree()"))
    {
        VariantList *list = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(list, varLstNew(), "new list");
            TEST_RESULT_PTR(varLstMove(NULL, memContextPrior()), NULL, "move null to old context");
            TEST_RESULT_PTR(varLstMove(list, memContextPrior()), list, "move var list to old context");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_PTR(varLstAdd(list, varNewInt(27)), list, "add int");
        TEST_RESULT_PTR(varLstAdd(list, varNewStr(strNew("test-str"))), list, "add string");

        TEST_RESULT_INT(varLstSize(list), 2, "list size");

        TEST_RESULT_INT(varInt(varLstGet(list, 0)), 27, "check int");
        TEST_RESULT_STR_Z(varStr(varLstGet(list, 1)), "test-str", "check string");

        TEST_RESULT_VOID(varLstFree(list), "free list");
        TEST_RESULT_VOID(varLstFree(NULL), "free null list");
    }

    // *****************************************************************************************************************************
    if (testBegin("varLstDup()"))
    {
        VariantList *list = varLstNew();

        varLstAdd(list, varNewStrZ("string1"));
        varLstAdd(list, varNewStrZ("string2"));

        TEST_RESULT_STRLST_Z(strLstNewVarLst(varLstDup(list)), "string1\nstring2\n", "duplicate variant list");

        TEST_RESULT_PTR(varLstDup(NULL), NULL, "duplicate null list");
    }

    // *****************************************************************************************************************************
    if (testBegin("varLstNewStrLst()"))
    {
        StringList *listStr = strLstNew();

        strLstAdd(listStr, strNew("string1"));
        strLstAdd(listStr, strNew("string2"));

        TEST_RESULT_STRLST_Z(strLstNewVarLst(varLstNewStrLst(listStr)), "string1\nstring2\n", "variant list from string list");

        TEST_RESULT_PTR(varLstNewStrLst(NULL), NULL, "variant list from null string list");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
