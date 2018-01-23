/***********************************************************************************************************************************
Test Lists
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test sort comparator
***********************************************************************************************************************************/
static int
testComparator(const void *item1, const void *item2)
{
    int int1 = *(int *)item1;
    int int2 = *(int *)item2;

    if (int1 < int2)
        return -1;

    if (int1 > int2)
        return 1;

    return 0;
}

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
    if (testBegin("lstSort"))
    {
        List *list = lstNew(sizeof(int));
        int value;

        value = 3; lstAdd(list, &value);
        value = 5; lstAdd(list, &value);
        value = 3; lstAdd(list, &value);
        value = 2; lstAdd(list, &value);

        TEST_RESULT_PTR(lstSort(list, testComparator), list, "list sort");

        TEST_RESULT_INT(*((int *)lstGet(list, 0)), 2, "sort value 0");
        TEST_RESULT_INT(*((int *)lstGet(list, 1)), 3, "sort value 1");
        TEST_RESULT_INT(*((int *)lstGet(list, 2)), 3, "sort value 2");
        TEST_RESULT_INT(*((int *)lstGet(list, 3)), 5, "sort value 3");
    }
}
