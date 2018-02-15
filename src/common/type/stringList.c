/***********************************************************************************************************************************
String List Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/memContext.h"
#include "common/type/list.h"
#include "common/type/stringList.h"

/***********************************************************************************************************************************
Wrapper for lstNew()
***********************************************************************************************************************************/
StringList *
strLstNew()
{
    return (StringList *)lstNew(sizeof(String *));
}

/***********************************************************************************************************************************
Split a string into a string list based on a delimiter
***********************************************************************************************************************************/
StringList *
strLstNewSplit(const String *string, const String *delimiter)
{
    return strLstNewSplitZ(string, strPtr(delimiter));
}

StringList *
strLstNewSplitZ(const String *string, const char *delimiter)
{
    // Create the list
    StringList *this = strLstNew();

    // Base points to the beginning of the string that is being searched
    const char *stringBase = strPtr(string);

    // Match points to the next delimiter match that has been found
    const char *stringMatch = NULL;

    do
    {
        // Find a delimiter match
        stringMatch = strstr(stringBase, delimiter);

        // If a match was found then add the string
        if (stringMatch != NULL)
        {
            strLstAdd(this, strNewN(stringBase, stringMatch - stringBase));
            stringBase = stringMatch + strlen(delimiter);
        }
        // Else make whatever is left the last string
        else
            strLstAdd(this, strNew(stringBase));
    }
    while(stringMatch != NULL);

    return this;
}

/***********************************************************************************************************************************
Split a string into a string list based on a delimiter and max size per item

In other words each item in the list will be no longer than size even if multiple delimiters are skipped.  This is useful for
breaking up test on spaces, for example.
***********************************************************************************************************************************/
StringList *
strLstNewSplitSize(const String *string, const String *delimiter, size_t size)
{
    return strLstNewSplitSizeZ(string, strPtr(delimiter), size);
}

StringList *
strLstNewSplitSizeZ(const String *string, const char *delimiter, size_t size)
{
    // Create the list
    StringList *this = strLstNew();

    // Base points to the beginning of the string that is being searched
    const char *stringBase = strPtr(string);

    // Match points to the next delimiter match that has been found
    const char *stringMatchLast = NULL;
    const char *stringMatch = NULL;

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

                strLstAdd(this, strNewN(stringBase, stringMatch - stringBase));
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
                strLstAdd(this, strNewN(stringBase, (stringMatchLast - strlen(delimiter)) - stringBase));
                stringBase = stringMatchLast;
            }

            strLstAdd(this, strNew(stringBase));
        }
    }
    while(stringMatch != NULL);

    return this;
}

/***********************************************************************************************************************************
New string list from a variant list -- all variants in the list must be type string
***********************************************************************************************************************************/
StringList *
strLstNewVarLst(const VariantList *sourceList)
{
    // Create the list
    StringList *this = strLstNew();

    // Copy variants
    for (unsigned int listIdx = 0; listIdx < varLstSize(sourceList); listIdx++)
        strLstAdd(this, varStr(varLstGet(sourceList, listIdx)));

    return this;
}

/***********************************************************************************************************************************
Duplicate a string list
***********************************************************************************************************************************/
StringList *
strLstDup(const StringList *sourceList)
{
    // Create the list
    StringList *this = strLstNew();

    // Copy variants
    for (unsigned int listIdx = 0; listIdx < strLstSize(sourceList); listIdx++)
        strLstAdd(this, strLstGet(sourceList, listIdx));

    return this;
}

/***********************************************************************************************************************************
Wrapper for lstAdd()
***********************************************************************************************************************************/
StringList *
strLstAdd(StringList *this, const String *string)
{
    String *stringCopy = strDup(string);
    return (StringList *)lstAdd((List *)this, &stringCopy);
}

/***********************************************************************************************************************************
Add a zero-terminated string to the list
***********************************************************************************************************************************/
StringList *
strLstAddZ(StringList *this, const char *string)
{
    return strLstAdd(this, strNew(string));
}

/***********************************************************************************************************************************
Wrapper for lstGet()
***********************************************************************************************************************************/
String *
strLstGet(const StringList *this, unsigned int listIdx)
{
    return *(String **)lstGet((List *)this, listIdx);
}

/***********************************************************************************************************************************
Join a list of strings into a single string using the specified separator
***********************************************************************************************************************************/
String *
strLstJoin(const StringList *this, const char *separator)
{
    String *join = strNew("");

    for (unsigned int listIdx = 0; listIdx < strLstSize(this); listIdx++)
    {
        if (listIdx != 0)
            strCat(join, separator);

        if (strLstGet(this, listIdx) == NULL)
            strCat(join, "[NULL]");
        else
            strCat(join, strPtr(strLstGet(this, listIdx)));
    }

    return join;
}

/***********************************************************************************************************************************
Return an array of pointers to the zero-terminated strings in this list.  DO NOT override const and modify any of the strings in
this array, though it is OK to modify the array itself.
***********************************************************************************************************************************/
const char **
strLstPtr(const StringList *this)
{
    const char **list = memNew(strLstSize(this) * sizeof(char *));

    for (unsigned int listIdx = 0; listIdx < strLstSize(this); listIdx++)
    {
        if (strLstGet(this, listIdx) == NULL)
            list[listIdx] = NULL;
        else
            list[listIdx] = strPtr(strLstGet(this, listIdx));
    }

    return list;
}

/***********************************************************************************************************************************
Wrapper for lstSize()
***********************************************************************************************************************************/
unsigned int
strLstSize(const StringList *this)
{
    return lstSize((List *)this);
}

/***********************************************************************************************************************************
Sort strings in alphabetical order
***********************************************************************************************************************************/
static int
sortAscComparator(const void *item1, const void *item2)
{
    return strCmp(*(String **)item1, *(String **)item2);
}

static int
sortDescComparator(const void *item1, const void *item2)
{
    return strCmp(*(String **)item2, *(String **)item1);
}

StringList *
strLstSort(StringList *this, SortOrder sortOrder)
{
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
    return this;
}

/***********************************************************************************************************************************
Wrapper for lstFree()
***********************************************************************************************************************************/
void
strLstFree(StringList *this)
{
    if (this != NULL)
        lstFree((List *)this);
}
