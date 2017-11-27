/***********************************************************************************************************************************
List Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/memContext.h"
#include "common/type/list.h"

/***********************************************************************************************************************************
Contains information about the list
***********************************************************************************************************************************/
struct List
{
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
    // Create object
    List *this = memNew(sizeof(List));
    this->itemSize = itemSize;

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
            this->list = memGrowRaw(this->list, this->listSize * this->itemSize);
        }
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
Return list size
***********************************************************************************************************************************/
unsigned int
lstSize(const List *this)
{
    return this->listSize;
}

/***********************************************************************************************************************************
Free the string
***********************************************************************************************************************************/
void
lstFree(List *this)
{
    if (this->list != NULL)
        memFree(this->list);

    memFree(this);
}
