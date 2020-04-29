/***********************************************************************************************************************************
ZST Decompress
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBZST

#include <zstd.h>

#include "common/compress/zst/common.h"
#include "common/compress/zst/decompress.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(ZST_DECOMPRESS_FILTER_TYPE_STR,                       ZST_DECOMPRESS_FILTER_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define ZST_DECOMPRESS_TYPE                                         ZstDecompress
#define ZST_DECOMPRESS_PREFIX                                       zstDecompress

typedef struct ZstDecompress
{
    MemContext *memContext;                                         // Context to store data
    ZSTD_DStream *context;                                          // Decompression context
    IoFilter *filter;                                               // Filter interface

    bool inputSame;                                                 // Is the same input required on the next process call?
    size_t inputOffset;                                             // Current offset in input buffer
    bool frameDone;                                                 // Has the current frame completed?
    bool done;                                                      // Is decompression done?
} ZstDecompress;

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
static String *
zstDecompressToLog(const ZstDecompress *this)
{
    return strNewFmt(
        "{inputSame: %s, inputOffset: %zu, frameDone %s, done: %s}", cvtBoolToConstZ(this->inputSame), this->inputOffset,
        cvtBoolToConstZ(this->frameDone), cvtBoolToConstZ(this->done));
}

#define FUNCTION_LOG_ZST_DECOMPRESS_TYPE                                                                                           \
    ZstDecompress *
#define FUNCTION_LOG_ZST_DECOMPRESS_FORMAT(value, buffer, bufferSize)                                                              \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, zstDecompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free decompression context
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(ZST_DECOMPRESS, LOG, logLevelTrace)
{
    ZSTD_freeDStream(this->context);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Decompress data
***********************************************************************************************************************************/
static void
zstDecompressProcess(THIS_VOID, const Buffer *compressed, Buffer *decompressed)
{
    THIS(ZstDecompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ZST_DECOMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
        FUNCTION_LOG_PARAM(BUFFER, decompressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->context != NULL);
    ASSERT(decompressed != NULL);

    if (compressed == NULL)
    {
        // If the current frame being decompressed was not completed then error
        if (!this->frameDone)
            THROW(FormatError, "unexpected eof in compressed data");

        this->done = true;
    }
    else
    {
        // Initialize input/output buffer
        ZSTD_inBuffer in = {.src = bufPtrConst(compressed) + this->inputOffset, .size = bufUsed(compressed) - this->inputOffset};
        ZSTD_outBuffer out = {.dst = bufRemainsPtr(decompressed), .size = bufRemains(decompressed)};

        this->frameDone = zstError(ZSTD_decompressStream(this->context, &out, &in)) == 0;
        bufUsedInc(decompressed, out.pos);

        if (in.pos < in.size)
        {
            ASSERT(out.pos == out.size);

            this->inputSame = true;
            this->inputOffset += in.pos;
        }
        else
        {
            this->inputOffset = 0;
            this->inputSame = false;
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is decompress done?
***********************************************************************************************************************************/
static bool
zstDecompressDone(const THIS_VOID)
{
    THIS(const ZstDecompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ZST_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->done);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
zstDecompressInputSame(const THIS_VOID)
{
    THIS(const ZstDecompress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ZST_DECOMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->inputSame);
}

/**********************************************************************************************************************************/
IoFilter *
zstDecompressNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("ZstDecompress")
    {
        ZstDecompress *driver = memNew(sizeof(ZstDecompress));

        *driver = (ZstDecompress)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .context = ZSTD_createDStream(),
        };

        // Set callback to ensure zst context is freed
        memContextCallbackSet(driver->memContext, zstDecompressFreeResource, driver);

        // Initialize context
        zstError(ZSTD_initDStream(driver->context));

        // Create filter interface
        this = ioFilterNewP(
            ZST_DECOMPRESS_FILTER_TYPE_STR, driver, NULL, .done = zstDecompressDone, .inOut = zstDecompressProcess,
            .inputSame = zstDecompressInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

#endif // HAVE_LIBZST
