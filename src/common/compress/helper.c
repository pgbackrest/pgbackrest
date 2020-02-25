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

/***********************************************************************************************************************************
Configuration for supported and future compression types
***********************************************************************************************************************************/
static const struct compressHelperLocal
{
    const String *const type;                                       // Compress type -- must be extension without period prefixed
    const String *const ext;                                        // File extension with period prefixed
    IoFilter *(*compressNew)(int);                                  // Function to create new compression filter
    IoFilter *(*decompressNew)(void);                               // Function to create new decompression filter
    int levelDefault;                                               // Default compression level
} compressHelperLocal[] =
{
    {
        .type = STRDEF(COMPRESS_TYPE_NONE),
        .ext = STRDEF(""),
    },
    {
        .type = STRDEF(GZIP_EXT),
        .ext = STRDEF("." GZIP_EXT),
        .compressNew = gzipCompressNew,
        .decompressNew = gzipDecompressNew,
        .levelDefault = 6,
    },
    {
        .type = STRDEF(LZ4_EXT),
        .ext = STRDEF("." LZ4_EXT),
#ifdef HAVE_LIBLZ4
        .compressNew = lz4CompressNew,
        .decompressNew = lz4DecompressNew,
        .levelDefault = 1,
#endif
    },
    {
        .type = STRDEF(ZST_EXT),
        .ext = STRDEF("." ZST_EXT),
    },
    {
        .type = STRDEF(XZ_EXT),
        .ext = STRDEF("." XZ_EXT),
    },
    {
        .type = STRDEF(BZ2_EXT),
        .ext = STRDEF("." BZ2_EXT),
    },
};

#define COMPRESS_LIST_SIZE                                                                                                         \
    (sizeof(compressHelperLocal) / sizeof(struct compressHelperLocal))

/**********************************************************************************************************************************/
CompressType
compressTypeEnum(const String *type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, type);
    FUNCTION_TEST_END();

    ASSERT(type != NULL);

    CompressType result = compressTypeNone;

    for (; result < COMPRESS_LIST_SIZE; result++)
    {
        if (strEq(type, compressHelperLocal[result].type))
            break;
    }

    if (result == COMPRESS_LIST_SIZE)
        THROW_FMT(AssertError, "invalid compression type '%s'", strPtr(type));

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void compressTypePresent(CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < COMPRESS_LIST_SIZE);

    if (type != compressTypeNone && compressHelperLocal[type].compressNew == NULL)
    {
        THROW_FMT(
            OptionInvalidValueError, PROJECT_NAME " not compiled with %s support",
            strPtr(compressHelperLocal[type].type));
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
const String *
compressTypeStr(CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < COMPRESS_LIST_SIZE);

    FUNCTION_TEST_RETURN(compressHelperLocal[type].type);
}

/**********************************************************************************************************************************/
CompressType
compressTypeFromName(const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    CompressType result = compressTypeNone + 1;

    for (; result < COMPRESS_LIST_SIZE; result++)
    {
        if (strEndsWith(name, compressHelperLocal[result].ext))
            break;
    }

    if (result == COMPRESS_LIST_SIZE)
        result = compressTypeNone;

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
int
compressLevelDefault(CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < COMPRESS_LIST_SIZE);
    compressTypePresent(type);

    FUNCTION_TEST_RETURN(compressHelperLocal[type].levelDefault);
}

/**********************************************************************************************************************************/
IoFilter *
compressFilter(CompressType type, int level)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(INT, level);
    FUNCTION_TEST_END();

    ASSERT(type < COMPRESS_LIST_SIZE);
    ASSERT(type != compressTypeNone);
    compressTypePresent(type);

    FUNCTION_TEST_RETURN(compressHelperLocal[type].compressNew(level));
}

/**********************************************************************************************************************************/
IoFilter *
compressFilterVar(const String *filterType, const VariantList *filterParamList)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, filterType);
        FUNCTION_LOG_PARAM(VARIANT_LIST, filterParamList);
    FUNCTION_LOG_END();

    ASSERT(filterType != NULL);

    IoFilter *result = NULL;

    for (CompressType compressType = compressTypeNone + 1; compressType < COMPRESS_LIST_SIZE; compressType++)
    {
        if (strBeginsWith(filterType, compressHelperLocal[compressType].type))
        {
            compressTypePresent(compressType);

            switch (strPtr(filterType)[strSize(compressHelperLocal[compressType].type)])
            {
                case 'C':
                {
                    result = compressHelperLocal[compressType].compressNew(varIntForce(varLstGet(filterParamList, 0)));
                    break;
                }

                case 'D':
                {
                    result = compressHelperLocal[compressType].decompressNew();
                    break;
                }
            }
            break;
        }
    }

    FUNCTION_LOG_RETURN(IO_FILTER, result);
}

/**********************************************************************************************************************************/
IoFilter *
decompressFilter(CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < COMPRESS_LIST_SIZE);
    ASSERT(type != compressTypeNone);
    compressTypePresent(type);

    FUNCTION_TEST_RETURN(compressHelperLocal[type].decompressNew());
}

/**********************************************************************************************************************************/
const String *
compressExtStr(CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < COMPRESS_LIST_SIZE);

    FUNCTION_TEST_RETURN(compressHelperLocal[type].ext);
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

    strCat(file, strPtr(compressExtStr(type)));

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

    if (!strEndsWith(file, compressExtStr(type)))
        THROW_FMT(FormatError, "'%s' must have '%s' extension", strPtr(file), strPtr(compressExtStr(type)));

    FUNCTION_TEST_RETURN(strSubN(file, 0, strSize(file) - strSize(compressExtStr(type))));
}
