/***********************************************************************************************************************************
Compression Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/compress/helper.h"
#include "common/compress/bz2/common.h"
#include "common/compress/bz2/compress.h"
#include "common/compress/bz2/decompress.h"
#include "common/compress/gz/common.h"
#include "common/compress/gz/compress.h"
#include "common/compress/gz/decompress.h"
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
    IoFilter *(*compressNew)(int);                                  // Function to create new compression filter
    StringId decompressType;                                        // Type of the decompression filter
    IoFilter *(*decompressNew)(void);                               // Function to create new decompression filter
    int levelDefault;                                               // Default compression level
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
        .levelDefault = 9,
    },
    {
        .typeId = STRID5("gz", 0x3470),
        .type = STRDEF(GZ_EXT),
        .ext = STRDEF("." GZ_EXT),
        .compressType = GZ_COMPRESS_FILTER_TYPE,
        .compressNew = gzCompressNew,
        .decompressType = GZ_DECOMPRESS_FILTER_TYPE,
        .decompressNew = gzDecompressNew,
        .levelDefault = 6,
    },
    {
        .typeId = STRID6("lz4", 0x2068c1),
        .type = STRDEF(LZ4_EXT),
        .ext = STRDEF("." LZ4_EXT),
#ifdef HAVE_LIBLZ4
        .compressType = LZ4_COMPRESS_FILTER_TYPE,
        .compressNew = lz4CompressNew,
        .decompressType = LZ4_DECOMPRESS_FILTER_TYPE,
        .decompressNew = lz4DecompressNew,
        .levelDefault = 1,
#endif
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
        .levelDefault = 3,
#endif
    },
    {
        .typeId = STRID5("xz", 0x3580),
        .type = STRDEF(XZ_EXT),
        .ext = STRDEF("." XZ_EXT),
    },
};

/**********************************************************************************************************************************/
CompressType
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

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void
compressTypePresent(CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < LENGTH_OF(compressHelperLocal));

    if (type != compressTypeNone && compressHelperLocal[type].compressNew == NULL)
        THROW_FMT(OptionInvalidValueError, PROJECT_NAME " not compiled with %s support", strZ(compressHelperLocal[type].type));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
const String *
compressTypeStr(CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < LENGTH_OF(compressHelperLocal));

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

    for (; result < LENGTH_OF(compressHelperLocal); result++)
    {
        if (strEndsWith(name, compressHelperLocal[result].ext))
            break;
    }

    if (result == LENGTH_OF(compressHelperLocal))
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

    ASSERT(type < LENGTH_OF(compressHelperLocal));
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

    ASSERT(type < LENGTH_OF(compressHelperLocal));
    ASSERT(type != compressTypeNone);
    compressTypePresent(type);

    FUNCTION_TEST_RETURN(compressHelperLocal[type].compressNew(level));
}

/**********************************************************************************************************************************/
IoFilter *
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

                result = ioFilterMove(compress->compressNew(pckReadI32P(pckReadNew(filterParam))), memContextPrior());
                break;
            }
            else if (filterType == compress->decompressType)
            {
                result = ioFilterMove(compress->decompressNew(), memContextPrior());
                break;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(IO_FILTER, result);
}

/**********************************************************************************************************************************/
IoFilter *
decompressFilter(CompressType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(type < LENGTH_OF(compressHelperLocal));
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

    ASSERT(type < LENGTH_OF(compressHelperLocal));

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

    strCat(file, compressExtStr(type));

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
        THROW_FMT(FormatError, "'%s' must have '%s' extension", strZ(file), strZ(compressExtStr(type)));

    FUNCTION_TEST_RETURN(strSubN(file, 0, strSize(file) - strSize(compressExtStr(type))));
}
