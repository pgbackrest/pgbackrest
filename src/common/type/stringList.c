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
Wrapper for lstAdd()
***********************************************************************************************************************************/
StringList *
strLstAdd(StringList *this, String *string)
{
    return (StringList *)lstAdd((List *)this, &string);
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
Concatenate a list of strings into a single string using the specified separator
***********************************************************************************************************************************/
String *strLstCat(StringList *this, const char *separator)
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
