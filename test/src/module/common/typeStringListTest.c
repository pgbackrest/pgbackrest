/***********************************************************************************************************************************
Test String Lists
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("strLstNew(), strLstAdd, strLstGet(), strLstSize(), and strLstFree()"))
    {
        StringList *list = strLstNew();

        // Add strings to the list
        // -------------------------------------------------------------------------------------------------------------------------
        for (int listIdx = 0; listIdx <= LIST_INITIAL_SIZE; listIdx++)
        {
            if (listIdx == 0)
            {
                TEST_RESULT_PTR(strLstAdd(list, NULL), list, "add null item");
            }
            else
                TEST_RESULT_PTR(strLstAdd(list, strNewFmt("STR%02d", listIdx)), list, "add item %d", listIdx);
        }

        TEST_RESULT_INT(strLstSize(list), 9, "list size");

        // Read them back and check values
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int listIdx = 0; listIdx < strLstSize(list); listIdx++)
        {
            if (listIdx == 0)
            {
                TEST_RESULT_PTR(strLstGet(list, listIdx), NULL, "check null item");
            }
            else
                TEST_RESULT_STR(strPtr(strLstGet(list, listIdx)), strPtr(strNewFmt("STR%02d", listIdx)), "check item %d", listIdx);
        }

        strLstFree(list);
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstPtr()"))
    {
        StringList *list = strLstNew();

        // Add strings to the list
        // -------------------------------------------------------------------------------------------------------------------------
        for (int listIdx = 0; listIdx <= 3; listIdx++)
        {
            if (listIdx == 0)
                strLstAdd(list, NULL);
            else
                strLstAdd(list, strNewFmt("STR%02d", listIdx));
        }

        // Check pointer
        // -------------------------------------------------------------------------------------------------------------------------
        const char **szList = strLstPtr(list);

        for (unsigned int listIdx = 0; listIdx < strLstSize(list); listIdx++)
        {
            if (listIdx == 0)
            {
                TEST_RESULT_PTR(szList[listIdx], NULL, "check null item");
            }
            else
                TEST_RESULT_STR(szList[listIdx], strPtr(strNewFmt("STR%02d", listIdx)), "check item %d", listIdx);
        }

        strLstFree(list);
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstNewVarLst()"))
    {
        VariantList *varList = varLstNew();

        varLstAdd(varList, varNewStr(strNew("string1")));
        varLstAdd(varList, varNewStr(strNew("string2")));

        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewVarLst(varList), ", ")), "string1, string2", "string list from variant list");

        varLstFree(varList);
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstJoin()"))
    {
        StringList *list = strLstNew();

        TEST_RESULT_STR(strPtr(strLstJoin(list, ", ")), "", "empty list");

        strLstAdd(list, strNew("item1"));
        strLstAddZ(list, "item2");

        TEST_RESULT_STR(strPtr(strLstJoin(list, ", ")), "item1, item2", "list");

        strLstAdd(list, NULL);

        TEST_RESULT_STR(strPtr(strLstJoin(list, ", ")), "item1, item2, [NULL]", "list with NULL at end");

        TEST_RESULT_STR(strPtr(strLstJoin(strLstDup(list), ", ")), "item1, item2, [NULL]", "dup'd list will NULL at end");

        strLstFree(list);
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstSplit()"))
    {
        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewSplit(strNew(""), strNew(", ")), ", ")), "", "empty list");
        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewSplit(strNew("item1"), strNew(", ")), ", ")), "item1", "one item");
        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewSplit(strNew("item1, item2"), strNew(", ")), ", ")), "item1, item2", "two items");
    }
}
