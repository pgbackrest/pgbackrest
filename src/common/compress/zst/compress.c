/***********************************************************************************************************************************
ZST Compress
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBZST

#include <zstd.h>

#include "common/compress/zst/common.h"
#include "common/compress/zst/compress.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(ZST_COMPRESS_FILTER_TYPE_STR,                         ZST_COMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define ZST_COMPRESS_TYPE                                           ZstCompress
#define ZST_COMPRESS_PREFIX                                         zstCompress

typedef struct ZstCompress
{
    MemContext *memContext;                                         // Context to store data
    ZSTD_CStream *context;                                          // Compression context
    int level;                                                      // Compression level
    IoFilter *filter;                                               // Filter interface

    bool inputSame;                                                 // Is the same input required on the next process call?
    size_t inputOffset;                                             // Current offset in input buffer
    bool flushing;                                                  // Is input complete and flushing in progress?
} ZstCompress;

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
static String *
zstCompressToLog(const ZstCompress *this)
{
    return strNewFmt(
        "{level: %d, inputSame: %s, inputOffset: %zu, flushing: %s}", this->level, cvtBoolToConstZ(this->inputSame),
        this->inputOffset, cvtBoolToConstZ(this->flushing));
}

#define FUNCTION_LOG_ZST_COMPRESS_TYPE                                                                                             \
    ZstCompress *
#define FUNCTION_LOG_ZST_COMPRESS_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, zstCompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free compression context
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(ZST_COMPRESS, LOG, logLevelTrace)
{
    ZSTD_freeCStream(this->context);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
static void
zstCompressProcess(THIS_VOID, const Buffer *uncompressed, Buffer *compressed)
{
    THIS(ZstCompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ZST_COMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, uncompressed);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!(this->flushing && !this->inputSame));
    ASSERT(this->context != NULL);
    ASSERT(compressed != NULL);
    ASSERT(!this->flushing || uncompressed == NULL);

    // Initialize output buffer
    ZSTD_outBuffer out = {.dst = bufRemainsPtr(compressed), .size = bufRemains(compressed)};

    // If input is NULL then start flushing
    if (uncompressed == NULL)
    {
        this->flushing = true;
        this->inputSame = zstError(ZSTD_endStream(this->context, &out)) != 0;
    }
    // Else still have input data
    else
    {
        // Initialize input buffer
        ZSTD_inBuffer in =
        {
            .src = bufPtrConst(uncompressed) + this->inputOffset,
            .size = bufUsed(uncompressed) - this->inputOffset,
        };

        // Perform compression
        zstError(ZSTD_compressStream(this->context, &out, &in));

        // If the input buffer was not entirely consumed then set inputSame and store the offset where processing will restart
        if (in.pos < in.size)
        {
            // Output buffer should be completely full
            ASSERT(out.pos == out.size);

            this->inputSame = true;
            this->inputOffset += in.pos;
        }
        // Else ready for more input
        else
        {
            this->inputSame = false;
            this->inputOffset = 0;
        }
    }

    bufUsedInc(compressed, out.pos);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is compress done?
***********************************************************************************************************************************/
static bool
zstCompressDone(const THIS_VOID)
{
    THIS(const ZstCompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ZST_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->flushing && !this->inputSame);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
zstCompressInputSame(const THIS_VOID)
{
    THIS(const ZstCompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ZST_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/**********************************************************************************************************************************/
IoFilter *
zstCompressNew(int level)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, level);
    FUNCTION_LOG_END();

    ASSERT(level >= 0);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("ZstCompress")
    {
        ZstCompress *driver = memNew(sizeof(ZstCompress));

        *driver = (ZstCompress)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .context = ZSTD_createCStream(),
            .level = level,
        };

        // Set callback to ensure zst context is freed
        memContextCallbackSet(driver->memContext, zstCompressFreeResource, driver);

        // Initialize context
        zstError(ZSTD_initCStream(driver->context, driver->level));

        // Create param list
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewInt(level));

        // Create filter interface
        this = ioFilterNewP(
            ZST_COMPRESS_FILTER_TYPE_STR, driver, paramList, .done = zstCompressDone, .inOut = zstCompressProcess,
            .inputSame = zstCompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

#endif // HAVE_LIBZST
