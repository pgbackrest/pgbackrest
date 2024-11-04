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
FN_EXTERN int lstComparatorStr(const void *item1, const void *item2);

// General purpose list comparator for unsigned int or structs with an unsigned int as the first member
FN_EXTERN int lstComparatorUInt(const void *item1, const void *item2);

// General purpose list comparator for int or structs with an int as the first member
FN_EXTERN int lstComparatorInt(const void *item1, const void *item2);

// General purpose list comparator for zero-terminated strings or structs with a zero-terminated string as the first member
FN_EXTERN int lstComparatorZ(const void *item1, const void *item2);

// Macro to compare two values in a branchless and transitive fashion
#define LST_COMPARATOR_CMP(item1, item2)                                                                                           \
    (((item1) > (item2)) - ((item1) < (item2)))

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

FN_EXTERN List *lstNew(size_t itemSize, ListParam param);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct ListPub
{
    unsigned int listSize;                                          // List size
} ListPub;

// Set a new comparator
FN_EXTERN List *lstComparatorSet(List *this, ListComparator *comparator);

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
FN_EXTERN List *lstClear(List *this);

// Get an item from the list
FN_EXTERN void *lstGet(const List *this, unsigned int listIdx);
FN_EXTERN void *lstGetLast(const List *this);

// Find an item in the list
FN_EXTERN void *lstFind(const List *this, const void *item);
FN_EXTERN const void *lstFindDefault(const List *this, const void *item, const void *itemDefault);
FN_EXTERN unsigned int lstFindIdx(const List *this, const void *item);

// Does an item exist in the list?
FN_INLINE_ALWAYS bool
lstExists(const List *const this, const void *const item)
{
    return lstFind(this, item) != NULL;
}

// Get the index of a list item
FN_EXTERN unsigned int lstIdx(const List *this, const void *item);

// Insert an item into the list
FN_EXTERN void *lstInsert(List *this, unsigned int listIdx, const void *item);

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
FN_EXTERN bool lstRemove(List *this, const void *item);
FN_EXTERN List *lstRemoveIdx(List *this, unsigned int listIdx);
FN_EXTERN List *lstRemoveLast(List *this);

// List sort
FN_EXTERN List *lstSort(List *this, SortOrder sortOrder);

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
FN_EXTERN void lstToLog(const List *this, StringStatic *debugLog);

#define FUNCTION_LOG_LIST_TYPE                                                                                                     \
    List *
#define FUNCTION_LOG_LIST_FORMAT(value, buffer, bufferSize)                                                                        \
    FUNCTION_LOG_OBJECT_FORMAT(value, lstToLog, buffer, bufferSize)

#endif
