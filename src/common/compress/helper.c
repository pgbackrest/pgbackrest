/***********************************************************************************************************************************
Compression Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/compress/bz2/common.h"
#include "common/compress/bz2/compress.h"
#include "common/compress/bz2/decompress.h"
#include "common/compress/gz/common.h"
#include "common/compress/gz/compress.h"
#include "common/compress/gz/decompress.h"
#include "common/compress/helper.h"
#include "common/compress/helper.intern.h"
#include "common/compress/lz4/common.h"
#include "common/compress/lz4/compress.h"
#include "common/compress/lz4/decompress.h"
#include "common/compress/zst/common.h"
#include "common/compress/zst/compress.h"
#include "common/compress/zst/decompress.h"
#include "common/debug.h"
#include "common/log.h"
#include "version.h"

/***********************************************************************************************************************************
Compression type constants
***********************************************************************************************************************************/
#define COMPRESS_TYPE_NONE                                          "none"

// Constants for currently unsupported compression types
#define XZ_EXT                                                      "xz"

/***********************************************************************************************************************************
Configuration for supported and future compression types
***********************************************************************************************************************************/
static const struct CompressHelperLocal
{
    StringId typeId;                                                // Compress type id
    const String *const type;                                       // Compress type -- must be extension without period prefixed
    const String *const ext;                                        // File extension with period prefixed
    StringId compressType;                                          // Type of the compression filter
    IoFilter *(*compressNew)(int, bool);                            // Function to create new compression filter
    StringId decompressType;                                        // Type of the decompression filter
    IoFilter *(*decompressNew)(bool);                               // Function to create new decompression filter
    int levelDefault : 8;                                           // Default compression level
    int levelMin : 8;                                               // Minimum compression level
    int levelMax : 8;                                               // Maximum compression level
} compressHelperLocal[] =
{
    {
        .typeId = STRID5("none", 0x2b9ee0),
        .type = STRDEF(COMPRESS_TYPE_NONE),
        .ext = STRDEF(""),
    },
    {
        .typeId = STRID5("bz2", 0x73420),
        .type = STRDEF(BZ2_EXT),
        .ext = STRDEF("." BZ2_EXT),
        .compressType = BZ2_COMPRESS_FILTER_TYPE,
        .compressNew = bz2CompressNew,
        .decompressType = BZ2_DECOMPRESS_FILTER_TYPE,
        .decompressNew = bz2DecompressNew,
        .levelDefault = BZ2_COMPRESS_LEVEL_DEFAULT,
        .levelMin = BZ2_COMPRESS_LEVEL_MIN,
        .levelMax = BZ2_COMPRESS_LEVEL_MAX,
    },
    {
        .typeId = STRID5("gz", 0x3470),
        .type = STRDEF(GZ_EXT),
        .ext = STRDEF("." GZ_EXT),
        .compressType = GZ_COMPRESS_FILTER_TYPE,
        .compressNew = gzCompressNew,
        .decompressType = GZ_DECOMPRESS_FILTER_TYPE,
        .decompressNew = gzDecompressNew,
        .levelDefault = GZ_COMPRESS_LEVEL_DEFAULT,
        .levelMin = GZ_COMPRESS_LEVEL_MIN,
        .levelMax = GZ_COMPRESS_LEVEL_MAX,
    },
    {
        .typeId = STRID6("lz4", 0x2068c1),
        .type = STRDEF(LZ4_EXT),
        .ext = STRDEF("." LZ4_EXT),
        .compressType = LZ4_COMPRESS_FILTER_TYPE,
        .compressNew = lz4CompressNew,
        .decompressType = LZ4_DECOMPRESS_FILTER_TYPE,
        .decompressNew = lz4DecompressNew,
        .levelDefault = LZ4_COMPRESS_LEVEL_DEFAULT,
        .levelMin = LZ4_COMPRESS_LEVEL_MIN,
        .levelMax = LZ4_COMPRESS_LEVEL_MAX,
    },
    {
        .typeId = STRID5("zst", 0x527a0),
        .type = STRDEF(ZST_EXT),
        .ext = STRDEF("." ZST_EXT),
#ifdef HAVE_LIBZST
        .compressType = ZST_COMPRESS_FILTER_TYPE,
        .compressNew = zstCompressNew,
        .decompressType = ZST_DECOMPRESS_FILTER_TYPE,
        .decompressNew = zstDecompressNew,
        .levelDefault = ZST_COMPRESS_LEVEL_DEFAULT,
        .levelMin = ZST_COMPRESS_LEVEL_MIN,
        .levelMax = ZST_COMPRESS_LEVEL_MAX,
#endif
    },
    {
        .typeId = STRID5("xz", 0x3580),
        .type = STRDEF(XZ_EXT),
        .ext = STRDEF("." XZ_EXT),
    },
};

