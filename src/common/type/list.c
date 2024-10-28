/***********************************************************************************************************************************
List Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/type/list.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct List
{
    ListPub pub;                                                    // Publicly accessible variables
    size_t itemSize;
    unsigned int listSizeMax;
    SortOrder sortOrder;
    unsigned char *listAlloc;                                       // Pointer to memory allocated for the list
    unsigned char *list;                                            // Pointer to the current start of the list
    ListComparator *comparator;
};

/**********************************************************************************************************************************/
FN_EXTERN List *
lstNew(const size_t itemSize, const ListParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, itemSize);
        FUNCTION_TEST_PARAM(FUNCTIONP, param.comparator);
    FUNCTION_TEST_END();

    OBJ_NEW_BEGIN(List, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (List)
        {
            .itemSize = itemSize,
            .sortOrder = param.sortOrder,
            .comparator = param.comparator,
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(LIST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN List *
lstClear(List *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->list != NULL)
    {
        MEM_CONTEXT_BEGIN(lstMemContext(this))
        {
            memFree(this->list);
        }
        MEM_CONTEXT_END();

        this->pub.listSize = 0;
        this->listSizeMax = 0;
    }

    FUNCTION_TEST_RETURN(LIST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN int
lstComparatorStr(const void *const item1, const void *const item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, item1);
        FUNCTION_TEST_PARAM_P(VOID, item2);
    FUNCTION_TEST_END();

    ASSERT(item1 != NULL);
    ASSERT(item2 != NULL);

    FUNCTION_TEST_RETURN(INT, strCmp(*(const String *const *)item1, *(const String *const *)item2));
}

/**********************************************************************************************************************************/
FN_EXTERN int
lstComparatorInt(const void *const intPtr1, const void *const intPtr2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, intPtr1);
        FUNCTION_TEST_PARAM_P(VOID, intPtr2);
    FUNCTION_TEST_END();

    ASSERT(intPtr1 != NULL);
    ASSERT(intPtr2 != NULL);

    const int int1 = *(const int *)intPtr1;
    const int int2 = *(const int *)intPtr2;

    FUNCTION_TEST_RETURN(INT, LST_COMPARATOR_CMP(int1, int2));
}

/**********************************************************************************************************************************/
FN_EXTERN int
lstComparatorUInt(const void *const uintPtr1, const void *const uintPtr2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, uintPtr1);
        FUNCTION_TEST_PARAM_P(VOID, uintPtr2);
    FUNCTION_TEST_END();

    ASSERT(uintPtr1 != NULL);
    ASSERT(uintPtr2 != NULL);

    const unsigned int uint1 = *(const unsigned int *)uintPtr1;
    const unsigned int uint2 = *(const unsigned int *)uintPtr2;

    FUNCTION_TEST_RETURN(INT, LST_COMPARATOR_CMP(uint1, uint2));
}

/**********************************************************************************************************************************/
FN_EXTERN int
lstComparatorZ(const void *const item1, const void *const item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, item1);
        FUNCTION_TEST_PARAM_P(VOID, item2);
    FUNCTION_TEST_END();

    ASSERT(item1 != NULL);
    ASSERT(item2 != NULL);

    FUNCTION_TEST_RETURN(INT, strcmp(*(const char *const *)item1, *(const char *const *)item2));
}

/***********************************************************************************************************************************
General function for a descending comparator that simply switches the parameters on the main comparator (which should be asc)
***********************************************************************************************************************************/
static const List *comparatorDescList = NULL;

static int
lstComparatorDesc(const void *const item1, const void *const item2)
{
    return comparatorDescList->comparator(item2, item1);
}

/**********************************************************************************************************************************/
FN_EXTERN void *
lstGet(const List *const this, const unsigned int listIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Ensure list index is in range
    if (listIdx >= lstSize(this))
        THROW_FMT(AssertError, "cannot get index %u from list with %u value(s)", listIdx, lstSize(this));

    // Return pointer to list item
    FUNCTION_TEST_RETURN_P(VOID, this->list + (listIdx * this->itemSize));
}

FN_EXTERN void *
lstGetLast(const List *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Ensure there are items in the list
    if (lstSize(this) == 0)
        THROW(AssertError, "cannot get last from list with no values");

    // Return pointer to list item
    FUNCTION_TEST_RETURN_P(VOID, lstGet(this, lstSize(this) - 1));
}

/**********************************************************************************************************************************/
FN_EXTERN void *
lstFind(const List *const this, const void *const item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->comparator != NULL);
    ASSERT(item != NULL);

    if (this->list != NULL)
    {
        if (this->sortOrder == sortOrderAsc)
            FUNCTION_TEST_RETURN_P(VOID, bsearch(item, this->list, lstSize(this), this->itemSize, this->comparator));
        else if (this->sortOrder == sortOrderDesc)
        {
            // Assign the list for the descending comparator to use
            comparatorDescList = this;

            FUNCTION_TEST_RETURN_P(VOID, bsearch(item, this->list, lstSize(this), this->itemSize, lstComparatorDesc));
        }

        // Fall back on an iterative search
        for (unsigned int listIdx = 0; listIdx < lstSize(this); listIdx++)
        {
            if (this->comparator(item, lstGet(this, listIdx)) == 0)
                FUNCTION_TEST_RETURN_P(VOID, lstGet(this, listIdx));
        }
    }

    FUNCTION_TEST_RETURN_P(VOID, NULL);
}

