/***********************************************************************************************************************************
List Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_LIST_H
#define COMMON_TYPE_LIST_H

/***********************************************************************************************************************************
List object
***********************************************************************************************************************************/
#define LIST_TYPE                                                   List
#define LIST_PREFIX                                                 lst

typedef struct List List;

#include "common/memContext.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Define initial size of a list
***********************************************************************************************************************************/
#define LIST_INITIAL_SIZE                                           8

/***********************************************************************************************************************************
Function type for comparing items in the list

The return value should be -1 when item1 < item2, 0 when item1 == item2, and 1 when item2 > item1.
***********************************************************************************************************************************/
typedef int ListComparator(const void *item1, const void *item2);

// General purpose list comparator for Strings structs with a String as the first member
int lstComparatorStr(const void *item1, const void *item2);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
List *lstNew(size_t itemSize);
List *lstNewParam(size_t itemSize, ListComparator *comparator);
List *lstAdd(List *this, const void *item);
List *lstClear(List *this);
void *lstGet(const List *this, unsigned int listIdx);
void *lstFindDefault(const List *this, void *item, void *itemDefault);
List *lstInsert(List *this, unsigned int listIdx, const void *item);
List *lstRemove(List *this, unsigned int listIdx);
MemContext *lstMemContext(const List *this);
List *lstMove(List *this, MemContext *parentNew);
unsigned int lstSize(const List *this);
List *lstSort(List *this, int (*comparator)(const void *, const void*));
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
