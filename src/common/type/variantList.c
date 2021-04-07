/***********************************************************************************************************************************
Variant List Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/list.h"
#include "common/type/variantList.h"

/**********************************************************************************************************************************/
VariantList *
varLstNewStrLst(const StringList *stringList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, stringList);
    FUNCTION_TEST_END();

    VariantList *result = NULL;

    if (stringList != NULL)
    {
        result = varLstNew();

        for (unsigned int listIdx = 0; listIdx < strLstSize(stringList); listIdx++)
            varLstAdd(result, varNewStr(strLstGet(stringList, listIdx)));
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
VariantList *
varLstDup(const VariantList *source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, source);
    FUNCTION_TEST_END();

    VariantList *result = NULL;

    if (source != NULL)
    {
        result = varLstNew();

        for (unsigned int listIdx = 0; listIdx < varLstSize(source); listIdx++)
            varLstAdd(result, varDup(varLstGet(source, listIdx)));
    }

    FUNCTION_TEST_RETURN(result);
}
