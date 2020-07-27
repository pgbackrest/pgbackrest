/***********************************************************************************************************************************
String List Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/stringList.h"

/***********************************************************************************************************************************
Wrapper for lstNew*()
***********************************************************************************************************************************/
StringList *
strLstNew(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN((StringList *)lstNewP(sizeof(String *), .comparator = lstComparatorStr));
}

/***********************************************************************************************************************************
Internal add -- the string must have been created in the list's mem context before being passed
***********************************************************************************************************************************/
static String *
strLstAddInternal(StringList *this, String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(*(String **)lstAdd((List *)this, &string));
}

/***********************************************************************************************************************************
Internal insert -- the string must have been created in the list's mem context before being passed
***********************************************************************************************************************************/
static String *
strLstInsertInternal(StringList *this, unsigned int listIdx, String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(*(String **)lstInsert((List *)this, listIdx, &string));
}

/**********************************************************************************************************************************/
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
        while (stringMatch != NULL);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
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
        while (stringMatch != NULL);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
bool
strLstExists(const StringList *this, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstExists((List *)this, &string));
}

bool
strLstExistsZ(const StringList *this, const char *cstring)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRINGZ, cstring);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const String *string = cstring == NULL ? NULL : STR(cstring);

    FUNCTION_TEST_RETURN(lstExists((List *)this, &string));
}

/**********************************************************************************************************************************/
String *
strLstAdd(StringList *this, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstAddInternal(this, strDup(string));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(result);
}

String *
strLstAddIfMissing(StringList *this, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String **result = lstFind((List *)this, &string);

    if (result == NULL)
        FUNCTION_TEST_RETURN(strLstAdd(this, string));

    FUNCTION_TEST_RETURN(*result);
}

String *
strLstAddZ(StringList *this, const char *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRINGZ, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstAddInternal(this, strNew(string));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
String *
strLstInsert(StringList *this, unsigned int listIdx, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstInsertInternal(this, listIdx, strDup(string));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(result);
}

String *
strLstInsertZ(StringList *this, unsigned int listIdx, const char *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
        FUNCTION_TEST_PARAM(STRINGZ, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstInsertInternal(this, listIdx, strNew(string));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
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
            strCatZ(join, separator);

        if (strLstGet(this, listIdx) == NULL)
            strCatZ(join, "[NULL]");
        else
            strCatFmt(join, "%s%s%s", quote, strPtr(strLstGet(this, listIdx)), quote);
    }

    FUNCTION_TEST_RETURN(join);
}

/**********************************************************************************************************************************/
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

        strLstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
bool
strLstRemove(StringList *this, const String *item)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, item);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(item != NULL);

    FUNCTION_TEST_RETURN(lstRemove((List *)this, &item));
}

StringList *
strLstRemoveIdx(StringList *this, unsigned int listIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN((StringList *)lstRemoveIdx((List *)this, listIdx));
}

/**********************************************************************************************************************************/
unsigned int
strLstSize(const StringList *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstSize((List *)this));
}

/**********************************************************************************************************************************/
StringList *
strLstSort(StringList *this, SortOrder sortOrder)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(ENUM, sortOrder);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    lstSort((List *)this, sortOrder);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
StringList *
strLstComparatorSet(StringList *this, ListComparator *comparator)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(FUNCTIONP, comparator);
    FUNCTION_TEST_END();

    lstComparatorSet((List *)this, comparator);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strLstToLog(const StringList *this)
{
    return strNewFmt("{[%s]}", strPtr(strLstJoinQuote(this, ", ", "\"")));
}

/**********************************************************************************************************************************/
void
strLstFree(StringList *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
    FUNCTION_TEST_END();

    lstFree((List *)this);

    FUNCTION_TEST_RETURN_VOID();
}