/**********************************************************************************************************************************/
FN_EXTERN CompressType
compressTypeEnum(const StringId type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, type);
    FUNCTION_TEST_END();

    ASSERT(type != 0);

    CompressType result = compressTypeNone;

    for (; result < LENGTH_OF(compressHelperLocal); result++)
    {
        if (type == compressHelperLocal[result].typeId)
            break;
    }

    if (result == LENGTH_OF(compressHelperLocal))
        THROW_FMT(AssertError, "invalid compression type '%s'", strZ(strIdToStr(type)));

    FUNCTION_TEST_RETURN(ENUM, result);
}

/**********************************************************************************************************************************/
static void
compressTypePresent(const CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < LENGTH_OF(compressHelperLocal));

    if (type != compressTypeNone && compressHelperLocal[type].compressNew == NULL)
        THROW_FMT(OptionInvalidValueError, PROJECT_NAME " not built with %s support", strZ(compressHelperLocal[type].type));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
compressTypeStr(const CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < LENGTH_OF(compressHelperLocal));

    FUNCTION_TEST_RETURN_CONST(STRING, compressHelperLocal[type].type);
}

/**********************************************************************************************************************************/
FN_EXTERN CompressType
compressTypeFromName(const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    CompressType result = compressTypeNone + 1;

    for (; result < LENGTH_OF(compressHelperLocal); result++)
    {
        if (strEndsWith(name, compressHelperLocal[result].ext))
            break;
    }

    if (result == LENGTH_OF(compressHelperLocal))
        result = compressTypeNone;

    FUNCTION_TEST_RETURN(ENUM, result);
}

/**********************************************************************************************************************************/
FN_EXTERN int
compressLevelDefault(const CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < LENGTH_OF(compressHelperLocal));
    compressTypePresent(type);

    FUNCTION_TEST_RETURN(INT, compressHelperLocal[type].levelDefault);
}

/**********************************************************************************************************************************/
FN_EXTERN int
compressLevelMin(const CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < LENGTH_OF(compressHelperLocal));
    compressTypePresent(type);

    FUNCTION_TEST_RETURN(INT, compressHelperLocal[type].levelMin);
}

/**********************************************************************************************************************************/
FN_EXTERN int
compressLevelMax(const CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < LENGTH_OF(compressHelperLocal));
    compressTypePresent(type);

    FUNCTION_TEST_RETURN(INT, compressHelperLocal[type].levelMax);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
compressFilter(const CompressType type, const int level, const CompressFilterParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(INT, level);
        FUNCTION_TEST_PARAM(BOOL, param.raw);
    FUNCTION_TEST_END();

    ASSERT(type < LENGTH_OF(compressHelperLocal));
    ASSERT(type != compressTypeNone);
    compressTypePresent(type);

    FUNCTION_TEST_RETURN(IO_FILTER, compressHelperLocal[type].compressNew(level, param.raw));
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
compressFilterPack(const StringId filterType, const Pack *const filterParam)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING_ID, filterType);
        FUNCTION_LOG_PARAM(VARIANT_LIST, filterParam);
    FUNCTION_LOG_END();

    ASSERT(filterType != 0);

    IoFilter *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        for (CompressType compressIdx = compressTypeNone + 1; compressIdx < LENGTH_OF(compressHelperLocal); compressIdx++)
        {
            const struct CompressHelperLocal *compress = &compressHelperLocal[compressIdx];

            if (filterType == compress->compressType)
            {
                ASSERT(filterParam != NULL);

                PackRead *const paramRead = pckReadNew(filterParam);
                const int level = pckReadI32P(paramRead);
                const bool raw = pckReadBoolP(paramRead);

                result = ioFilterMove(compress->compressNew(level, raw), memContextPrior());
                break;
            }
            else if (filterType == compress->decompressType)
            {
                result = ioFilterMove(compress->decompressNew(pckReadBoolP(pckReadNew(filterParam))), memContextPrior());
                break;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(IO_FILTER, result);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
decompressFilter(const CompressType type, const DecompressFilterParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(BOOL, param.raw);
    FUNCTION_TEST_END();

    ASSERT(type < LENGTH_OF(compressHelperLocal));
    ASSERT(type != compressTypeNone);
    compressTypePresent(type);

    FUNCTION_TEST_RETURN(IO_FILTER, compressHelperLocal[type].decompressNew(param.raw));
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
compressExtStr(const CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < LENGTH_OF(compressHelperLocal));

    FUNCTION_TEST_RETURN_CONST(STRING, compressHelperLocal[type].ext);
}

/**********************************************************************************************************************************/
FN_EXTERN void
compressExtCat(String *const file, const CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, file);
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(file != NULL);

    strCat(file, compressExtStr(type));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN String *
compressExtStrip(const String *const file, const CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, file);
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(file != NULL);

    if (!strEndsWith(file, compressExtStr(type)))
        THROW_FMT(FormatError, "'%s' must have '%s' extension", strZ(file), strZ(compressExtStr(type)));

    FUNCTION_TEST_RETURN(STRING, strSubN(file, 0, strSize(file) - strSize(compressExtStr(type))));
}
