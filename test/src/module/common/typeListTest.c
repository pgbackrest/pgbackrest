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
}
