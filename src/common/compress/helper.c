/***********************************************************************************************************************************
Compression Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/compress/helper.h"
#include "common/compress/gzip/common.h"
#include "common/compress/gzip/compress.h"
#include "common/compress/lz4/common.h"
#include "common/compress/lz4/compress.h"
#include "common/debug.h"
#include "common/log.h"
#include "version.h"

/**********************************************************************************************************************************/
CompressType
compressTypeEnum(const String *type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, type);
    FUNCTION_TEST_END();

    CompressType result = compressTypeNone;

    if (type != NULL)
    {
        if (strEqZ(type, GZIP_EXT))
            result = compressTypeGzip;
        else if (strEqZ(type, LZ4_EXT))
        {
#ifdef HAVE_LIBLZ4
            result = compressTypeLz4;
#else
            THROW(OptionInvalidValueError, PROJECT_NAME " not compiled with " LZ4_EXT " support");
#endif
        }
        else
            THROW_FMT(AssertError, "invalid compression type '%s'", strPtr(type));
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
bool
compressFilterAdd(IoFilterGroup *filterGroup, CompressType type, int level)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, filterGroup);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(INT, level);
    FUNCTION_LOG_END();

    bool result = false;

    if (type != compressTypeNone)
    {
        switch (type)
        {
            case compressTypeGzip:
            {
                ioFilterGroupAdd(filterGroup, gzipCompressNew(level, false));
                break;
            }

#ifdef HAVE_LIBLZ4
            case compressTypeLz4:
            {
                ioFilterGroupAdd(filterGroup, lz4CompressNew(level));
                break;
            }
#endif

            default:
                THROW_FMT(AssertError, "invalid compression type %u", type);
        }

        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}


/**********************************************************************************************************************************/
const char *
compressExtZ(CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    const char *result = NULL;

    switch (type)
    {
        case compressTypeNone:
        {
            result = "";
            break;
        }

        case compressTypeGzip:
        {
            result = "." GZIP_EXT;
            break;
        }

#ifdef HAVE_LIBLZ4
        case compressTypeLz4:
        {
            result = "." LZ4_EXT;
            break;
        }
#endif

        default:
            THROW_FMT(AssertError, "invalid compression type %u", type);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void
compressExtCat(String *file, CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, file);
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(file != NULL);

    strCat(file, compressExtZ(type));

    FUNCTION_TEST_RETURN_VOID();
}
