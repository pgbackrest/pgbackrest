/***********************************************************************************************************************************
LZ4 Compress

Developed against version r131 using the documentation in https://github.com/lz4/lz4/blob/r131/lib/lz4frame.h.
***********************************************************************************************************************************/
#include "build.auto.h"

#include <lz4frame.h>
#include <stdio.h>
#include <string.h>

#include "common/compress/common.h"
#include "common/compress/lz4/common.h"
#include "common/compress/lz4/compress.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Older versions of lz4 do not define the max header size. This seems to be the max for any version.
***********************************************************************************************************************************/
#ifndef LZ4F_HEADER_SIZE_MAX
#define LZ4F_HEADER_SIZE_MAX                                        19
#endif

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct Lz4Compress
{
    LZ4F_compressionContext_t context;                              // LZ4 compression context
    LZ4F_preferences_t prefs;                                       // Preferences -- just compress level set
    IoFilter *filter;                                               // Filter interface

    Buffer *buffer;                                                 // For when the output buffer can't accept all compressed data
    bool first;                                                     // Is this the first call to process?
    bool inputSame;                                                 // Is the same input required on the next process call?
    bool flushing;                                                  // Is input complete and flushing in progress?
} Lz4Compress;

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
static void
lz4CompressToLog(const Lz4Compress *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog, "{level: %d, first: %s, inputSame: %s, flushing: %s}", this->prefs.compressionLevel,
        cvtBoolToConstZ(this->first), cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->flushing));
}

#define FUNCTION_LOG_LZ4_COMPRESS_TYPE                                                                                             \
    Lz4Compress *
