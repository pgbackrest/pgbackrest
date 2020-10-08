/***********************************************************************************************************************************
List Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/type/list.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct List
{
    MemContext *memContext;
    size_t itemSize;
    unsigned int listSize;
    unsigned int listSizeMax;
    SortOrder sortOrder;
    unsigned char *list;
    ListComparator *comparator;
};

OBJECT_DEFINE_MOVE(LIST);
OBJECT_DEFINE_FREE(LIST);

/**********************************************************************************************************************************/
List *
lstNew(size_t itemSize, ListParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, itemSize);
        FUNCTION_TEST_PARAM(FUNCTIONP, param.comparator);
    FUNCTION_TEST_END();

    List *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("List")
    {
        // Create object
        this = memNew(sizeof(List));

        *this = (List)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .itemSize = itemSize,
            .sortOrder = param.sortOrder,
            .comparator = param.comparator,
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
void *
lstAdd(List *this, const void *item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(item != NULL);

    FUNCTION_TEST_RETURN(lstInsert(this, lstSize(this), item));
}

/**********************************************************************************************************************************/
List *
lstClear(List *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->list != NULL)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            memFree(this->list);
        }
        MEM_CONTEXT_END();

        this->listSize = 0;
        this->listSizeMax = 0;
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
int
lstComparatorStr(const void *item1, const void *item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, item1);
        FUNCTION_TEST_PARAM_P(VOID, item2);
    FUNCTION_TEST_END();

    ASSERT(item1 != NULL);
    ASSERT(item2 != NULL);

    FUNCTION_TEST_RETURN(strCmp(*(String **)item1, *(String **)item2));
}

/***********************************************************************************************************************************
General function for a descending comparator that simply switches the parameters on the main comparator (which should be asc)
***********************************************************************************************************************************/
static const List *comparatorDescList = NULL;

static int
lstComparatorDesc(const void *item1, const void *item2)
{
    return comparatorDescList->comparator(item2, item1);
}

/**********************************************************************************************************************************/
void *
lstGet(const List *this, unsigned int listIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Ensure list index is in range
    if (listIdx >= this->listSize)
        THROW_FMT(AssertError, "cannot get index %u from list with %u value(s)", listIdx, this->listSize);

    // Return pointer to list item
    FUNCTION_TEST_RETURN(this->list + (listIdx * this->itemSize));
}

void *
lstGetLast(const List *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Ensure there are items in the list
    if (this->listSize == 0)
        THROW(AssertError, "cannot get last from list with no values");

    // Return pointer to list item
    FUNCTION_TEST_RETURN(lstGet(this, this->listSize - 1));
}

/**********************************************************************************************************************************/
bool
lstExists(const List *this, const void *item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(item != NULL);

    FUNCTION_TEST_RETURN(lstFind(this, item) != NULL);
}

/**********************************************************************************************************************************/
void *
lstFind(const List *this, const void *item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->comparator != NULL);
    ASSERT(item != NULL);

    if (this->sortOrder == sortOrderAsc)
        FUNCTION_TEST_RETURN(bsearch(item, this->list, this->listSize, this->itemSize, this->comparator));
    else if (this->sortOrder == sortOrderDesc)
    {
        // Assign the list for the descending comparator to use
        comparatorDescList = this;

        FUNCTION_TEST_RETURN(bsearch(item, this->list, this->listSize, this->itemSize, lstComparatorDesc));
    }

    // Fall back on an iterative search
    for (unsigned int listIdx = 0; listIdx < lstSize(this); listIdx++)
    {
        if (this->comparator(item, lstGet(this, listIdx)) == 0)
            FUNCTION_TEST_RETURN(lstGet(this, listIdx));
    }

    FUNCTION_TEST_RETURN(NULL);
}

unsigned int
lstFindIdx(const List *this, const void *item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(item != NULL);

    void *result = lstFind(this, item);

    FUNCTION_TEST_RETURN(result == NULL ? LIST_NOT_FOUND : lstIdx(this, result));
}

