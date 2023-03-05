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
FN_EXTERN VariantList *
varLstNewStrLst(const StringList *const stringList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, stringList);
    FUNCTION_TEST_END();

    VariantList *this = NULL;

    if (stringList != NULL)
    {
        this = varLstNew();

        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            for (unsigned int listIdx = 0; listIdx < strLstSize(stringList); listIdx++)
                varLstAdd(this, varNewStr(strLstGet(stringList, listIdx)));
        }
        MEM_CONTEXT_OBJ_END();
    }

    FUNCTION_TEST_RETURN(VARIANT_LIST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN VariantList *
varLstDup(const VariantList *const source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, source);
    FUNCTION_TEST_END();

    VariantList *this = NULL;

    if (source != NULL)
    {
        this = varLstNew();

        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            for (unsigned int listIdx = 0; listIdx < varLstSize(source); listIdx++)
                varLstAdd(this, varDup(varLstGet(source, listIdx)));
        }
        MEM_CONTEXT_OBJ_END();
    }

    FUNCTION_TEST_RETURN(VARIANT_LIST, this);
}
