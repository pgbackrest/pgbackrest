/***********************************************************************************************************************************
String List Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    FUNCTION_TEST_RETURN((StringList *)lstNew(sizeof(String *)));
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
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN((StringList *)lstAdd((List *)this, &string));
}

/***********************************************************************************************************************************
Internal insert -- the string must have been created in the list's mem context before being passed
***********************************************************************************************************************************/
static StringList *
strLstInsertInternal(StringList *this, unsigned int listIdx, String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN((StringList *)lstInsert((List *)this, listIdx, &string));
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
    FUNCTION_TEST_END();

    ASSERT(string != NULL);
    ASSERT(delimiter != NULL);

    FUNCTION_TEST_RETURN(strLstNewSplitZ(string, strPtr(delimiter)));
}

StringList *
strLstNewSplitZ(const String *string, const char *delimiter)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
        FUNCTION_TEST_PARAM(STRINGZ, delimiter);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);
    ASSERT(delimiter != NULL);

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

    FUNCTION_TEST_RETURN(this);
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
    FUNCTION_TEST_END();

    ASSERT(string != NULL);
    ASSERT(delimiter != NULL);

    FUNCTION_TEST_RETURN(strLstNewSplitSizeZ(string, strPtr(delimiter), size));
}

StringList *
strLstNewSplitSizeZ(const String *string, const char *delimiter, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
        FUNCTION_TEST_PARAM(STRINGZ, delimiter);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);
    ASSERT(delimiter != NULL);

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

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
New string list from a variant list -- all variants in the list must be type string
***********************************************************************************************************************************/
StringList *
strLstNewVarLst(const VariantList *sourceList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, sourceList);
    FUNCTION_TEST_END();

    // Create the list
    StringList *this = NULL;

    if  (sourceList != NULL)
    {
        this = strLstNew();

        // Copy variants
        MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
        {
            for (unsigned int listIdx = 0; listIdx < varLstSize(sourceList); listIdx++)
                strLstAddInternal(this, strDup(varStr(varLstGet(sourceList, listIdx))));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Duplicate a string list
***********************************************************************************************************************************/
StringList *
strLstDup(const StringList *sourceList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, sourceList);
    FUNCTION_TEST_END();

    StringList *this = NULL;

    if (sourceList != NULL)
    {
        // Create the list
        this = strLstNew();

        // Copy strings
        MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
        {
            for (unsigned int listIdx = 0; listIdx < strLstSize(sourceList); listIdx++)
                strLstAddInternal(this, strDup(strLstGet(sourceList, listIdx)));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(this);
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
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    bool result = false;

    for (unsigned int listIdx = 0; listIdx < strLstSize(this); listIdx++)
    {
        if (strEq(strLstGet(this, listIdx), string))
        {
            result = true;
            break;
        }
    }

    FUNCTION_TEST_RETURN(result);
}

bool
strLstExistsZ(const StringList *this, const char *cstring)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRINGZ, cstring);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    bool result = false;

    for (unsigned int listIdx = 0; listIdx < strLstSize(this); listIdx++)
    {
        if (strEqZ(strLstGet(this, listIdx), cstring))
        {
            result = true;
            break;
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Add String to the list
***********************************************************************************************************************************/
StringList *
strLstAdd(StringList *this, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstAddInternal(this, strDup(string));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(result);
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
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstAddInternal(this, strNew(string));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(result);
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
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(*(String **)lstGet((List *)this, listIdx));
}


/***********************************************************************************************************************************
Insert String into the list
***********************************************************************************************************************************/
StringList *
strLstInsert(StringList *this, unsigned int listIdx, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstInsertInternal(this, listIdx, strDup(string));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Insert zero-terminated string into the list
***********************************************************************************************************************************/
StringList *
strLstInsertZ(StringList *this, unsigned int listIdx, const char *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
        FUNCTION_TEST_PARAM(STRINGZ, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstInsertInternal(this, listIdx, strNew(string));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(result);
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
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(separator != NULL);

    FUNCTION_TEST_RETURN(strLstJoinQuote(this, separator, ""));
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
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(separator != NULL);
    ASSERT(quote != NULL);

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

    FUNCTION_TEST_RETURN(join);
}

/***********************************************************************************************************************************
Return all items in this list which are not in the anti list

The input lists must *both* be sorted ascending or the results will be unpredictable.
***********************************************************************************************************************************/
StringList *
strLstMergeAnti(const StringList *this, const StringList *anti)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING_LIST, anti);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(anti != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = strLstNew();
        unsigned int antiIdx = 0;

        // Check every item in this
        for (unsigned int thisIdx = 0; thisIdx < strLstSize(this); thisIdx++)
        {
            bool add = true;
            const String *listItem = strLstGet(this, thisIdx);
            ASSERT(listItem != NULL);

            // If anything left in anti look for matches
            while (antiIdx < strLstSize(anti))
            {
                ASSERT(strLstGet(anti, antiIdx) != NULL);
                int compare = strCmp(listItem, strLstGet(anti, antiIdx));

                // If the current item in this is less than the current item in anti then it will be added
                if (compare < 0)
                {
                    break;
                }
                // If they are equal it will not be added
                else if (compare == 0)
                {
                    add = false;
                    antiIdx++;
                    break;
                }
                // Otherwise keep searching anti for a match
                else
                    antiIdx++;
            }

            // Add to the result list if no match found
            if (add)
                strLstAdd(result, listItem);
        }

        strLstMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
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
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    lstMove((List *)this, parentNew);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Return a null-terminated array of pointers to the zero-terminated strings in this list.  DO NOT override const and modify any of the
strings in this array, though it is OK to modify the array itself.
***********************************************************************************************************************************/
const char **
strLstPtr(const StringList *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const char **list = memNew((strLstSize(this) + 1) * sizeof(char *));

    for (unsigned int listIdx = 0; listIdx < strLstSize(this); listIdx++)
    {
        if (strLstGet(this, listIdx) == NULL)
            list[listIdx] = NULL;
        else
            list[listIdx] = strPtr(strLstGet(this, listIdx));
    }

    list[strLstSize(this)] = NULL;

    FUNCTION_TEST_RETURN(list);
}

/***********************************************************************************************************************************
Wrapper for lstSize()
***********************************************************************************************************************************/
unsigned int
strLstSize(const StringList *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstSize((List *)this));
}

/***********************************************************************************************************************************
Sort strings in alphabetical order
***********************************************************************************************************************************/
static int
sortAscComparator(const void *item1, const void *item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, item1);
        FUNCTION_TEST_PARAM_P(VOID, item2);
    FUNCTION_TEST_END();

    ASSERT(item1 != NULL);
    ASSERT(item2 != NULL);

    FUNCTION_TEST_RETURN(strCmp(*(String **)item1, *(String **)item2));
}

static int
sortDescComparator(const void *item1, const void *item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, item1);
        FUNCTION_TEST_PARAM_P(VOID, item2);
    FUNCTION_TEST_END();

    ASSERT(item1 != NULL);
    ASSERT(item2 != NULL);

    FUNCTION_TEST_RETURN(strCmp(*(String **)item2, *(String **)item1));
}

StringList *
strLstSort(StringList *this, SortOrder sortOrder)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(ENUM, sortOrder);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

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

    FUNCTION_TEST_RETURN(this);
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

    FUNCTION_TEST_RETURN_VOID();
}
