/***********************************************************************************************************************************
String List Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/list.h"
#include "common/type/stringList.h"

/***********************************************************************************************************************************
Wrapper for lstNew()
***********************************************************************************************************************************/
StringList *
strLstNew(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RESULT(STRING_LIST, (StringList *)lstNew(sizeof(String *)));
}

/***********************************************************************************************************************************
Internal add -- the string must have been created in the list's mem context before being passed
***********************************************************************************************************************************/
static StringList *
strLstAddInternal(StringList *this, String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, string);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING_LIST, (StringList *)lstAdd((List *)this, &string));
}

/***********************************************************************************************************************************
Split a string into a string list based on a delimiter
***********************************************************************************************************************************/
StringList *
strLstNewSplit(const String *string, const String *delimiter)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
        FUNCTION_TEST_PARAM(STRING, delimiter);

        FUNCTION_TEST_ASSERT(string != NULL);
        FUNCTION_TEST_ASSERT(delimiter != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING_LIST, strLstNewSplitZ(string, strPtr(delimiter)));
}

StringList *
strLstNewSplitZ(const String *string, const char *delimiter)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
        FUNCTION_TEST_PARAM(STRINGZ, delimiter);

        FUNCTION_TEST_ASSERT(string != NULL);
        FUNCTION_TEST_ASSERT(delimiter != NULL);
    FUNCTION_TEST_END();

    // Create the list
    StringList *this = strLstNew();

    // Base points to the beginning of the string that is being searched
    const char *stringBase = strPtr(string);

    // Match points to the next delimiter match that has been found
    const char *stringMatch = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        do
        {
            // Find a delimiter match
            stringMatch = strstr(stringBase, delimiter);

            // If a match was found then add the string
            if (stringMatch != NULL)
            {
                strLstAddInternal(this, strNewN(stringBase, (size_t)(stringMatch - stringBase)));
                stringBase = stringMatch + strlen(delimiter);
            }
            // Else make whatever is left the last string
            else
                strLstAddInternal(this, strNew(stringBase));
        }
        while(stringMatch != NULL);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RESULT(STRING_LIST, this);
}

/***********************************************************************************************************************************
Split a string into a string list based on a delimiter and max size per item

In other words each item in the list will be no longer than size even if multiple delimiters are skipped.  This is useful for
breaking up test on spaces, for example.
***********************************************************************************************************************************/
StringList *
strLstNewSplitSize(const String *string, const String *delimiter, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
        FUNCTION_TEST_PARAM(STRING, delimiter);
        FUNCTION_TEST_PARAM(SIZE, size);

        FUNCTION_TEST_ASSERT(string != NULL);
        FUNCTION_TEST_ASSERT(delimiter != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING_LIST, strLstNewSplitSizeZ(string, strPtr(delimiter), size));
}

StringList *
strLstNewSplitSizeZ(const String *string, const char *delimiter, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
        FUNCTION_TEST_PARAM(STRINGZ, delimiter);
        FUNCTION_TEST_PARAM(SIZE, size);

        FUNCTION_TEST_ASSERT(string != NULL);
        FUNCTION_TEST_ASSERT(delimiter != NULL);
    FUNCTION_TEST_END();

    // Create the list
    StringList *this = strLstNew();

    // Base points to the beginning of the string that is being searched
    const char *stringBase = strPtr(string);

    // Match points to the next delimiter match that has been found
    const char *stringMatchLast = NULL;
    const char *stringMatch = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        do
        {
            // Find a delimiter match
            stringMatch = strstr(stringMatchLast == NULL ? stringBase : stringMatchLast, delimiter);

            // If a match was found then add the string
            if (stringMatch != NULL)
            {
                if ((size_t)(stringMatch - stringBase) >= size)
                {
                    if (stringMatchLast != NULL)
                        stringMatch = stringMatchLast - strlen(delimiter);

                    strLstAddInternal(this, strNewN(stringBase, (size_t)(stringMatch - stringBase)));
                    stringBase = stringMatch + strlen(delimiter);
                    stringMatchLast = NULL;
                }
                else
                    stringMatchLast = stringMatch + strlen(delimiter);
            }
            // Else make whatever is left the last string
            else
            {
                if (stringMatchLast != NULL && strlen(stringBase) - strlen(delimiter) >= size)
                {
                    strLstAddInternal(this, strNewN(stringBase, (size_t)((stringMatchLast - strlen(delimiter)) - stringBase)));
                    stringBase = stringMatchLast;
                }

                strLstAddInternal(this, strNew(stringBase));
            }
        }
        while(stringMatch != NULL);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RESULT(STRING_LIST, this);
}

/***********************************************************************************************************************************
New string list from a variant list -- all variants in the list must be type string
***********************************************************************************************************************************/
StringList *
strLstNewVarLst(const VariantList *sourceList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, sourceList);

        FUNCTION_TEST_ASSERT(sourceList != NULL);
    FUNCTION_TEST_END();

    // Create the list
    StringList *this = strLstNew();

    // Copy variants
    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        for (unsigned int listIdx = 0; listIdx < varLstSize(sourceList); listIdx++)
            strLstAddInternal(this, strDup(varStr(varLstGet(sourceList, listIdx))));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RESULT(STRING_LIST, this);
}

/***********************************************************************************************************************************
Duplicate a string list
***********************************************************************************************************************************/
StringList *
strLstDup(const StringList *sourceList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, sourceList);

        FUNCTION_TEST_ASSERT(sourceList != NULL);
    FUNCTION_TEST_END();

    // Create the list
    StringList *this = strLstNew();

    // Copy strings
    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        for (unsigned int listIdx = 0; listIdx < strLstSize(sourceList); listIdx++)
            strLstAddInternal(this, strDup(strLstGet(sourceList, listIdx)));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RESULT(STRING_LIST, this);
}

