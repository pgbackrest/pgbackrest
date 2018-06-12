/***********************************************************************************************************************************
List Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/type/list.h"

/***********************************************************************************************************************************
Contains information about the list
***********************************************************************************************************************************/
struct List
{
    MemContext *memContext;
    size_t itemSize;
    unsigned int listSize;
    unsigned int listSizeMax;
    unsigned char *list;
};

/***********************************************************************************************************************************
Create a new list
***********************************************************************************************************************************/
List *
lstNew(size_t itemSize)
{
    FUNCTION_TEST_VOID();

    List *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("List")
    {
        // Create object
        this = memNew(sizeof(List));
        this->memContext = MEM_CONTEXT_NEW();
        this->itemSize = itemSize;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RESULT(LIST, this);
}

/***********************************************************************************************************************************
Add an item to the end of the list
***********************************************************************************************************************************/
List *
lstAdd(List *this, const void *item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(VOIDP, item);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(item != NULL);
    FUNCTION_TEST_END();

    // If list size = max then allocate more space
    if (this->listSize == this->listSizeMax)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            // If nothing has been allocated yet
            if (this->listSizeMax == 0)
            {
                this->listSizeMax = LIST_INITIAL_SIZE;
                this->list = memNewRaw(this->listSizeMax * this->itemSize);
            }
            // Else the list needs to be extended
            else
            {
                this->listSizeMax *= 2;
                this->list = memGrowRaw(this->list, this->listSizeMax * this->itemSize);
            }
        }
        MEM_CONTEXT_END();
    }

    memcpy(this->list + (this->listSize * this->itemSize), item, this->itemSize);
    this->listSize++;

    FUNCTION_TEST_RESULT(LIST, this);
}

/***********************************************************************************************************************************
Get an item from the list
***********************************************************************************************************************************/
void *
lstGet(const List *this, unsigned int listIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    // Ensure list index is in range
    if (listIdx >= this->listSize)
        THROW_FMT(AssertError, "cannot get index %u from list with %u value(s)", listIdx, this->listSize);

    // Return pointer to list item
    FUNCTION_TEST_RESULT(VOIDP, this->list + (listIdx * this->itemSize));
}

/***********************************************************************************************************************************
Return the memory context for this list
***********************************************************************************************************************************/
MemContext *
lstMemContext(const List *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(MEM_CONTEXT, this->memContext);
}

/***********************************************************************************************************************************
Move the string list
***********************************************************************************************************************************/
List *
lstMove(List *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);

        FUNCTION_TEST_ASSERT(parentNew != NULL);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RESULT(LIST, this);
}

/***********************************************************************************************************************************
Return list size
***********************************************************************************************************************************/
unsigned int
lstSize(const List *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(UINT, this->listSize);
}

/***********************************************************************************************************************************
List sort
***********************************************************************************************************************************/
List *
lstSort(List *this, int (*comparator)(const void *, const void*))
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
        FUNCTION_TEST_PARAM(FUNCTIONP, comparator);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(comparator != NULL);
    FUNCTION_TEST_END();

    qsort(this->list, this->listSize, this->itemSize, comparator);

    FUNCTION_TEST_RESULT(LIST, this);
}

/***********************************************************************************************************************************
Convert to a zero-terminated string for logging
***********************************************************************************************************************************/
size_t
lstToLog(const List *this, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *string = NULL;

        if (this == NULL)
            string = strNew("null");
        else
            string = strNewFmt("{size: %u}", this->listSize);

        result = (size_t)snprintf(buffer, bufferSize, "%s", strPtr(string));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Free the string
***********************************************************************************************************************************/
void
lstFree(List *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, this);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_TEST_RESULT_VOID();
}
