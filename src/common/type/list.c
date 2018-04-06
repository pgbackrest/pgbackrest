/***********************************************************************************************************************************
List Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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
    List *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("List")
    {
        // Create object
        this = memNew(sizeof(List));
        this->memContext = MEM_CONTEXT_NEW();
        this->itemSize = itemSize;
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Add an item to the end of the list
***********************************************************************************************************************************/
List *
lstAdd(List *this, const void *item)
{
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

    // Return list
    return this;
}

/***********************************************************************************************************************************
Get an item from the list
***********************************************************************************************************************************/
void *
lstGet(const List *this, unsigned int listIdx)
{
    // Ensure list index is in range
    if (listIdx >= this->listSize)
        THROW(AssertError, "cannot get index %d from list with %d value(s)", listIdx, this->listSize);

    // Return pointer to list item
    return this->list + (listIdx * this->itemSize);
}

/***********************************************************************************************************************************
Return the memory context for this list
***********************************************************************************************************************************/
MemContext *
lstMemContext(const List *this)
{
    return this->memContext;
}

/***********************************************************************************************************************************
Move the string list
***********************************************************************************************************************************/
List *
lstMove(List *this, MemContext *parentNew)
{
    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    return this;
}

/***********************************************************************************************************************************
Return list size
***********************************************************************************************************************************/
unsigned int
lstSize(const List *this)
{
    return this->listSize;
}

/***********************************************************************************************************************************
List sort
***********************************************************************************************************************************/
List *
lstSort(List *this, int (*comparator)(const void *, const void*))
{
    qsort(this->list, this->listSize, this->itemSize, comparator);

    return this;
}

/***********************************************************************************************************************************
Free the string
***********************************************************************************************************************************/
void
lstFree(List *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