FN_EXTERN unsigned int
lstFindIdx(const List *const this, const void *const item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(item != NULL);

    void *const result = lstFind(this, item);

    FUNCTION_TEST_RETURN(UINT, result == NULL ? LIST_NOT_FOUND : lstIdx(this, result));
}

FN_EXTERN const void *
lstFindDefault(const List *const this, const void *const item, const void *const itemDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
        FUNCTION_TEST_PARAM_P(VOID, itemDefault);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(item != NULL);

    void *const result = lstFind(this, item);

    FUNCTION_TEST_RETURN_CONST_P(VOID, result == NULL ? itemDefault : result);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
lstIdx(const List *const this, const void *const item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(item != NULL);

    // Item pointers should always be aligned with the beginning of an item in the list
    ASSERT((size_t)((const unsigned char *const)item - this->list) % this->itemSize == 0);

    const size_t result = (size_t)((const unsigned char *const)item - this->list) / this->itemSize;

    // Item pointers should always be in range
    ASSERT(result < lstSize(this));

    FUNCTION_TEST_RETURN(UINT, (unsigned int)result);
}

/**********************************************************************************************************************************/
FN_EXTERN void *
lstInsert(List *const this, const unsigned int listIdx, const void *const item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(listIdx <= lstSize(this));
    ASSERT(item != NULL);

    // If list size = max then allocate more space
    if (lstSize(this) == this->listSizeMax)
    {
        MEM_CONTEXT_BEGIN(lstMemContext(this))
        {
            // If nothing has been allocated yet
            if (this->listSizeMax == 0)
            {
                this->listSizeMax = LIST_INITIAL_SIZE;
                this->list = memNew(this->listSizeMax * this->itemSize);
            }
            // Else the list needs to be extended
            else
            {
                this->listSizeMax *= 2;
                this->list = memResize(this->list, this->listSizeMax * this->itemSize);
            }

            this->listAlloc = this->list;
        }
        MEM_CONTEXT_END();
    }
    // Else if there is space before the beginning of the list then move the list down
    else if (
        (this->list != this->listAlloc) &&
        (lstSize(this) + ((uintptr_t)(this->list - this->listAlloc) / this->itemSize) == this->listSizeMax))
    {
        memmove(this->listAlloc, this->list, lstSize(this) * this->itemSize);
        this->list = this->listAlloc;
    }

    // Calculate the position where this item will be copied
    void *const itemPtr = this->list + (listIdx * this->itemSize);

    // If not inserting at the end then move items down to make space
    if (listIdx != lstSize(this))
        memmove(this->list + ((listIdx + 1) * this->itemSize), itemPtr, (lstSize(this) - listIdx) * this->itemSize);

    // Copy item into the list
    this->sortOrder = sortOrderNone;
    memcpy(itemPtr, item, this->itemSize);
    this->pub.listSize++;

    FUNCTION_TEST_RETURN_P(VOID, itemPtr);
}

/**********************************************************************************************************************************/
FN_EXTERN List *
lstRemoveIdx(List *const this, const unsigned int listIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(listIdx <= lstSize(this));

    // Decrement the list size
    this->pub.listSize--;

    // If this is the first item then move the list pointer up to avoid moving all the items
    if (listIdx == 0)
    {
        this->list += this->itemSize;
    }
    // Else remove the item by moving the items after it down
    else
    {
        memmove(
            this->list + (listIdx * this->itemSize), this->list + ((listIdx + 1) * this->itemSize),
            (lstSize(this) - listIdx) * this->itemSize);
    }

    FUNCTION_TEST_RETURN(LIST, this);
}

FN_EXTERN bool
lstRemove(List *const this, const void *const item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(item != NULL);

    const unsigned int listIdx = lstFindIdx(this, item);

    if (listIdx != LIST_NOT_FOUND)
    {
        lstRemoveIdx(this, listIdx);
        FUNCTION_TEST_RETURN(BOOL, true);
    }

    FUNCTION_TEST_RETURN(BOOL, false);
}

FN_EXTERN List *
lstRemoveLast(List *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (lstSize(this) == 0)
        THROW(AssertError, "cannot remove last from list with no values");

    FUNCTION_TEST_RETURN(LIST, lstRemoveIdx(this, lstSize(this) - 1));
}

/**********************************************************************************************************************************/
FN_EXTERN List *
lstSort(List *const this, const SortOrder sortOrder)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(ENUM, sortOrder);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->comparator != NULL);

    if (this->list != NULL)
    {
        switch (sortOrder)
        {
            case sortOrderAsc:
                qsort(this->list, lstSize(this), this->itemSize, this->comparator);
                break;

            case sortOrderDesc:
            {
                // Assign the list that will be sorted for the comparator function to use
                comparatorDescList = this;

                qsort(this->list, lstSize(this), this->itemSize, lstComparatorDesc);
                break;
            }

            case sortOrderNone:
                break;
        }
    }

    this->sortOrder = sortOrder;

    FUNCTION_TEST_RETURN(LIST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN List *
lstComparatorSet(List *const this, ListComparator *const comparator)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(FUNCTIONP, comparator);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    this->comparator = comparator;
    this->sortOrder = sortOrderNone;

    FUNCTION_TEST_RETURN(LIST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
lstToLog(const List *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{size: %u}", lstSize(this));
}
