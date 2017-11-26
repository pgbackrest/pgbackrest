/***********************************************************************************************************************************
Test Lists
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("lstNew() and lstFree()"))
    {
        List *list = lstNew(sizeof(void *));

        TEST_RESULT_INT(list->itemSize, sizeof(void *), "item size");
        TEST_RESULT_INT(list->listSize, 0, "list size");
        TEST_RESULT_INT(list->listSizeMax, 0, "list size max");

        void *ptr = NULL;
        TEST_RESULT_PTR(lstAdd(list, &ptr), list, "add item");

        lstFree(list);
    }

    // *****************************************************************************************************************************
    if (testBegin("lstAdd() and lstSize()"))
    {
        List *list = lstNew(sizeof(int));

        // Add ints to the list
        for (int listIdx = 0; listIdx <= LIST_INITIAL_SIZE; listIdx++)
            TEST_RESULT_PTR(lstAdd(list, &listIdx), list, "add item %d", listIdx);

        TEST_RESULT_INT(lstSize(list), 9, "list size");

        // Read them back and check values
        for (unsigned int listIdx = 0; listIdx < lstSize(list); listIdx++)
        {
            int *item = lstGet(list, listIdx);
            TEST_RESULT_INT(*item, listIdx, "check item %d", listIdx);
        }

        TEST_ERROR(lstGet(list, lstSize(list)), AssertError, "cannot get index 9 from list with 9 value(s)");
    }

    // *****************************************************************************************************************************
    if (testBegin("StringList"))
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

        // strLstCat()
        // -------------------------------------------------------------------------------------------------------------------------
        list = strLstNew();

        TEST_RESULT_STR(strPtr(strLstCat(list, ", ")), "", "empty list");

        strLstAdd(list, strNew("item1"));
        strLstAdd(list, strNew("item2"));

        TEST_RESULT_STR(strPtr(strLstCat(list, ", ")), "item1, item2", "empty list");

        strLstAdd(list, NULL);

        TEST_RESULT_STR(strPtr(strLstCat(list, ", ")), "item1, item2, [NULL]", "empty list");

        strLstFree(list);
    }
}
