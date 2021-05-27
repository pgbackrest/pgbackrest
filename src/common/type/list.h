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
    MemContext *memContext;                                         // Mem context
    unsigned int listSize;                                          // List size
} ListPub;

// Set a new comparator
List *lstComparatorSet(List *this, ListComparator *comparator);

// Memory context for this list
__attribute__((always_inline)) static inline MemContext *
lstMemContext(const List *const this)
{
    return THIS_PUB(List)->memContext;
}

// List size
__attribute__((always_inline)) static inline unsigned int
lstSize(const List *const this)
{
    return THIS_PUB(List)->listSize;
}

// Is the list empty?
__attribute__((always_inline)) static inline bool
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
__attribute__((always_inline)) static inline bool
lstExists(const List *const this, const void *const item)
{
    return lstFind(this, item) != NULL;
}

// Get the index of a list item
unsigned int lstIdx(const List *this, const void *item);

// Insert an item into the list
void *lstInsert(List *this, unsigned int listIdx, const void *item);

// Add an item to the end of the list
__attribute__((always_inline)) static inline void *
lstAdd(List *const this, const void *const item)
{
    return lstInsert(this, lstSize(this), item);
}

// Move to a new parent mem context
__attribute__((always_inline)) static inline List *
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
__attribute__((always_inline)) static inline void
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

#endif