#define FUNCTION_LOG_LZ4_COMPRESS_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_OBJECT_FORMAT(value, lz4CompressToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free compression context
***********************************************************************************************************************************/
static void
lz4CompressFreeResource(THIS_VOID)
{
    THIS(Lz4Compress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(LZ4_COMPRESS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    LZ4F_freeCompressionContext(this->context);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Compress data
***********************************************************************************************************************************/
// Helper to return a buffer where output will be written. If there is enough space in the provided output buffer then use it,
// otherwise allocate an internal buffer to hold the compressed data. Once we start using the internal buffer we'll need to continue
// using it until it is completely flushed.
static Buffer *
lz4CompressBuffer(Lz4Compress *const this, const size_t required, Buffer *const output)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
        FUNCTION_TEST_PARAM(SIZE, required);
        FUNCTION_TEST_PARAM(BUFFER, output);
    FUNCTION_TEST_END();

    Buffer *result = output;

    // Is an internal buffer required to hold the compressed data?
    if (!bufEmpty(this->buffer) || required >= bufRemains(output))
    {
        // Reallocate buffer if it is not large enough
        if (required >= bufRemains(this->buffer))
            bufResize(this->buffer, bufUsed(this->buffer) + required);

        result = this->buffer;
    }

    FUNCTION_TEST_RETURN(BUFFER, result);
}

// Helper to flush output data to compressed buffer
static void
lz4CompressFlush(Lz4Compress *const this, Buffer *const output, Buffer *const compressed)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
        FUNCTION_TEST_PARAM(BUFFER, output);
        FUNCTION_TEST_PARAM(BUFFER, compressed);
    FUNCTION_TEST_END();

    // If the compressed buffer can hold all the output
    if (bufRemains(compressed) >= bufUsed(output))
    {
        bufCat(compressed, output);
        bufUsedZero(output);

        this->inputSame = false;
    }
    // Else flush as much as possible and set inputSame to flush more once the compressed buffer has been emptied
    else
    {
        size_t catSize = bufRemains(compressed);
        bufCatSub(compressed, output, 0, catSize);

        memmove(bufPtr(output), bufPtr(output) + catSize, bufUsed(output) - catSize);
        bufUsedSet(output, bufUsed(output) - catSize);

        this->inputSame = true;
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
lz4CompressProcess(THIS_VOID, const Buffer *const uncompressed, Buffer *const compressed)
{
    THIS(Lz4Compress);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(LZ4_COMPRESS, this);
        FUNCTION_LOG_PARAM(BUFFER, uncompressed);
        FUNCTION_LOG_PARAM(BUFFER, compressed);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!(this->flushing && !this->inputSame));
    ASSERT(this->context != NULL);
    ASSERT(compressed != NULL);
    ASSERT(!this->flushing || uncompressed == NULL);

    // Flush overflow output to the compressed buffer
    if (this->inputSame)
    {
        lz4CompressFlush(this, this->buffer, compressed);
    }
    else
    {
        Buffer *output = NULL;

        // If first call to process then begin compression
        if (this->first)
        {
            output = lz4CompressBuffer(this, LZ4F_HEADER_SIZE_MAX, compressed);
            bufUsedInc(
                output, lz4Error(LZ4F_compressBegin(this->context, bufRemainsPtr(output), bufRemains(output), &this->prefs)));

            this->first = false;
        }

        // Normal processing call
        if (uncompressed != NULL)
        {
            output = lz4CompressBuffer(this, lz4Error(LZ4F_compressBound(bufUsed(uncompressed), &this->prefs)), compressed);

            bufUsedInc(
                output,
                lz4Error(
                    LZ4F_compressUpdate(
                        this->context, bufRemainsPtr(output), bufRemains(output), bufPtrConst(uncompressed), bufUsed(uncompressed),
                        NULL)));
        }
        // Else flush remaining output
        else
        {
            // Pass 1 as src size to help allocate enough space for the final flush. This is required for some versions that don't
            // allocate enough memory unless autoFlush is enabled. Other versions fail if autoFlush is only enabled before the final
            // flush. This will hopefully work across all versions even if it does allocate a larger buffer than needed.
            output = lz4CompressBuffer(this, lz4Error(LZ4F_compressBound(1, &this->prefs)), compressed);
            bufUsedInc(output, lz4Error(LZ4F_compressEnd(this->context, bufRemainsPtr(output), bufRemains(output), NULL)));

            this->flushing = true;
        }

        // If the output buffer was allocated locally it will need to be flushed to the compressed buffer
        if (output != compressed)
            lz4CompressFlush(this, output, compressed);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is compress done?
***********************************************************************************************************************************/
static bool
lz4CompressDone(const THIS_VOID)
{
    THIS(const Lz4Compress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->flushing && !this->inputSame);
}

/***********************************************************************************************************************************
Is the same input required on the next process call?
***********************************************************************************************************************************/
static bool
lz4CompressInputSame(const THIS_VOID)
{
    THIS(const Lz4Compress);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LZ4_COMPRESS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->inputSame);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
lz4CompressNew(const int level, const bool raw)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, level);
        FUNCTION_LOG_PARAM(BOOL, raw);
    FUNCTION_LOG_END();

    ASSERT(level >= LZ4_COMPRESS_LEVEL_MIN && level <= LZ4_COMPRESS_LEVEL_MAX);

    OBJ_NEW_BEGIN(Lz4Compress, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (Lz4Compress)
        {
            .prefs =
            {
                .compressionLevel = level,
                .frameInfo = {.contentChecksumFlag = raw ? LZ4F_noContentChecksum : LZ4F_contentChecksumEnabled},
            },
            .first = true,
            .buffer = bufNew(0),
        };

        // Create lz4 context
        lz4Error(LZ4F_createCompressionContext(&this->context, LZ4F_VERSION));

        // Set callback to ensure lz4 context is freed
        memContextCallbackSet(objMemContext(this), lz4CompressFreeResource, this);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER,
        ioFilterNewP(
            LZ4_COMPRESS_FILTER_TYPE, this, compressParamList(level, raw), .done = lz4CompressDone, .inOut = lz4CompressProcess,
            .inputSame = lz4CompressInputSame));
}
