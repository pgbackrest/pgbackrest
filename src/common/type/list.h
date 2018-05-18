/***********************************************************************************************************************************
List Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_LIST_H
#define COMMON_TYPE_LIST_H

/***********************************************************************************************************************************
List object
***********************************************************************************************************************************/
typedef struct List List;

#include "common/memContext.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Define initial size of a list
***********************************************************************************************************************************/
#define LIST_INITIAL_SIZE                                           8

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
List *lstNew(size_t itemSize);
List *lstAdd(List *this, const void *item);
void *lstGet(const List *this, unsigned int listIdx);
MemContext *lstMemContext(const List *this);
List *lstMove(List *this, MemContext *parentNew);
unsigned int lstSize(const List *this);
List *lstSort(List *this, int (*comparator)(const void *, const void*));
void lstFree(List *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
size_t lstToLog(const List *this, char *buffer, size_t bufferSize);

#define FUNCTION_DEBUG_LIST_TYPE                                                                                                   \
    List *
#define FUNCTION_DEBUG_LIST_FORMAT(value, buffer, bufferSize)                                                                      \
    lstToLog(value, buffer, bufferSize)

#endif
