/***********************************************************************************************************************************
ZST Common
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBZST

#include <zstd.h>

// Check the version -- this is done in configure but it makes sense to be sure
#if ZSTD_VERSION_MAJOR < 1
    #error "ZSTD_VERSION_MAJOR must be >= 1"
#endif

#include "common/compress/zst/common.h"
#include "common/debug.h"

/**********************************************************************************************************************************/
size_t
zstError(size_t error)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, error);
    FUNCTION_TEST_END();

    if (ZSTD_isError(error))
        THROW_FMT(FormatError, "zst error: [%zd] %s", (ssize_t)error, ZSTD_getErrorName(error));

    FUNCTION_TEST_RETURN(error);
}

#endif // HAVE_LIBLZ4
