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
    // Create the list
    StringList *this = strLstNew();

    // Base points to the beginning of the string that is being searched
    const char *stringBase = strPtr(string);

    // Match points to the next delimiter match that has been found
    const char *stringMatch = NULL;

    do
    {
        // Find a delimiter match
        stringMatch = strstr(stringBase, strPtr(delimiter));

        // If a match was found then add the string
        if (stringMatch != NULL)
        {
            strLstAdd(this, strNewSzN(stringBase, stringMatch - stringBase));
            stringBase = stringMatch + strSize(delimiter);
        }
        // Else make whatever is left the last string
        else
            strLstAdd(this, strNew(stringBase));
    }
    while(stringMatch != NULL);

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
strLstAdd(StringList *this, String *string)
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
Wrapper for lstFree()
***********************************************************************************************************************************/
void strLstFree(StringList *this)
{
    lstFree((List *)this);
}
