/***********************************************************************************************************************************
ZST Decompress
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBZST

#include <zstd.h>

#include "common/compress/zst/common.h"
#include "common/compress/zst/decompress.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct ZstDecompress
{
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
static void
zstDecompressToLog(const ZstDecompress *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog, "{inputSame: %s, inputOffset: %zu, frameDone %s, done: %s}", cvtBoolToConstZ(this->inputSame), this->inputOffset,
        cvtBoolToConstZ(this->frameDone), cvtBoolToConstZ(this->done));
}

#define FUNCTION_LOG_ZST_DECOMPRESS_TYPE                                                                                           \
    ZstDecompress *
#define FUNCTION_LOG_ZST_DECOMPRESS_FORMAT(value, buffer, bufferSize)                                                              \
    FUNCTION_LOG_OBJECT_FORMAT(value, zstDecompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free decompression context
***********************************************************************************************************************************/
static void
zstDecompressFreeResource(THIS_VOID)
{
    THIS(ZstDecompress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ZST_DECOMPRESS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    ZSTD_freeDStream(this->context);

    FUNCTION_LOG_RETURN_VOID();
}

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

    // When there is no more input then decompression is done
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

        // Perform decompression. Track frame done so we can detect unexpected EOF.
        this->frameDone = zstError(ZSTD_decompressStream(this->context, &out, &in)) == 0;
        bufUsedInc(decompressed, out.pos);

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

    FUNCTION_TEST_RETURN(BOOL, this->done);
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

    FUNCTION_TEST_RETURN(BOOL, this->inputSame);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
zstDecompressNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(ZstDecompress, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        ZstDecompress *const driver = OBJ_NAME(OBJ_NEW_ALLOC(), IoFilter::ZstDecompress);

        *driver = (ZstDecompress)
        {
            .context = ZSTD_createDStream(),
        };

        // Set callback to ensure zst context is freed
        memContextCallbackSet(objMemContext(driver), zstDecompressFreeResource, driver);

        // Initialize context
        zstError(ZSTD_initDStream(driver->context));

        // Create filter interface
        this = ioFilterNewP(
            ZST_DECOMPRESS_FILTER_TYPE, driver, NULL, .done = zstDecompressDone, .inOut = zstDecompressProcess,
            .inputSame = zstDecompressInputSame);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

#endif // HAVE_LIBZST
