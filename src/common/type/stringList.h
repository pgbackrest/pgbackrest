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

// Split a string into a string list based on a delimiter
StringList *strLstNewSplit(const String *string, const String *delimiter);
StringList *strLstNewSplitZ(const String *string, const char *delimiter);

// Split a string into a string list based on a delimiter and max size per item. In other words each item in the list will be no
// longer than size even if multiple delimiters are skipped.  This is useful for breaking up text on spaces, for example.
StringList *strLstNewSplitSize(const String *string, const String *delimiter, size_t size);
StringList *strLstNewSplitSizeZ(const String *string, const char *delimiter, size_t size);

// New string list from a variant list -- all variants in the list must be type string
StringList *strLstNewVarLst(const VariantList *sourceList);

// Duplicate a string list
StringList *strLstDup(const StringList *sourceList);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add String to the list
String *strLstAdd(StringList *this, const String *string);
String *strLstAddZ(StringList *this, const char *string);
String *strLstAddIfMissing(StringList *this, const String *string);

// Does the specified string exist in the list?
bool strLstExists(const StringList *this, const String *string);
bool strLstExistsZ(const StringList *this, const char *cstring);

// Insert into the list
String *strLstInsert(StringList *this, unsigned int listIdx, const String *string);
String *strLstInsertZ(StringList *this, unsigned int listIdx, const char *string);

// Get a string by index
String *strLstGet(const StringList *this, unsigned int listIdx);

// Join a list of strings into a single string using the specified separator
String *strLstJoin(const StringList *this, const char *separator);

// Join a list of strings into a single string using the specified separator and quote with specified quote character
String *strLstJoinQuote(const StringList *this, const char *separator, const char *quote);

// Return all items in this list which are not in the anti list. The input lists must *both* be sorted ascending or the results will
// be undefined.
StringList *strLstMergeAnti(const StringList *this, const StringList *anti);

// Move to a new parent mem context
StringList *strLstMove(StringList *this, MemContext *parentNew);

// Return a null-terminated array of pointers to the zero-terminated strings in this list. DO NOT override const and modify any of
// the strings in this array, though it is OK to modify the array itself.
const char **strLstPtr(const StringList *this);

// Remove an item from the list
bool strLstRemove(StringList *this, const String *item);
StringList *strLstRemoveIdx(StringList *this, unsigned int listIdx);

// List size
unsigned int strLstSize(const StringList *this);

// List sort
StringList *strLstSort(StringList *this, SortOrder sortOrder);

/***********************************************************************************************************************************
Setters
***********************************************************************************************************************************/
// Set a new comparator
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
