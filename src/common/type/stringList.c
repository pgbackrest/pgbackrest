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
Internal add -- the string must have been created in the list's mem context before being passed
***********************************************************************************************************************************/
FN_INLINE_ALWAYS String *
strLstAddInternal(StringList *const this, String *const string)
{
    return *(String **)lstAdd((List *)this, &string);
}

/***********************************************************************************************************************************
Internal insert -- the string must have been created in the list's mem context before being passed
***********************************************************************************************************************************/
FN_INLINE_ALWAYS String *
strLstInsertInternal(StringList *const this, const unsigned int listIdx, String *const string)
{
    return *(String **)lstInsert((List *)this, listIdx, &string);
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
strLstNewSplitZ(const String *const string, const char *const delimiter)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
        FUNCTION_TEST_PARAM(STRINGZ, delimiter);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);
    ASSERT(delimiter != NULL);

    // Create the list
    StringList *const this = strLstNew();

    // Base points to the beginning of the string that is being searched
    const char *stringBase = strZ(string);

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
                strLstAddInternal(this, strNewZN(stringBase, (size_t)(stringMatch - stringBase)));
                stringBase = stringMatch + strlen(delimiter);
            }
            // Else make whatever is left the last string
            else
                strLstAddInternal(this, strNewZ(stringBase));
        }
        while (stringMatch != NULL);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(STRING_LIST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
strLstNewVarLst(const VariantList *const sourceList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, sourceList);
    FUNCTION_TEST_END();

    // Create the list
    StringList *this = NULL;

    if (sourceList != NULL)
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

    FUNCTION_TEST_RETURN(STRING_LIST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
strLstDup(const StringList *const sourceList)
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

    FUNCTION_TEST_RETURN(STRING_LIST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
strLstAdd(StringList *const this, const String *const string)
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

    FUNCTION_TEST_RETURN(STRING, result);
}

FN_EXTERN String *
strLstAddSubN(StringList *const this, const String *const string, const size_t offset, const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, string);
        FUNCTION_TEST_PARAM(SIZE, offset);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(string != NULL);

    String *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstAddInternal(this, strSubN(string, offset, size));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

FN_EXTERN String *
strLstAddFmt(StringList *const this, const char *const format, ...)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRINGZ, format);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(format != NULL);

    // Determine how long the allocated string needs to be
    va_list argumentList;
    va_start(argumentList, format);
    const size_t size = (size_t)vsnprintf(NULL, 0, format, argumentList);
    va_end(argumentList);

    // Format string
    va_start(argumentList, format);
    char *const buffer = memNew(size + 1);
    vsnprintf(buffer, size + 1, format, argumentList);
    va_end(argumentList);

    String *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstAddInternal(this, strNewZN(buffer, size));
    }
    MEM_CONTEXT_END();

    memFree(buffer);

    FUNCTION_TEST_RETURN(STRING, result);
}

FN_EXTERN String *
strLstAddIfMissing(StringList *const this, const String *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *const *const result = lstFind((List *)this, &string);

    if (result == NULL)
        FUNCTION_TEST_RETURN(STRING, strLstAdd(this, string));

    FUNCTION_TEST_RETURN(STRING, *result);
}

FN_EXTERN String *
strLstAddZ(StringList *const this, const char *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRINGZ, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstAddInternal(this, strNewZ(string));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

FN_EXTERN String *
strLstAddZSubN(StringList *const this, const char *const string, const size_t offset, const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRINGZ, string);
        FUNCTION_TEST_PARAM(SIZE, offset);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(string != NULL);

    String *result = NULL;

    MEM_CONTEXT_BEGIN(lstMemContext((List *)this))
    {
        result = strLstAddInternal(this, strSubN(STR(string), offset, size));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
strLstFindIdx(const StringList *const this, const String *const string, const StrLstFindIdxParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRING, string);
        FUNCTION_TEST_PARAM(BOOL, param.required);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(string != NULL);

    const unsigned int result = lstFindIdx((List *)this, &string);

    if (result == LIST_NOT_FOUND && param.required)
        THROW_FMT(AssertError, "unable to find '%s' in string list", strZ(string));

    FUNCTION_TEST_RETURN(UINT, result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
strLstInsert(StringList *const this, const unsigned int listIdx, const String *const string)
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

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
strLstJoinQuote(const StringList *const this, const char *const separator, const char *const quote)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(STRINGZ, separator);
        FUNCTION_TEST_PARAM(STRINGZ, quote);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(separator != NULL);
    ASSERT(quote != NULL);

    String *const join = strNew();

    for (unsigned int listIdx = 0; listIdx < strLstSize(this); listIdx++)
    {
        if (listIdx != 0)
            strCatZ(join, separator);

        if (strLstGet(this, listIdx) == NULL)
            strCatZ(join, "[NULL]");
        else
            strCatFmt(join, "%s%s%s", quote, strZ(strLstGet(this, listIdx)), quote);
    }

    FUNCTION_TEST_RETURN(STRING, join);
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
strLstMergeAnti(const StringList *const this, const StringList *const anti)
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
            const String *const listItem = strLstGet(this, thisIdx);
            ASSERT(listItem != NULL);

            // If anything left in anti look for matches
            while (antiIdx < strLstSize(anti))
            {
                ASSERT(strLstGet(anti, antiIdx) != NULL);
                const int compare = strCmp(listItem, strLstGet(anti, antiIdx));

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

    FUNCTION_TEST_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
FN_EXTERN const char **
strLstPtr(const StringList *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const char **const list = memNew((strLstSize(this) + 1) * sizeof(char *));

    for (unsigned int listIdx = 0; listIdx < strLstSize(this); listIdx++)
    {
        if (strLstGet(this, listIdx) == NULL)
            list[listIdx] = NULL;
        else
            list[listIdx] = strZ(strLstGet(this, listIdx));
    }

    list[strLstSize(this)] = NULL;

    FUNCTION_TEST_RETURN_CONST_P(STRINGZ, list);
}

/**********************************************************************************************************************************/
FN_EXTERN void
strLstToLog(const StringList *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{[");

    for (unsigned int strLstIdx = 0; strLstIdx < strLstSize(this); strLstIdx++)
    {
        const String *const value = strLstGet(this, strLstIdx);

        if (strLstIdx != 0)
            strStcCat(debugLog, ", ");

        if (value == NULL)
            strStcCat(debugLog, NULL_Z);
        else
            strStcFmt(debugLog, "\"%s\"", strZ(value));
    }

    strStcCat(debugLog, "]}");
}