void *
lstFindDefault(const List *this, const void *item, void *itemDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
        FUNCTION_TEST_PARAM_P(VOID, itemDefault);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(item != NULL);

    void *result= lstFind(this, item);

    FUNCTION_TEST_RETURN(result == NULL ? itemDefault : result);
}

/**********************************************************************************************************************************/
unsigned int
lstIdx(const List *this, const void *item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(item != NULL);

    // Item pointers should always be aligned with the beginning of an item in the list
    ASSERT((size_t)((unsigned char * const)item - this->list) % this->itemSize == 0);

    size_t result = (size_t)((unsigned char * const)item - this->list) / this->itemSize;

    // Item pointers should always be in range
    ASSERT(result < this->listSize);

    FUNCTION_TEST_RETURN((unsigned int)result);
}

/**********************************************************************************************************************************/
void *
lstInsert(List *this, unsigned int listIdx, const void *item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(listIdx <= lstSize(this));
    ASSERT(item != NULL);

    // If list size = max then allocate more space
    if (this->listSize == this->listSizeMax)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
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
        }
        MEM_CONTEXT_END();
    }

    // If not inserting at the end then move items down to make space
    void *itemPtr = this->list + (listIdx * this->itemSize);

    if (listIdx != lstSize(this))
        memmove(this->list + ((listIdx + 1) * this->itemSize), itemPtr, (lstSize(this) - listIdx) * this->itemSize);

    // Copy item into the list
    this->sortOrder = sortOrderNone;
    memcpy(itemPtr, item, this->itemSize);
    this->listSize++;

    FUNCTION_TEST_RETURN(itemPtr);
}

/**********************************************************************************************************************************/
List *
lstRemoveIdx(List *this, unsigned int listIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(listIdx <= lstSize(this));

    // Remove the item by moving the items after it down
    this->listSize--;

    memmove(
        this->list + (listIdx * this->itemSize), this->list + ((listIdx + 1) * this->itemSize),
        (lstSize(this) - listIdx) * this->itemSize);

    FUNCTION_TEST_RETURN(this);
}

bool
lstRemove(List *this, const void *item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM_P(VOID, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(item != NULL);

    unsigned int listIdx = lstFindIdx(this, item);

    if (listIdx != LIST_NOT_FOUND)
    {
        lstRemoveIdx(this, listIdx);
        FUNCTION_TEST_RETURN(true);
    }

    FUNCTION_TEST_RETURN(false);
}

List *
lstRemoveLast(List *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->listSize == 0)
        THROW(AssertError, "cannot remove last from list with no values");

    FUNCTION_TEST_RETURN(lstRemoveIdx(this, this->listSize - 1));
}

/**********************************************************************************************************************************/
MemContext *
lstMemContext(const List *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->memContext);
}

/**********************************************************************************************************************************/
unsigned int
lstSize(const List *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->listSize);
}

/**********************************************************************************************************************************/
List *
lstSort(List *this, SortOrder sortOrder)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(ENUM, sortOrder);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->comparator != NULL);

    switch (sortOrder)
    {
        case sortOrderAsc:
        {
            qsort(this->list, this->listSize, this->itemSize, this->comparator);
            break;
        }

        case sortOrderDesc:
        {
            // Assign the list that will be sorted for the comparator function to use
            comparatorDescList = this;

            qsort(this->list, this->listSize, this->itemSize, lstComparatorDesc);
            break;
        }

        case sortOrderNone:
            break;
    }

    this->sortOrder = sortOrder;

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
List *
lstComparatorSet(List *this, ListComparator *comparator)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(FUNCTIONP, comparator);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    this->comparator = comparator;
    this->sortOrder = sortOrderNone;

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
lstToLog(const List *this)
{
    return strNewFmt("{size: %u}", this->listSize);
}
