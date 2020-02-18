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

/***********************************************************************************************************************************
Compression options for use by helper functions
***********************************************************************************************************************************/
static struct CompressHelperLocal
{
    CompressType type;
    int level;
} compressHelperLocal;

/**********************************************************************************************************************************/
void
compressInit(bool compress, const String *type, int level)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, compress);
        FUNCTION_LOG_PARAM(STRING, type);
        FUNCTION_LOG_PARAM(INT, level);
    FUNCTION_LOG_END();

    ASSERT(!compress || type != NULL);

    compressHelperLocal = (struct CompressHelperLocal){.type = compressTypeNone};

    if (compress)
    {
        if (strEqZ(type, GZIP_EXT))
            compressHelperLocal.type = compressTypeGzip;
        else if (strEqZ(type, LZ4_EXT))
        {
#ifdef HAVE_LIBLZ4
            compressHelperLocal.type = compressTypeLz4;
#else
            THROW(OptionInvalidValueError, PROJECT_NAME " not compiled with " LZ4_EXT " support");
#endif
        }
        else
            THROW_FMT(AssertError, "invalid compression type '%s'", strPtr(type));

        compressHelperLocal.level = level;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
compressFilterAdd(IoFilterGroup *filterGroup)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, filterGroup);
    FUNCTION_LOG_END();

    bool result = false;

    if (compressHelperLocal.type != compressTypeNone)
    {
        switch (compressHelperLocal.type)
        {
            case compressTypeGzip:
            {
                ioFilterGroupAdd(filterGroup, gzipCompressNew(compressHelperLocal.level, false));
                break;
            }

#ifdef HAVE_LIBLZ4
            case compressTypeLz4:
            {
                ioFilterGroupAdd(filterGroup, lz4CompressNew(compressHelperLocal.level));
                break;
            }
#endif

            default:
                THROW_FMT(AssertError, "invalid compression type '%u'", compressHelperLocal.type);
        }

        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}


/**********************************************************************************************************************************/
const char *
compressExtZ(void)
{
    FUNCTION_TEST_VOID();

    const char *result = NULL;

    switch (compressHelperLocal.type)
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
            THROW_FMT(AssertError, "invalid compression type %u", compressHelperLocal.type);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void
compressExtCat(String *file)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, file);
    FUNCTION_TEST_END();

    ASSERT(file != NULL);

    strCat(file, compressExtZ());

    FUNCTION_TEST_RETURN_VOID();
}
