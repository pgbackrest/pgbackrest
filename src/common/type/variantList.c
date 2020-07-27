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

/***********************************************************************************************************************************
Wrapper for lstNewP()
***********************************************************************************************************************************/
VariantList *
varLstNew(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN((VariantList *)lstNewP(sizeof(Variant *)));
}

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

/**********************************************************************************************************************************/
VariantList *
varLstAdd(VariantList *this, Variant *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, this);
        FUNCTION_TEST_PARAM(VARIANT, data);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    lstAdd((List *)this, &data);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
Variant *
varLstGet(const VariantList *this, unsigned int listIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, this);
        FUNCTION_TEST_PARAM(UINT, listIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(*(Variant **)lstGet((List *)this, listIdx));
}

/**********************************************************************************************************************************/
unsigned int
varLstSize(const VariantList *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstSize((List *)this));
}

/**********************************************************************************************************************************/
VariantList *
varLstMove(VariantList *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    lstMove((List *)this, parentNew);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
void
varLstFree(VariantList *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, this);
    FUNCTION_TEST_END();

    lstFree((List *)this);

    FUNCTION_TEST_RETURN_VOID();
}
