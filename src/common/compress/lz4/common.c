/***********************************************************************************************************************************
LZ4 Common
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBLZ4

#include <lz4frame.h>

#include "common/compress/lz4/common.h"
#include "common/debug.h"

/***********************************************************************************************************************************
Process lz4 errors
***********************************************************************************************************************************/
LZ4F_errorCode_t
lz4Error(LZ4F_errorCode_t error)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, error);
    FUNCTION_TEST_END();

    if (LZ4F_isError(error))
        THROW_FMT(FormatError, "lz4 error: [%zd] %s", (ssize_t)error, LZ4F_getErrorName(error));

    FUNCTION_TEST_RETURN(error);
}

#endif // HAVE_LIBLZ4