/***********************************************************************************************************************************
Does the specified string exist in the list?
***********************************************************************************************************************************/
bool
strLstExists(const StringList *this, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, string);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    bool result = false;

    for (unsigned int listIdx = 0; listIdx < strLstSize(this); listIdx++)
    {
        if (strEq(strLstGet(this, listIdx), string))
        {
            result = true;
            break;
        }
    }

    FUNCTION_TEST_RESULT(BOOL, result);
}

bool
strLstExistsZ(const StringList *this, const char *cstring)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRINGZ, cstring);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    bool result = false;

    for (unsigned int listIdx = 0; listIdx < strLstSize(this); listIdx++)
    {
        if (strEqZ(strLstGet(this, listIdx), cstring))
        {
            result = true;
            break;
        }
    }

    FUNCTION_TEST_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Wrapper for lstAdd()
***********************************************************************************************************************************/
StringList *
strLstAdd(StringList *this, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, string);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    StringList *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstAddInternal(this, strDup(string));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RESULT(STRING_LIST, result);
}

/***********************************************************************************************************************************
Add a zero-terminated string to the list
***********************************************************************************************************************************/
StringList *
strLstAddZ(StringList *this, const char *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRINGZ, string);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    StringList *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstAddInternal(this, strNew(string));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RESULT(STRING_LIST, result);
}

/***********************************************************************************************************************************
Wrapper for lstGet()
***********************************************************************************************************************************/
String *
strLstGet(const StringList *this, unsigned int listIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, *(String **)lstGet((List *)this, listIdx));
}

/***********************************************************************************************************************************
Join a list of strings into a single string using the specified separator
***********************************************************************************************************************************/
String *
strLstJoin(const StringList *this, const char *separator)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRINGZ, separator);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(separator != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, strLstJoinQuote(this, separator, ""));
}

/***********************************************************************************************************************************
Join a list of strings into a single string using the specified separator and quote with specified quote character
***********************************************************************************************************************************/
String *
strLstJoinQuote(const StringList *this, const char *separator, const char *quote)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRINGZ, separator);
        FUNCTION_TEST_PARAM(STRINGZ, quote);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(separator != NULL);
        FUNCTION_TEST_ASSERT(quote != NULL);
    FUNCTION_TEST_END();

    String *join = strNew("");

    for (unsigned int listIdx = 0; listIdx < strLstSize(this); listIdx++)
    {
        if (listIdx != 0)
            strCat(join, separator);

        if (strLstGet(this, listIdx) == NULL)
            strCat(join, "[NULL]");
        else
            strCatFmt(join, "%s%s%s", quote, strPtr(strLstGet(this, listIdx)), quote);
    }

    FUNCTION_TEST_RESULT(STRING, join);
}

/***********************************************************************************************************************************
Move the string list
***********************************************************************************************************************************/
StringList *
strLstMove(StringList *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);

        FUNCTION_TEST_ASSERT(parentNew != NULL);
    FUNCTION_TEST_END();

    lstMove((List *)this, parentNew);

    FUNCTION_TEST_RESULT(STRING_LIST, this);
}

/***********************************************************************************************************************************
Return an array of pointers to the zero-terminated strings in this list.  DO NOT override const and modify any of the strings in
this array, though it is OK to modify the array itself.
***********************************************************************************************************************************/
const char **
strLstPtr(const StringList *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    const char **list = memNew(strLstSize(this) * sizeof(char *));

    for (unsigned int listIdx = 0; listIdx < strLstSize(this); listIdx++)
    {
        if (strLstGet(this, listIdx) == NULL)
            list[listIdx] = NULL;
        else
            list[listIdx] = strPtr(strLstGet(this, listIdx));
    }

    FUNCTION_TEST_RESULT(CONST_CHARPP, list);
}

/***********************************************************************************************************************************
Wrapper for lstSize()
***********************************************************************************************************************************/
unsigned int
strLstSize(const StringList *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(UINT, lstSize((List *)this));
}

/***********************************************************************************************************************************
Sort strings in alphabetical order
***********************************************************************************************************************************/
static int
sortAscComparator(const void *item1, const void *item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VOIDP, item1);
        FUNCTION_TEST_PARAM(VOIDP, item2);

        FUNCTION_TEST_ASSERT(item1 != NULL);
        FUNCTION_TEST_ASSERT(item2 != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(INT, strCmp(*(String **)item1, *(String **)item2));
}

static int
sortDescComparator(const void *item1, const void *item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VOIDP, item1);
        FUNCTION_TEST_PARAM(VOIDP, item2);

        FUNCTION_TEST_ASSERT(item1 != NULL);
        FUNCTION_TEST_ASSERT(item2 != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(INT, strCmp(*(String **)item2, *(String **)item1));
}

StringList *
strLstSort(StringList *this, SortOrder sortOrder)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(ENUM, sortOrder);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    switch (sortOrder)
    {
        case sortOrderAsc:
        {
            lstSort((List *)this, sortAscComparator);
            break;
        }

        case sortOrderDesc:
        {
            lstSort((List *)this, sortDescComparator);
            break;
        }
    }

    FUNCTION_TEST_RESULT(STRING_LIST, this);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
strLstToLog(const StringList *this)
{
    return strNewFmt("{[%s]}", strPtr(strLstJoinQuote(this, ", ", "\"")));
}

/***********************************************************************************************************************************
Wrapper for lstFree()
***********************************************************************************************************************************/
void
strLstFree(StringList *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
    FUNCTION_TEST_END();

    lstFree((List *)this);

    FUNCTION_TEST_RESULT_VOID();
}
