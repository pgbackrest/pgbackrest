/***********************************************************************************************************************************
Compression Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/compress/helper.h"
#include "common/compress/gzip/common.h"
#include "common/compress/gzip/compress.h"
#include "common/compress/gzip/decompress.h"
#include "common/compress/lz4/common.h"
#include "common/compress/lz4/compress.h"
#include "common/compress/lz4/decompress.h"
#include "common/debug.h"
#include "common/log.h"
#include "version.h"

/***********************************************************************************************************************************
Compression type constants
***********************************************************************************************************************************/
#define COMPRESS_TYPE_NONE                                          "none"

// Constants for currently unsupported compression types
#define ZST_EXT                                                     "zst"
#define BZ2_EXT                                                     "bz2"
#define XZ_EXT                                                      "xz"

static const struct compressHelperLocal
{
    const String *const type;
    bool present;
    const String *const ext;
} compressHelperLocal[] =
{
    {
        .type = STRDEF(COMPRESS_TYPE_NONE),
        .present = true,
        .ext = STRDEF(""),
    },
    {
        .type = STRDEF(GZIP_EXT),
        .present = true,
        .ext = STRDEF("." GZIP_EXT),
    },
    {
        .type = STRDEF(LZ4_EXT),
#ifdef HAVE_LIBLZ4
        .present = true,
#endif
        .ext = STRDEF("." LZ4_EXT),
    },
    {
        .type = STRDEF(ZST_EXT),
        .present = false,
        .ext = STRDEF("." ZST_EXT),
    },
    {
        .type = STRDEF(XZ_EXT),
        .present = false,
        .ext = STRDEF("." XZ_EXT),
    },
    {
        .type = STRDEF(BZ2_EXT),
        .present = false,
        .ext = STRDEF("." BZ2_EXT),
    },
};

/**********************************************************************************************************************************/
CompressType
compressTypeEnum(const String *type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, type);
    FUNCTION_TEST_END();

    ASSERT(type != NULL);

    CompressType result = compressTypeNone;

    for (; result < sizeof(compressHelperLocal) / sizeof(struct compressHelperLocal); result++)
    {
        if (strEq(type, compressHelperLocal[result].type))
        {
            if (!compressHelperLocal[result].present)
            {
                THROW_FMT(
                    OptionInvalidValueError, PROJECT_NAME " not compiled with %s support",
                    strPtr(compressHelperLocal[result].type));
            }

            break;
        }
    }

    if (result == sizeof(compressHelperLocal) / sizeof(struct compressHelperLocal))
        THROW_FMT(AssertError, "invalid compression type '%s'", strPtr(type));

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
const char *
compressTypeZ(CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < sizeof(compressHelperLocal) / sizeof(struct compressHelperLocal));

    FUNCTION_TEST_RETURN(strPtr(compressHelperLocal[type].type));
}

/**********************************************************************************************************************************/
CompressType
compressTypeFromName(const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    CompressType result = compressTypeNone + 1;

    for (; result < sizeof(compressHelperLocal) / sizeof(struct compressHelperLocal); result++)
    {
        if (strEndsWith(name, compressHelperLocal[result].ext))
            break;
    }

    if (result == sizeof(compressHelperLocal) / sizeof(struct compressHelperLocal))
        result = compressTypeNone;

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
                ioFilterGroupAdd(filterGroup, gzipCompressNew(level));
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
bool
decompressFilterAdd(IoFilterGroup *filterGroup, CompressType type)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, filterGroup);
        FUNCTION_LOG_PARAM(ENUM, type);
    FUNCTION_LOG_END();

    bool result = false;

    if (type != compressTypeNone)
    {
        switch (type)
        {
            case compressTypeGzip:
            {
                ioFilterGroupAdd(filterGroup, gzipDecompressNew());
                break;
            }

#ifdef HAVE_LIBLZ4
            case compressTypeLz4:
            {
                ioFilterGroupAdd(filterGroup, lz4DecompressNew());
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

    ASSERT(type < sizeof(compressHelperLocal) / sizeof(struct compressHelperLocal));

    FUNCTION_TEST_RETURN(strPtr(compressHelperLocal[type].ext));
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

/**********************************************************************************************************************************/
String *
compressExtStrip(const String *file, CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, file);
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(file != NULL);

    if (!strEndsWithZ(file, compressExtZ(type)))
        THROW_FMT(FormatError, "'%s' must have '%s' extension", strPtr(file), compressExtZ(type));

    FUNCTION_TEST_RETURN(strSubN(file, 0, strSize(file) - strlen(compressExtZ(type))));
}
