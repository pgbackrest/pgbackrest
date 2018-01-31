/***********************************************************************************************************************************
Variant List Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/memContext.h"
#include "common/type/list.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Wrapper for lstNew()
***********************************************************************************************************************************/
VariantList *
varLstNew()
{
    return (VariantList *)lstNew(sizeof(Variant *));
}

/***********************************************************************************************************************************
Create a variant list from a string list
***********************************************************************************************************************************/
VariantList *
varLstNewStrLst(const StringList *stringList)
{
    VariantList *result = NULL;

    if (stringList != NULL)
    {
        result = varLstNew();

        for (unsigned int listIdx = 0; listIdx < strLstSize(stringList); listIdx++)
            varLstAdd(result, varNewStr(strLstGet(stringList, listIdx)));
    }

    return result;
}

/***********************************************************************************************************************************
Duplicate a variant list
***********************************************************************************************************************************/
VariantList *
varLstDup(const VariantList *source)
{
    VariantList *result = NULL;

    if (source != NULL)
    {
        result = varLstNew();

        for (unsigned int listIdx = 0; listIdx < varLstSize(source); listIdx++)
            varLstAdd(result, varDup(varLstGet(source, listIdx)));
    }

    return result;
}

/***********************************************************************************************************************************
Wrapper for lstAdd()
***********************************************************************************************************************************/
VariantList *
varLstAdd(VariantList *this, Variant *data)
{
    return (VariantList *)lstAdd((List *)this, &data);
}

/***********************************************************************************************************************************
Wrapper for lstGet()
***********************************************************************************************************************************/
Variant *
varLstGet(const VariantList *this, unsigned int listIdx)
{
    return *(Variant **)lstGet((List *)this, listIdx);
}

/***********************************************************************************************************************************
Wrapper for lstSize()
***********************************************************************************************************************************/
unsigned int
varLstSize(const VariantList *this)
{
    return lstSize((List *)this);
}

/***********************************************************************************************************************************
Wrapper for lstFree()
***********************************************************************************************************************************/
void
varLstFree(VariantList *this)
{
    lstFree((List *)this);
}
