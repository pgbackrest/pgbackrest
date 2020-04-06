/***********************************************************************************************************************************
List Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_LIST_H
#define COMMON_TYPE_LIST_H

#include <limits.h>

/***********************************************************************************************************************************
List object
***********************************************************************************************************************************/
#define LIST_TYPE                                                   List
#define LIST_PREFIX                                                 lst

typedef struct List List;

#include "common/memContext.h"
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
List *lstNew(size_t itemSize);

typedef struct ListParam
{
    VAR_PARAM_HEADER;
    SortOrder sortOrder;
    ListComparator *comparator;
} ListParam;

#define lstNewP(itemSize, ...)                                                                                                     \
    lstNewParam(itemSize, (ListParam){VAR_PARAM_INIT, __VA_ARGS__})

List *lstNewParam(size_t itemSize, ListParam param);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add an item to the end of the list
void *lstAdd(List *this, const void *item);

// Clear items from a list
List *lstClear(List *this);

// Get an item from the list
void *lstGet(const List *this, unsigned int listIdx);

// Does an item exist in the list?
bool lstExists(const List *this, const void *item);

// Find an item in the list
void *lstFind(const List *this, const void *item);
void *lstFindDefault(const List *this, const void *item, void *itemDefault);
unsigned int lstFindIdx(const List *this, const void *item);

// Get the index of a list item
unsigned int lstIdx(const List *this, const void *item);

// Insert an item into the list
void *lstInsert(List *this, unsigned int listIdx, const void *item);

// Memory context for this list
MemContext *lstMemContext(const List *this);

// Move to a new parent mem context
List *lstMove(List *this, MemContext *parentNew);

// Remove an item from the list
bool lstRemove(List *this, const void *item);
List *lstRemoveIdx(List *this, unsigned int listIdx);

// Return list size
unsigned int lstSize(const List *this);

// List sort
List *lstSort(List *this, SortOrder sortOrder);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Set a new comparator
List *lstComparatorSet(List *this, ListComparator *comparator);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void lstFree(List *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *lstToLog(const List *this);

#define FUNCTION_LOG_LIST_TYPE                                                                                                     \
    List *
#define FUNCTION_LOG_LIST_FORMAT(value, buffer, bufferSize)                                                                        \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, lstToLog, buffer, bufferSize)

#endif
