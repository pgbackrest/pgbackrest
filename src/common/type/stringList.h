/***********************************************************************************************************************************
String List Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGLIST_H
#define COMMON_TYPE_STRINGLIST_H

/***********************************************************************************************************************************
StringList object
***********************************************************************************************************************************/
typedef struct StringList StringList;

#include "common/memContext.h"
#include "common/type/list.h"
#include "common/type/string.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
StringList *strLstNew(void);
StringList *strLstNewParam(ListComparator *comparator);
StringList *strLstNewSplit(const String *string, const String *delimiter);
StringList *strLstNewSplitZ(const String *string, const char *delimiter);
StringList *strLstNewSplitSize(const String *string, const String *delimiter, size_t size);
StringList *strLstNewSplitSizeZ(const String *string, const char *delimiter, size_t size);
StringList *strLstNewVarLst(const VariantList *sourceList);
StringList *strLstDup(const StringList *sourceList);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
String *strLstAdd(StringList *this, const String *string);
String *strLstAddZ(StringList *this, const char *string);
String *strLstAddIfMissing(StringList *this, const String *string);
bool strLstExists(const StringList *this, const String *string);
bool strLstExistsZ(const StringList *this, const char *cstring);
String *strLstInsert(StringList *this, unsigned int listIdx, const String *string);
String *strLstInsertZ(StringList *this, unsigned int listIdx, const char *string);
String *strLstGet(const StringList *this, unsigned int listIdx);
String *strLstJoin(const StringList *this, const char *separator);
String *strLstJoinQuote(const StringList *this, const char *separator, const char *quote);
StringList *strLstMergeAnti(const StringList *this, const StringList *anti);
StringList *strLstMove(StringList *this, MemContext *parentNew);
const char **strLstPtr(const StringList *this);
bool strLstRemove(StringList *this, const String *item);
unsigned int strLstSize(const StringList *this);
StringList *strLstSort(StringList *this, SortOrder sortOrder);

/***********************************************************************************************************************************
Setters
***********************************************************************************************************************************/
StringList *strLstComparatorSet(StringList *this, ListComparator *comparator);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void strLstFree(StringList *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *strLstToLog(const StringList *this);

#define FUNCTION_LOG_STRING_LIST_TYPE                                                                                              \
    StringList *
#define FUNCTION_LOG_STRING_LIST_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, strLstToLog, buffer, bufferSize)

#endif
