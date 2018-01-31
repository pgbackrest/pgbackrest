/***********************************************************************************************************************************
Test Variant Lists
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("varLstNew(), varLstAdd(), varLstSize(), varLstGet(), and varListFree()"))
    {
        VariantList *list = NULL;

        TEST_ASSIGN(list, varLstNew(), "new list");

        TEST_RESULT_PTR(varLstAdd(list, varNewInt(27)), list, "add int");
        TEST_RESULT_PTR(varLstAdd(list, varNewStr(strNew("test-str"))), list, "add string");

        TEST_RESULT_INT(varLstSize(list), 2, "list size");

        TEST_RESULT_INT(varInt(varLstGet(list, 0)), 27, "check int");
        TEST_RESULT_STR(strPtr(varStr(varLstGet(list, 1))), "test-str", "check string");

        TEST_RESULT_VOID(varLstFree(list), "free list");
    }

    // *****************************************************************************************************************************
    if (testBegin("varLstDup()"))
    {
        VariantList *list = varLstNew();

        varLstAdd(list, varNewStr(strNew("string1")));
        varLstAdd(list, varNewStr(strNew("string2")));

        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstNewVarLst(varLstDup(list)), ", ")),
            "string1, string2", "duplicate variant list");

        TEST_RESULT_PTR(varLstDup(NULL), NULL, "duplicate null list");
    }

    // *****************************************************************************************************************************
    if (testBegin("varLstNewStrLst()"))
    {
        StringList *listStr = strLstNew();

        strLstAdd(listStr, strNew("string1"));
        strLstAdd(listStr, strNew("string2"));

        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstNewVarLst(varLstNewStrLst(listStr)), ", ")),
            "string1, string2", "variant list from string list");

        TEST_RESULT_PTR(varLstNewStrLst(NULL), NULL, "variant list from null string list");
    }
}
