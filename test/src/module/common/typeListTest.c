/***********************************************************************************************************************************
Test Lists
***********************************************************************************************************************************/
#include "common/time.h"
#include "common/type/collection.h"

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
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("lstNew*(), lstMemContext(), lstToLog(), and lstFree()"))
    {
        List *list = lstNewP(sizeof(void *));

        TEST_RESULT_UINT(list->itemSize, sizeof(void *), "item size");
        TEST_RESULT_UINT(list->pub.listSize, 0, "list size");
        TEST_RESULT_UINT(list->listSizeMax, 0, "list size max");
        TEST_RESULT_PTR(lstMemContext(list), objMemContext(list), "list mem context");
        TEST_RESULT_VOID(lstClear(list), "clear list");

        void *ptr = NULL;
        TEST_RESULT_VOID(lstAdd(list, &ptr), "add item");
        TEST_RESULT_STR_Z(lstToLog(list), "{size: 1}", "check log");

        TEST_RESULT_VOID(lstClear(list), "clear list");
        TEST_RESULT_STR_Z(lstToLog(list), "{size: 0}", "check log after clear");

        TEST_RESULT_VOID(lstFree(list), "free list");
        TEST_RESULT_VOID(lstFree(lstNewP(1)), "free empty list");
        TEST_RESULT_VOID(lstFree(NULL), "free null list");

        TEST_ASSIGN(list, lstNewP(sizeof(String *), .comparator = lstComparatorStr), "new list with params");

        const String *string1 = STRDEF("string1");
        TEST_RESULT_STR_Z(*(String **)lstAdd(list, &string1), "string1", "    add string1");
        const String *string2 = STRDEF("string2");
        TEST_RESULT_VOID(lstAdd(list, &string2), "    add string2");

        const String *string3 = STRDEF("string3");
        TEST_RESULT_PTR(lstFindDefault(list, &string3, (void *)1), (void *)1, "    find string3 returns default");
        TEST_RESULT_BOOL(lstExists(list, &string3), false, "    string3 does not exist");
        TEST_RESULT_STR_Z(*(String **)lstFind(list, &string2), "string2", "    find string2");
        TEST_RESULT_STR_Z(*(String **)lstFindDefault(list, &string2, NULL), "string2", "    find string2 no default");
        TEST_RESULT_BOOL(lstExists(list, &string2), true, "    string2 exists");

        TEST_RESULT_BOOL(lstRemove(list, &string2), true, "    remove string2");
        TEST_RESULT_BOOL(lstRemove(list, &string2), false, "    unable to remove string2");
        TEST_RESULT_PTR(lstFind(list, &string2), NULL, "    unable to find string2");
    }

    // *****************************************************************************************************************************
    if (testBegin("lstAdd(), lstInsert(), lstMove(), and lstSize()"))
    {
        List *list = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            list = lstNewP(sizeof(int));

            TEST_RESULT_BOOL(lstEmpty(list), true, "list empty");

            TEST_ERROR(lstGetLast(list), AssertError, "cannot get last from list with no values");
            TEST_ERROR(lstRemoveLast(list), AssertError, "cannot remove last from list with no values");

            // Add ints to the list
            for (int listIdx = 1; listIdx <= LIST_INITIAL_SIZE; listIdx++)
            {
                TEST_RESULT_VOID(lstAdd(list, &listIdx), zNewFmt("add item %d", listIdx));
            }

            lstMove(list, memContextPrior());
        }
        MEM_CONTEXT_TEMP_END();

        // Insert an int at the beginning
        int insertIdx = 0;
        TEST_RESULT_INT(*(int *)lstInsert(list, 0, &insertIdx), 0, zNewFmt("insert item %d", insertIdx));

        // Check the size
        TEST_RESULT_INT(lstSize(list), 9, "list size");
        TEST_RESULT_BOOL(lstEmpty(list), false, "list not empty");

        // Read them back and check values
        for (unsigned int listIdx = 0; listIdx < lstSize(list); listIdx++)
        {
            int *item = lstGet(list, listIdx);
            TEST_RESULT_INT(*item, listIdx, zNewFmt("check item %u", listIdx));
        }

        // Remove first item
        TEST_RESULT_VOID(lstRemoveIdx(list, 0), "remove first item");

        // Read them back and check values
        for (unsigned int listIdx = 0; listIdx < lstSize(list); listIdx++)
        {
            int *item = listIdx == lstSize(list) - 1 ? lstGetLast(list) : lstGet(list, listIdx);

            TEST_RESULT_INT(*item, listIdx + 1, zNewFmt("check item %u", listIdx));
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("add items to force the list to get moved down");

        TEST_RESULT_INT(lstSize(list), 8, "check size");
        TEST_RESULT_INT(list->listSizeMax, 16, "check size max");

        for (int listIdx = 0; listIdx < 8; listIdx++)
        {
            int item = listIdx + 9;
            TEST_RESULT_VOID(lstAdd(list, &item), zNewFmt("add item %d", item));
        }

        TEST_RESULT_UINT(lstSize(list), list->listSizeMax, "size equals max size");

        // Remove last item
        TEST_RESULT_VOID(lstRemoveLast(list), "remove last item");

        // Read them back and check values
        for (unsigned int listIdx = 0; listIdx < lstSize(list); listIdx++)
        {
            int *item = lstGet(list, listIdx);
            TEST_RESULT_INT(*item, listIdx + 1, zNewFmt("check item %u", listIdx));
        }

        TEST_ERROR(lstGet(list, lstSize(list)), AssertError, "cannot get index 15 from list with 15 value(s)");
        TEST_RESULT_VOID(lstMove(NULL, memContextTop()), "move null list");
    }

    // *****************************************************************************************************************************
    if (testBegin("lstSort"))
    {
        List *list = lstNewP(sizeof(int));
        lstComparatorSet(list, testComparator);
        int value = 0;

        TEST_RESULT_PTR(lstSort(list, sortOrderAsc), list, "list sort asc");
        TEST_RESULT_PTR(lstFind(list, &value), NULL, "unable to find in empty list");

        value = 3; lstAdd(list, &value);
        value = 5; lstAdd(list, &value);
        value = 3; lstAdd(list, &value);
        value = 2; lstAdd(list, &value);

        TEST_RESULT_PTR(lstSort(list, sortOrderNone), list, "list sort none");

        TEST_RESULT_INT(*(int *)lstGet(list, 0), 3, "sort value 0");
        TEST_RESULT_INT(*(int *)lstGet(list, 1), 5, "sort value 1");
        TEST_RESULT_INT(*(int *)lstGet(list, 2), 3, "sort value 2");
        TEST_RESULT_INT(*(int *)lstGet(list, 3), 2, "sort value 3");

        TEST_RESULT_PTR(lstSort(list, sortOrderAsc), list, "list sort asc");

        TEST_RESULT_INT(*(int *)lstGet(list, 0), 2, "sort value 0");
        TEST_RESULT_INT(*(int *)lstGet(list, 1), 3, "sort value 1");
        TEST_RESULT_INT(*(int *)lstGet(list, 2), 3, "sort value 2");
        TEST_RESULT_INT(*(int *)lstGet(list, 3), 5, "sort value 3");

        TEST_RESULT_PTR(lstSort(list, sortOrderDesc), list, "list sort desc");

        TEST_RESULT_INT(*(int *)lstGet(list, 0), 5, "sort value 0");
        TEST_RESULT_INT(*(int *)lstGet(list, 1), 3, "sort value 1");
        TEST_RESULT_INT(*(int *)lstGet(list, 2), 3, "sort value 2");
        TEST_RESULT_INT(*(int *)lstGet(list, 3), 2, "sort value 3");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("lstComparatorZ()");

        const char *string1 = "abc";
        const char *string2 = "def";

        TEST_RESULT_INT(lstComparatorZ(&string1, &string1), 0, "strings are equal");
        TEST_RESULT_BOOL(lstComparatorZ(&string1, &string2) < 0, true, "first string is less");
        TEST_RESULT_BOOL(lstComparatorZ(&string2, &string1) > 0, true, "first string is greater");
    }

    // *****************************************************************************************************************************
    if (testBegin("lstFind()"))
    {
        // Generate a list of values
        int testMax = 100;

        List *list = lstNewP(sizeof(int), .comparator = testComparator);

        for (int listIdx = 0; listIdx < testMax; listIdx++)
            lstAdd(list, &listIdx);

        ASSERT(lstSize(list) == (unsigned int)testMax);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("search ascending sort");

        lstSort(list, sortOrderAsc);

        for (int listIdx = 0; listIdx < testMax; listIdx++)
            ASSERT(*(int *)lstFind(list, &listIdx) == listIdx);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("search descending sort");

        lstSort(list, sortOrderDesc);

        for (int listIdx = 0; listIdx < testMax; listIdx++)
            ASSERT(*(int *)lstFind(list, &listIdx) == listIdx);
    }

    /******************************************************************************************************************************/
    if (testBegin("foreach List Iteration")) {

        // Create an empty list
        const List * emptyList = lstNewP(sizeof(int));

        // Create a long list
        const int testMax = 100;
        List *longList = lstNewP(sizeof(int));
        for (int listIdx = 0; listIdx < testMax; listIdx++)
            lstAdd(longList, &listIdx);
        ASSERT(lstSize(longList) == (unsigned int) testMax);

        // Scan the empty list.
        int *item;
        foreach(item, emptyList)
            ASSERT(false);  // We shouldn't be here with an empty list
        TEST_RESULT_VOID((void)0, "scan empty list");

        // Scan the bigger list.
        int count = 0;
        foreach(item, longList)
        {
            ASSERT(*value == count);
            count++;
        }
        TEST_RESULT_INT(count, testMax, "non-empty List");

        // Scan the empty list, inside a collection
        Collection *emptyCollection = NEWCOLLECTION(List, emptyList);
        FOREACH(int, value, Collection, emptyCollection)
            ASSERT(*value != *value);  // We shouldn't be here with an empty list
        ENDFOREACH;
        TEST_RESULT_VOID((void) 0, "empty list inside Collection");

        // Scan the longer list, inside a collection.
        Collection *longCollection = NEWCOLLECTION(List, longList);
        count = 0;
        FOREACH(int, value, Collection, collection)
            ASSERT(*value == count);
            count++;
        ENDFOREACH;
        TEST_RESULT_INT(count, testMax, "non-empty list inside Collection");

        // Try to get next() item of an empty list.
        ListItr *itr = listItrNew(emptyList);
        ASSERT(listItrNext(itr) == NULL);
        ASSERT(ListItrNext(itr) == NULL);
        TEST_RESULT_PTR(listItrNext(itr), NULL, "iterate beyond end of empty List");
        listItrFree(itr);

        // Similar, but this time with items in the list.
        itr = listItrNew(longList);  // A second iterator in parallel
        foreach(item, longList)
            ASSERT(*listItrNext(itr) = *item);
        ASSERT(item == NULL);
        TEST_RESULT_PTR(listItrNext(itr), NULL, "iterate beyond end of List");
        listItrFree(itr);

        // Try to create a Collection from NULL list.
        TEST_ERROR((void) NEWCOLLECTION(List, NULL), AssertError, "assertion 'subCollection != NULL' failed");

        // Create a Collection within a Collection and verify we can still iterate through it.
        Collection *superCollection = NEWCOLLECTION(Collection, longCollection);
        count = 0;
        FOREACH(int, value, Collection, superCollection)
            ASSERT(*value == count);
            count++;
        ENDFOREACH;
        TEST_RESULT_INT(count, testMax, "Collection inside Collection");

        // Use a mock destructor. Since iteration loop is macro based, this is a quick way to mock.
#define listItrFree mockListItrFree
        extern int eventCount;
        extern void mockListItrFree(ListItr *this);

        // Verify the destructor gets called on a list with no exceptions.
        eventCount = 0;
        FOREACH(int, value, List, list)
            (void) value;
        ENDFOREACH;
        TEST_RESULT_INT(eventCount, 1, "destructor invoked after loop ends");

        // Throw an exception within a loop and verify the destructor gets called.
        eventCount = 0;
        TRY_BEGIN()
            FOREACH(int, value, List, list)
                if (*value > testMax / 2)
                    THROW(FormatError, "");  // Any non-fatal error.
            ENDFOREACH;
        CATCH(FormatError)
            eventCount++; // An extra increment to verify we catch the error.
        TRY_END();
        TEST_RESULT_INT(eventCount, 2, "destructor invoked after exception");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
A Mocked destructor - to verify we are shutting down correctly. Used by the List iterator tests.
***********************************************************************************************************************************/
#undef listItrFree
    int eventCount;  // Keep count of interesting test events.
    void
    mockListItrFree(ListItr *this)
    {
        eventCount++;
        listItrFree(this);
    }
