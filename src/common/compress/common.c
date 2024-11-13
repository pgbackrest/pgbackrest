/***********************************************************************************************************************************
Compression Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/compress/common.h"
#include "common/debug.h"

/**********************************************************************************************************************************/
FN_EXTERN Pack *
compressParamList(const int level, const bool raw)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(INT, level);
        FUNCTION_TEST_PARAM(BOOL, raw);
    FUNCTION_TEST_END();

    Pack *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteI32P(packWrite, level);
        pckWriteBoolP(packWrite, raw);
        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Pack *
decompressParamList(const bool raw)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, raw);
    FUNCTION_TEST_END();

    Pack *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteBoolP(packWrite, raw);
        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(PACK, result);
}
