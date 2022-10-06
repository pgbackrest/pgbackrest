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
// Create empty StringList
FN_INLINE_ALWAYS StringList *
strLstNew(void)
{
    return (StringList *)lstNewP(sizeof(String *), .comparator = lstComparatorStr);
}

// Split a string into a string list based on a delimiter
StringList *strLstNewSplitZ(const String *string, const char *delimiter);

FN_INLINE_ALWAYS StringList *
strLstNewSplit(const String *const string, const String *const delimiter)
{
    return strLstNewSplitZ(string, strZ(delimiter));
}

// New string list from a variant list -- all variants in the list must be type string
StringList *strLstNewVarLst(const VariantList *sourceList);

// Duplicate a string list
StringList *strLstDup(const StringList *sourceList);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Set a new comparator
FN_INLINE_ALWAYS StringList *
strLstComparatorSet(StringList *const this, ListComparator *const comparator)
{
    return (StringList *)lstComparatorSet((List *)this, comparator);
}

// List size
FN_INLINE_ALWAYS unsigned int
strLstSize(const StringList *const this)
{
    return lstSize((List *)this);
}

// Is the list empty?
FN_INLINE_ALWAYS bool
strLstEmpty(const StringList *const this)
{
    return strLstSize(this) == 0;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add String to the list
String *strLstAdd(StringList *this, const String *string);
String *strLstAddSubN(StringList *this, const String *string, size_t offset, size_t size);

FN_INLINE_ALWAYS String *
strLstAddSub(StringList *const this, const String *const string, const size_t size)
{
    return strLstAddSubN(this, string, 0, size);
}

String *strLstAddFmt(StringList *this, const char *format, ...) __attribute__((format(printf, 2, 3)));
String *strLstAddZ(StringList *this, const char *string);
String *strLstAddZSubN(StringList *this, const char *string, size_t offset, size_t size);

FN_INLINE_ALWAYS String *
strLstAddZSub(StringList *const this, const char *const string, const size_t size)
{
    return strLstAddZSubN(this, string, 0, size);
}

String *strLstAddIfMissing(StringList *this, const String *string);

// Does the specified string exist in the list?
FN_INLINE_ALWAYS bool
strLstExists(const StringList *const this, const String *const string)
{
    return lstExists((List *)this, &string);
}

// Find string index in the list
typedef struct StrLstFindIdxParam
{
    VAR_PARAM_HEADER;
    bool required;
} StrLstFindIdxParam;

#define strLstFindIdxP(this, string, ...)                                                                                          \
    strLstFindIdx(this, string, (StrLstFindIdxParam){VAR_PARAM_INIT, __VA_ARGS__})

unsigned int strLstFindIdx(const StringList *this, const String *string, StrLstFindIdxParam param);

// Insert into the list
String *strLstInsert(StringList *this, unsigned int listIdx, const String *string);

// Get a string by index
FN_INLINE_ALWAYS String *
strLstGet(const StringList *const this, const unsigned int listIdx)
{
    return *(String **)lstGet((List *)this, listIdx);
}

// Join a list of strings into a single string using the specified separator and quote with specified quote character
String *strLstJoinQuote(const StringList *this, const char *separator, const char *quote);

// Join a list of strings into a single string using the specified separator
FN_INLINE_ALWAYS String *
strLstJoin(const StringList *const this, const char *const separator)
{
    return strLstJoinQuote(this, separator, "");
}

// Return all items in this list which are not in the anti list. The input lists must *both* be sorted ascending or the results will
// be undefined.
StringList *strLstMergeAnti(const StringList *this, const StringList *anti);

// Move to a new parent mem context
FN_INLINE_ALWAYS StringList *
strLstMove(StringList *const this, MemContext *const parentNew)
{
    return (StringList *)lstMove((List *)this, parentNew);
}

// Return a null-terminated array of pointers to the zero-terminated strings in this list. DO NOT override const and modify any of
// the strings in this array, though it is OK to modify the array itself.
const char **strLstPtr(const StringList *this);

// Remove an item from the list
FN_INLINE_ALWAYS bool
strLstRemove(StringList *const this, const String *const item)
{
    return lstRemove((List *)this, &item);
}

FN_INLINE_ALWAYS StringList *
strLstRemoveIdx(StringList *const this, const unsigned int listIdx)
{
    return (StringList *)lstRemoveIdx((List *)this, listIdx);
}

// List sort
FN_INLINE_ALWAYS StringList *
strLstSort(StringList *const this, const SortOrder sortOrder)
{
    return (StringList *)lstSort((List *)this, sortOrder);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
strLstFree(StringList *const this)
{
    lstFree((List *)this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *strLstToLog(const StringList *this);

#define FUNCTION_LOG_STRING_LIST_TYPE                                                                                              \
    StringList *
#define FUNCTION_LOG_STRING_LIST_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, strLstToLog, buffer, bufferSize)

#endif
