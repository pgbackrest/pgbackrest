/***********************************************************************************************************************************
String List Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGLIST_H
#define COMMON_TYPE_STRINGLIST_H

/***********************************************************************************************************************************
String list type
***********************************************************************************************************************************/
typedef struct StringList StringList;

/***********************************************************************************************************************************
Sort orders
***********************************************************************************************************************************/
typedef enum
{
    sortOrderAsc,
    sortOrderDesc,
} SortOrder;

#include "common/memContext.h"
#include "common/type/string.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
StringList *strLstNew();
StringList *strLstNewSplit(const String *string, const String *delimiter);
StringList *strLstNewSplitZ(const String *string, const char *delimiter);
StringList *strLstNewSplitSize(const String *string, const String *delimiter, size_t size);
StringList *strLstNewSplitSizeZ(const String *string, const char *delimiter, size_t size);
StringList *strLstNewVarLst(const VariantList *sourceList);
StringList *strLstDup(const StringList *sourceList);

StringList *strLstAdd(StringList *this, const String *string);
StringList *strLstAddZ(StringList *this, const char *string);
String *strLstGet(const StringList *this, unsigned int listIdx);
String *strLstJoin(const StringList *this, const char *separator);
StringList * strLstMove(StringList *this, MemContext *parentNew);
const char **strLstPtr(const StringList *this);
unsigned int strLstSize(const StringList *this);
StringList *strLstSort(StringList *this, SortOrder sortOrder);

void strLstFree(StringList *this);

#endif
