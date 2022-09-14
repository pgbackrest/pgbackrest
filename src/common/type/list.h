/***********************************************************************************************************************************
List Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_LIST_H
#define COMMON_TYPE_LIST_H

#include <limits.h>

/***********************************************************************************************************************************
List object
***********************************************************************************************************************************/
typedef struct List List;

#include "common/memContext.h"
#include "common/type/object.h"
#include "common/type/param.h"
#include "common/type/string.h"
#include "common/type/collection.h"

/***********************************************************************************************************************************
Sort orders
***********************************************************************************************************************************/
typedef enum
{
    sortOrderNone,
    sortOrderAsc,
    sortOrderDesc,
} SortOrder;

/***********************************************************************************************************************************
Define initial size of a list
***********************************************************************************************************************************/
#define LIST_INITIAL_SIZE                                           8

/***********************************************************************************************************************************
Item was not found in the list
***********************************************************************************************************************************/
#define LIST_NOT_FOUND                                              UINT_MAX

/***********************************************************************************************************************************
Function type for comparing items in the list

The return value should be -1 when item1 < item2, 0 when item1 == item2, and 1 when item2 > item1.
***********************************************************************************************************************************/
typedef int ListComparator(const void *item1, const void *item2);

// General purpose list comparator for Strings or structs with a String as the first member
int lstComparatorStr(const void *item1, const void *item2);

// General purpose list comparator for zero-terminated strings or structs with a zero-terminated string as the first member
int lstComparatorZ(const void *item1, const void *item2);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct ListParam
{
    VAR_PARAM_HEADER;
    SortOrder sortOrder;
    ListComparator *comparator;
} ListParam;

#define lstNewP(itemSize, ...)                                                                                                     \
    lstNew(itemSize, (ListParam){VAR_PARAM_INIT, __VA_ARGS__})

List *lstNew(size_t itemSize, ListParam param);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct ListPub
{
    unsigned int listSize;                                          // List size
} ListPub;

// Set a new comparator
List *lstComparatorSet(List *this, ListComparator *comparator);

// Memory context for this list
FN_INLINE_ALWAYS MemContext *
lstMemContext(List *const this)
{
    return objMemContext(this);
}

// List size
FN_INLINE_ALWAYS unsigned int
lstSize(const List *const this)
{
    return THIS_PUB(List)->listSize;
}

// Is the list empty?
FN_INLINE_ALWAYS bool
lstEmpty(const List *const this)
{
    return lstSize(this) == 0;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Clear items from a list
List *lstClear(List *this);

// Get an item from the list
void *lstGet(const List *this, unsigned int listIdx);
void *lstGetLast(const List *this);

// Find an item in the list
void *lstFind(const List *this, const void *item);
void *lstFindDefault(const List *this, const void *item, void *itemDefault);
unsigned int lstFindIdx(const List *this, const void *item);

// Does an item exist in the list?
FN_INLINE_ALWAYS bool
lstExists(const List *const this, const void *const item)
{
    return lstFind(this, item) != NULL;
}

// Get the index of a list item
unsigned int lstIdx(const List *this, const void *item);

// Insert an item into the list
void *lstInsert(List *this, unsigned int listIdx, const void *item);

// Add an item to the end of the list
FN_INLINE_ALWAYS void *
lstAdd(List *const this, const void *const item)
{
    return lstInsert(this, lstSize(this), item);
}

// Move to a new parent mem context
FN_INLINE_ALWAYS List *
lstMove(List *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Remove an item from the list
bool lstRemove(List *this, const void *item);
List *lstRemoveIdx(List *this, unsigned int listIdx);
List *lstRemoveLast(List *this);

// List sort
List *lstSort(List *this, SortOrder sortOrder);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
lstFree(List *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *lstToLog(const List *this);

#define FUNCTION_LOG_LIST_TYPE                                                                                                     \
    List *
#define FUNCTION_LOG_LIST_FORMAT(value, buffer, bufferSize)                                                                        \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, lstToLog, buffer, bufferSize)

/***********************************************************************************************************************************
List Iteration.
***********************************************************************************************************************************/
typedef struct ListItr ListItr;                                     // Just a placeholder since all methods are inlined.
typedef struct ListItrPub
{
    List *list;                                                     // The list we're iterating
    unsigned int listIdx;                                           // Position of next item in the list
} ListItrPub;

// Construct a list iterator to scan the given list.
ListItr *listItrNew(List *list);

/***********************************************************************************************************************************
Get a pointer to the next item from the list, or NULL if no more.
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void *
listItrNext(ListItr *this)
{
    if (THIS_PUB(ListItr)->listIdx >= lstSize(THIS_PUB(ListItr)->list))
        return NULL;
    else
        return lstGet(THIS_PUB(ListItr)->list, THIS_PUB(ListItr)->listIdx++);
}

/***********************************************************************************************************************************
Free the list object.
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
listItrFree(ListItr *this)
{
    objFree(this);
}

// The following macros enable Lists as abstract Collections.
#define newListItr(list) listItrNew(list)
#define nextListItr(list) listItrNext(list)
#define freeListItr(list) listItrFree(list)

#endif
