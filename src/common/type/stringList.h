/***********************************************************************************************************************************
String List Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGLIST_H
#define COMMON_TYPE_STRINGLIST_H

/***********************************************************************************************************************************
StringList object
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
StringList *strLstNew(void);
StringList *strLstNewSplit(const String *string, const String *delimiter);
StringList *strLstNewSplitZ(const String *string, const char *delimiter);
StringList *strLstNewSplitSize(const String *string, const String *delimiter, size_t size);
StringList *strLstNewSplitSizeZ(const String *string, const char *delimiter, size_t size);
StringList *strLstNewVarLst(const VariantList *sourceList);
StringList *strLstDup(const StringList *sourceList);

StringList *strLstAdd(StringList *this, const String *string);
StringList *strLstAddZ(StringList *this, const char *string);
bool strLstExists(const StringList *this, const String *string);
bool strLstExistsZ(const StringList *this, const char *cstring);
String *strLstGet(const StringList *this, unsigned int listIdx);
String *strLstJoin(const StringList *this, const char *separator);
String *strLstJoinQuote(const StringList *this, const char *separator, const char *quote);
StringList * strLstMove(StringList *this, MemContext *parentNew);
const char **strLstPtr(const StringList *this);
unsigned int strLstSize(const StringList *this);
StringList *strLstSort(StringList *this, SortOrder sortOrder);

void strLstFree(StringList *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *strLstToLog(const StringList *this);

#define FUNCTION_DEBUG_STRING_LIST_TYPE                                                                                            \
    StringList *
#define FUNCTION_DEBUG_STRING_LIST_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_DEBUG_STRING_OBJECT_FORMAT(value, strLstToLog, buffer, bufferSize)

#endif
