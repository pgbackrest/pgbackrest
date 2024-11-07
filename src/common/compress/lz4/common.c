/***********************************************************************************************************************************
LZ4 Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include <lz4frame.h>

#include "common/compress/lz4/common.h"
#include "common/debug.h"

/**********************************************************************************************************************************/
FN_EXTERN LZ4F_errorCode_t
lz4Error(const LZ4F_errorCode_t error)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, error);
    FUNCTION_TEST_END();

    if (LZ4F_isError(error))
        THROW_FMT(FormatError, "lz4 error: [%zd] %s", (ssize_t)error, LZ4F_getErrorName(error));

    FUNCTION_TEST_RETURN_TYPE(LZ4F_errorCode_t, error);
}
